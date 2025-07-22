// volume_real_data.comp.wgsl
struct SparsePoint {
    x: f32,
    y: f32,
    z: f32,
    value: f32,
    padding1: f32,
    padding2: f32,
    padding3: f32,
    padding4: f32,
};

struct Uniforms {
    // 第一组：16字节对齐的float4
    minValue: f32,
    maxValue: f32,
    gridWidth: f32,
    gridHeight: f32,
    
    // 第二组：16字节对齐的float4 (新增gridDepth)
    gridDepth: f32,
    searchRadius: f32,
    padding1: f32,
    padding2: f32,
    
    // 第三组：16字节对齐的uint4
    totalNodes: u32,
    totalPoints: u32,
    numLevels: u32,
    interpolationMethod: u32,
};

struct GPUPoint3D {
    x: f32,
    y: f32,
    z: f32,
    value: f32,
    padding1: f32,
    padding2: f32,
    padding3: f32,
    padding4: f32,
};

@group(0) @binding(0) var outputTexture: texture_storage_3d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
@group(1) @binding(0) var inputTF: texture_2d<f32>;
@group(2) @binding(0) var<storage, read> kdTreePoints: array<GPUPoint3D>;

// ============ Transfer Function ============

fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth - 1)), 0, i32(tfWidth - 1));
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}

// ============ 3D KDTree 实现 ============
fn distance3D(x1: f32, y1: f32, z1: f32, x2: f32, y2: f32, z2: f32) -> f32 {
    let dx = x1 - x2;
    let dy = y1 - y2;
    let dz = z1 - z2;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

fn distanceVec3(p1: vec3<f32>, p2: vec3<f32>) -> f32 {
    return distance(p1, p2);
}

// 二进制树辅助函数（对应CPU的BinaryTree）
fn levelOf(nodeID: i32) -> i32 {
    var temp = nodeID + 1;
    var k = 0;
    while (temp > 1) {
        temp = temp >> 1;
        k = k + 1;
    }
    return k;
}

// 编码和解码函数（保持与2D版本一致）
struct EncodedEntry {
    distBits: u32,    // 高32位：距离
    pointIDBits: u32, // 低32位：点ID
};

fn floatAsUint(f: f32) -> u32 {
    return bitcast<u32>(f);
}

fn uintAsFloat(u: u32) -> f32 {
    return bitcast<f32>(u);
}

fn encode(dist: f32, pointID: i32) -> EncodedEntry {
    var entry: EncodedEntry;
    entry.distBits = floatAsUint(dist);
    entry.pointIDBits = bitcast<u32>(pointID);
    return entry;
}

fn decodeDist2(entry: EncodedEntry) -> f32 {
    return uintAsFloat(entry.distBits);
}

fn decodePointID(entry: EncodedEntry) -> i32 {
    return bitcast<i32>(entry.pointIDBits);
}

fn compareEntries(a: EncodedEntry, b: EncodedEntry) -> bool {
    if (a.distBits != b.distBits) {
        return a.distBits < b.distBits;
    }
    return a.pointIDBits < b.pointIDBits;
}

// 3D KDTree候选列表结构
struct FixedCandidateList3D {
    entry: array<EncodedEntry, 5>,
    cutOffRadius2: f32,
    k: i32,
};

// 初始化3D候选列表
fn initCandidateList3D(cutOffRadius: f32, k: i32) -> FixedCandidateList3D {
    var list: FixedCandidateList3D;
    list.cutOffRadius2 = cutOffRadius * cutOffRadius;
    list.k = k;
    
    let initEntry = encode(list.cutOffRadius2, -1);
    for (var i = 0; i < 5; i++) {
        list.entry[i] = initEntry;
    }
    
    return list;
}

// 获取最大半径
fn maxRadius2_3D(list: ptr<function, FixedCandidateList3D>) -> f32 {
    return decodeDist2((*list).entry[(*list).k - 1]);
}

// 获取距离
fn getDist2_3D(list: ptr<function, FixedCandidateList3D>, i: i32) -> f32 {
    return decodeDist2((*list).entry[i]);
}

// 获取点ID
fn getPointID_3D(list: ptr<function, FixedCandidateList3D>, i: i32) -> i32 {
    return decodePointID((*list).entry[i]);
}

// push函数
fn push3D(list: ptr<function, FixedCandidateList3D>, dist: f32, pointID: i32) {
    let v = encode(dist, pointID);
    
    var currentV = v;
    for (var i = 0; i < (*list).k; i++) {
        let entrySmaller = compareEntries((*list).entry[i], currentV);
        var vmax: EncodedEntry;
        var vmin: EncodedEntry;
        
        if (entrySmaller) {
            vmin = (*list).entry[i];
            vmax = currentV;
        } else {
            vmin = currentV;
            vmax = (*list).entry[i];
        }
        
        (*list).entry[i] = vmin;
        currentV = vmax;
    }
}

// 处理候选点
fn processCandidate3D(list: ptr<function, FixedCandidateList3D>, candPrimID: i32, candDist2: f32) -> f32 {
    push3D(list, candDist2, candPrimID);
    return maxRadius2_3D(list);
}

// 计算三维点之间的平方距离
fn sqrDistance3D(p1: vec3<f32>, p2: vec3<f32>) -> f32 {
    let d = p1 - p2;
    return dot(d, d);
}

// 获取3D坐标的特定维度值
fn getCoord3D(point: vec3<f32>, dim: i32) -> f32 {
    // dim: 0=x, 1=y, 2=z
    if (dim == 0) {
        return point.x;
    } else if (dim == 1) {
        return point.y;
    } else {
        return point.z;
    }
}

// 3D KDTree遍历函数
fn kdTreeTraverseStackFree3D(
    result: ptr<function, FixedCandidateList3D>, 
    queryPoint: vec3<f32>, 
    N: i32,
    eps: f32
) {
    let epsErr = 1.0 + eps;
    let numDims = 3;  // 3D的维度数
    var cullDist = uniforms.searchRadius * uniforms.searchRadius;
    
    var prev = -1;
    var curr = 0;
    
    loop {
        let parent = (curr + 1) / 2 - 1;
        
        if (curr >= N) {
            prev = curr;
            curr = parent;
            continue;
        }
        
        let currNode = kdTreePoints[curr];
        let child = 2 * curr + 1;
        let fromChild = (prev >= child);
        
        if (!fromChild) {
            let currPoint = vec3<f32>(currNode.x, currNode.y, currNode.z);
            let sqrDist = sqrDistance3D(queryPoint, currPoint);
            cullDist = processCandidate3D(result, curr, sqrDist);
        }
        
        // 计算当前维度：3D中在x,y,z之间循环
        let currDim = levelOf(curr) % numDims;
        
        let currDimDist = getCoord3D(queryPoint, currDim) - getCoord3D(vec3<f32>(currNode.x, currNode.y, currNode.z), currDim);
        
        let currSide = select(0, 1, currDimDist > 0.0);
        let currCloseChild = 2 * curr + 1 + currSide;
        let currFarChild = 2 * curr + 2 - currSide;
        
        var next = -1;
        
        if (prev == currCloseChild) {
            if ((currFarChild < N) && (currDimDist * currDimDist * epsErr < cullDist)) {
                next = currFarChild;
            } else {
                next = parent;
            }
        } else if (prev == currFarChild) {
            next = parent;
        } else {
            if (child < N) {
                next = currCloseChild;
            } else {
                next = parent;
            }
        }
        
        if (next == -1) {
            return;
        }
        
        prev = curr;
        curr = next;
    }
}

// 3D KDTree KNN搜索函数
fn kdTreeKNNSearch3D(queryPoint: vec3<f32>, k: i32, searchRadius: f32) -> FixedCandidateList3D {
    var result = initCandidateList3D(searchRadius, k);
    
    if (uniforms.totalNodes > 0u) {
        kdTreeTraverseStackFree3D(&result, queryPoint, i32(uniforms.totalNodes), 0.0);
    }
    
    return result;
}

// 使用3D KDTree的最近邻插值
fn kdTreeNearestNeighborInterpolation3D(dataPos: vec3<f32>) -> f32 {
    var knnResult = kdTreeKNNSearch3D(dataPos, 1, uniforms.searchRadius);
    
    let pointID = getPointID_3D(&knnResult, 0);
    if (pointID >= 0 && pointID < i32(uniforms.totalNodes)) {
        return kdTreePoints[pointID].value;
    }

    
    return -1.0;
}

// 3D KDTree反距离权重插值
fn kdTreeIDWWithPower3D(dataPos: vec3<f32>, k: i32, power: f32) -> f32 {
    var knnResult = kdTreeKNNSearch3D(dataPos, k, uniforms.searchRadius);
    
    let firstPointID = getPointID_3D(&knnResult, 0);
    if (firstPointID < 0) {
        return -1.0;
    }
    
    let firstDist2 = getDist2_3D(&knnResult, 0);
    if (firstDist2 < 0.0001) {
        return kdTreePoints[firstPointID].value;
    }
    
    var weightedSum = 0.0;
    var weightSum = 0.0;
    
    for (var i = 0; i < k; i++) {
        let pointID = getPointID_3D(&knnResult, i);
        if (pointID >= 0 && pointID < i32(uniforms.totalNodes)) {
            let dist2 = getDist2_3D(&knnResult, i);
            if (dist2 > 0.0001) {
                let dist = sqrt(dist2);
                let weight = 1.0 / pow(dist, power);
                weightedSum += kdTreePoints[pointID].value * weight;
                weightSum += weight;
            }
        }
    }
    
    if (weightSum > 0.0) {
        return weightedSum / weightSum;
    }
    
    return -1.0;
}

// 暴力搜索3D最近邻（用于验证）
fn bruteNearestKD3D(query: vec3<f32>) -> f32 {
    var minDist = 1e30;
    var bestValue = 0.0;
    for (var i = 0u; i < uniforms.totalNodes; i = i + 1u) {
        let p = kdTreePoints[i];
        let d = distance3D(query.x, query.y, query.z, p.x, p.y, p.z);
        if (d < minDist) {
            minDist = d;
            bestValue = p.value;
        }
    }
    return bestValue;
}

// ============ 传统的3D插值方法（备选） ============

// 最近邻插值（传统方法）
fn nearestNeighborInterpolation3D(dataPos: vec3<f32>) -> f32 {
    var minDist = 999999.0;
    var nearestValue = 0.0;
    var found = false;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec3<f32>(point.x, point.y, point.z);
        let dist = distance3D(dataPos.x, dataPos.y, dataPos.z, pointPos.x, pointPos.y, pointPos.z);
        
        if (dist < minDist) {
            minDist = dist;
            nearestValue = point.value;
            found = true;
        }
    }
    
    if (!found) {
        return -1.0;
    }
    
    return nearestValue;
}

// 反距离权重插值（传统方法）
fn inverseDistanceWeighting3D(dataPos: vec3<f32>) -> f32 {
    var weightSum = 0.0;
    var valueSum = 0.0;
    let power = 2.0;
    let minDistance = 0.001;
    var foundPoints = 0u;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec3<f32>(point.x, point.y, point.z);
        let dist = distance3D(dataPos.x, dataPos.y, dataPos.z, pointPos.x, pointPos.y, pointPos.z);
        
        if (dist <= uniforms.searchRadius) {
            foundPoints++;
            
            if (dist < minDistance) {
                return point.value;
            }
            
            let weight = 1.0 / pow(dist, power);
            weightSum += weight;
            valueSum += weight * point.value;
        }
    }
    
    if (foundPoints == 0u || weightSum == 0.0) {
        return -1.0;
    }
    
    return valueSum / weightSum;
}

// 主插值函数
fn interpolateValue(dataPos: vec3<f32>) -> f32 {
    return nearestNeighborInterpolation3D(dataPos);
}


// ============ Main Compute Shader ============

@compute @workgroup_size(4, 4, 4)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let dims = textureDimensions(outputTexture);
    
    // 边界检查
    if (global_id.x >= dims.x || global_id.y >= dims.y || global_id.z >= dims.z) {
        return;
    }
    
    // 当前像素在纹理空间
    let pixelCoord = vec3<f32>(f32(global_id.x), f32(global_id.y), f32(global_id.z));
    let uvw = pixelCoord / vec3<f32>(f32(dims.x), f32(dims.y), f32(dims.z));
    
    // 映射到数据空间
    let dataPos = vec3<f32>(
        uvw.x * uniforms.gridWidth,
        uvw.y * uniforms.gridHeight,
        uvw.z * uniforms.gridDepth
    );
    
    // 使用真实数据插值
    let interpolatedValue = interpolateValue(dataPos);

    var color = vec4<f32>(1.0, 1.0, 1.0, 1.0); 
    if (interpolatedValue != -1.0) {
        // 标准化值到[0,1]
        var epsilon = 10.0 / 256.0;

        let normalized = clamp(
            (interpolatedValue - (-1.0)) / (1.0 - (-1.0)),
            0.0 + epsilon, 1.0 - epsilon
        );
        
        color = getColorFromTF(normalized);
        
    }

    textureStore(outputTexture, vec3<i32>(global_id.xyz), color);

    
}