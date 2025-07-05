struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32
};

struct Uniforms {
    // 第一组：16字节对齐的float4
    minValue: f32,
    maxValue: f32,
    gridWidth: f32,
    gridHeight: f32,
    
    // 第二组：16字节对齐的uint4  
    totalNodes: u32,
    totalPoints: u32,
    numLevels: u32,
    interpolationMethod: u32,
    
    // 第三组：16字节对齐，包含searchRadius和padding
    searchRadius: f32,
    padding1: f32,
    padding2: f32,
    padding3: f32,
};

struct GPUPoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32
};

@group(0) @binding(0) var outputTexture: texture_storage_2d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
@group(1) @binding(0) var inputTF: texture_2d<f32>;
@group(2) @binding(0) var<storage, read> kdTreePoints: array<GPUPoint>;

fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}
fn distance2D(x1: f32, y1: f32, x2: f32, y2: f32) -> f32 {
    let dx = x1 - x2;
    let dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}

fn distanceVec2(p1: vec2<f32>, p2: vec2<f32>) -> f32 {
    return distance(p1, p2);
}

struct MyKNNResult {
    indices: array<u32, 3>,
    distances: array<f32, 3>,
    values: array<f32, 3>,
    count: u32,
};


// 安全的KNN搜索函数
fn findKNN(dataPos: vec2<f32>, k: u32) -> MyKNNResult {
    var result: MyKNNResult;
    result.count = 0u;
    
    // 初始化所有数组，使用最大搜索半径
    for (var i = 0u; i < 3u; i++) {
        result.distances[i] = 3.40282347e+38;
        result.indices[i] = 0u;
        result.values[i] = 0.0;
    }
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distanceVec2(dataPos, pointPos);

        if (dist < 3.40282347e+38) {
            // 安全地插入点
            var inserted = false;
            
            // 检查是否应该插入以及插入位置
            for (var j = 0u; j < min(k, 3u); j++) {
                if (j >= result.count || dist < result.distances[j]) {
                    // 安全地后移元素
                    var moveCount = min(result.count, min(k, 3u) - 1u);
                    for (var moveIdx = moveCount; moveIdx > j; moveIdx--) {
                        if (moveIdx < 3u && moveIdx > 0u && (moveIdx - 1u) < 3u) {
                            result.distances[moveIdx] = result.distances[moveIdx - 1u];
                            result.indices[moveIdx] = result.indices[moveIdx - 1u];
                            result.values[moveIdx] = result.values[moveIdx - 1u];
                        }
                    }
                    
                    // 插入新元素
                    if (j < 3u) {
                        result.distances[j] = dist;
                        result.indices[j] = i;
                        result.values[j] = point.value;
                        inserted = true;
                    }
                    break;
                }
            }
            
            if (inserted && result.count < min(k, 3u)) {
                result.count++;
            }
        }
    }
    
    return result;
}

// 最近邻插值
fn nearestNeighborInterpolation(dataPos: vec2<f32>) -> f32 {
    var minDist = 3.40282347e+38;
    var nearestValue = 0.0;
    var found = false;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distanceVec2(dataPos, pointPos);
        
        if (dist < minDist) {
            minDist = dist;
            nearestValue = point.value;
            found = true;
        }
    }
    
    if (found) {
        return nearestValue;
    }
    return -1.0;
}

// KNN平均插值
fn knnAverageInterpolation(dataPos: vec2<f32>) -> f32 {
    let knn = findKNN(dataPos, 3u);
    
    if (knn.count == 0u) {
        return -1.0;
    }
    
    var sum = 0.0;
    // 展开循环避免动态索引
    if (knn.count >= 1u) {
        sum += knn.values[0];
    }
    if (knn.count >= 2u) {
        sum += knn.values[1];
    }
    if (knn.count >= 3u) {
        sum += knn.values[2];
    }
    
    return sum / f32(knn.count);
}

// KNN质心插值（反距离加权）
fn knnCentroidInterpolation(dataPos: vec2<f32>) -> f32 {
    let knn = findKNN(dataPos, 3u);
    
    if (knn.count == 0u) {
        return -1.0;
    }
    
    // 检查重合点
    if (knn.distances[0] < 0.0001) {
        return knn.values[0];
    }
    
    var weightedSum = 0.0;
    var weightSum = 0.0;
    
    // 展开循环避免动态索引
    if (knn.count >= 1u) {
        let weight = 1.0 / (knn.distances[0] * knn.distances[0]);
        weightedSum += knn.values[0] * weight;
        weightSum += weight;
    }
    if (knn.count >= 2u) {
        let weight = 1.0 / (knn.distances[1] * knn.distances[1]);
        weightedSum += knn.values[1] * weight;
        weightSum += weight;
    }
    if (knn.count >= 3u) {
        let weight = 1.0 / (knn.distances[2] * knn.distances[2]);
        weightedSum += knn.values[2] * weight;
        weightSum += weight;
    }
    
    return weightedSum / weightSum;
}

// KDTree的暴力最近邻搜索
fn bruteNearestKD(query: vec2<f32>) -> f32 {
    var minDist = 1e30;
    var bestValue = 0.0;
    for (var i = 0u; i < uniforms.totalNodes; i = i + 1u) {
        let p = kdTreePoints[i];
        let d = distance2D(query.x, query.y, p.x, p.y);
        if (d < minDist) {
            minDist = d;
            bestValue = p.value;
        }
    }
    return bestValue;
}

// 二进制树辅助函数（对应CPU的BinaryTree）
fn levelOf(nodeID: i32) -> i32 {
    // 对应CPU代码中的 BinaryTree::levelOf
    // int k = 63 - __builtin_clzll(nodeID + 1);
    // WGSL中没有clz函数，需要手动实现
    var temp = nodeID + 1;
    var k = 0;
    while (temp > 1) {
        temp = temp >> 1;
        k = k + 1;
    }
    return k;
}

// 编码和解码函数（替代CPU的BaseCandidateList，使用两个u32替代u64）
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
    // 对应CPU代码：(uint64_t(float_as_uint(f)) << 32) | uint32_t(i)
    var entry: EncodedEntry;
    entry.distBits = floatAsUint(dist);
    entry.pointIDBits = bitcast<u32>(pointID);
    return entry;
}

fn decodeDist2(entry: EncodedEntry) -> f32 {
    // 对应CPU代码：uint_as_float(v >> 32)
    return uintAsFloat(entry.distBits);
}

fn decodePointID(entry: EncodedEntry) -> i32 {
    // 对应CPU代码：int(uint32_t(v))
    return bitcast<i32>(entry.pointIDBits);
}

// 比较两个编码条目（用于排序）
fn compareEntries(a: EncodedEntry, b: EncodedEntry) -> bool {
    // 首先比较距离
    if (a.distBits != b.distBits) {
        return a.distBits < b.distBits;
    }
    // 如果距离相同，比较点ID
    return a.pointIDBits < b.pointIDBits;
}

// KDTree候选列表结构（对应CPU版本的FixedCandidateList）
struct FixedCandidateList {
    entry: array<EncodedEntry, 5>,  // 对应CPU的uint64_t entry[k]
    cutOffRadius2: f32,
    k: i32,
};

// 初始化候选列表（对应CPU的FixedCandidateList构造函数）
fn initCandidateList(cutOffRadius: f32, k: i32) -> FixedCandidateList {
    var list: FixedCandidateList;
    list.cutOffRadius2 = cutOffRadius * cutOffRadius;
    list.k = k;
    
    // 对应CPU代码：entry[i] = this->encode(cutOffRadius*cutOffRadius,-1);
    let initEntry = encode(list.cutOffRadius2, -1);
    for (var i = 0; i < 5; i++) {
        list.entry[i] = initEntry;
    }
    
    return list;
}

// 获取最大半径（对应CPU的maxRadius2）
fn maxRadius2(list: ptr<function, FixedCandidateList>) -> f32 {
    // 对应CPU代码：return this->decode_dist2(entry[k-1]);
    return decodeDist2((*list).entry[(*list).k - 1]);
}

// 获取距离（对应CPU的get_dist2）
fn getDist2(list: ptr<function, FixedCandidateList>, i: i32) -> f32 {
    return decodeDist2((*list).entry[i]);
}

// 获取点ID（对应CPU的get_pointID）
fn getPointID(list: ptr<function, FixedCandidateList>, i: i32) -> i32 {
    return decodePointID((*list).entry[i]);
}

// push函数（对应CPU的FixedCandidateList::push）
fn push(list: ptr<function, FixedCandidateList>, dist: f32, pointID: i32) {
    let v = encode(dist, pointID);
    
    // 对应CPU代码中的核心逻辑：
    // for (int i=0;i<k;i++) {
    //     uint64_t vmax = std::max(entry[i],v);
    //     uint64_t vmin = std::min(entry[i],v);
    //     entry[i] = vmin;
    //     v = vmax;
    // }
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

// 处理候选点（对应CPU的processCandidate）
fn processCandidate(list: ptr<function, FixedCandidateList>, candPrimID: i32, candDist2: f32) -> f32 {
    push(list, candDist2, candPrimID);
    return maxRadius2(list);
}

// 计算二维点之间的平方距离（对应CPU的sqrDistance）
fn sqrDistance2D(p1: vec2<f32>, p2: vec2<f32>) -> f32 {
    let d = p1 - p2;
    return dot(d, d);
}

// 获取坐标的特定维度值（对应CPU的get_coord）
fn getCoord(point: vec2<f32>, dim: i32) -> f32 {
    // 对应CPU代码：inline float get_coord(const float2 &v, int d) { return d ? v.y : v.x; }
    return select(point.x, point.y, dim != 0);
}

// KDTree遍历函数
fn kdTreeTraverseStackFree(
    result: ptr<function, FixedCandidateList>, 
    queryPoint: vec2<f32>, 
    N: i32,
    eps: f32
) {
    let epsErr = 1.0 + eps;
    let numDims = 2;
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
            let currPoint = vec2<f32>(currNode.x, currNode.y);
            let sqrDist = sqrDistance2D(queryPoint, currPoint);
            cullDist = processCandidate(result, curr, sqrDist);
        }
        
        // 计算当前维度：对应CPU代码中的 BinaryTree::levelOf(curr) % num_dims
        let currDim = levelOf(curr) % numDims;
        
        let currDimDist = getCoord(queryPoint, currDim) - getCoord(vec2<f32>(currNode.x, currNode.y), currDim);
        
        let currSide = select(0, 1, currDimDist > 0.0);
        let currCloseChild = 2 * curr + 1 + currSide;
        let currFarChild = 2 * curr + 2 - currSide;
        
        var next = -1;
        
        if (prev == currCloseChild) {
            // if we came from the close child, we may still have to check
            // the far side - but only if this exists, and if far half of
            // current space if even within search radius.
            if ((currFarChild < N) && (currDimDist * currDimDist * epsErr < cullDist)) {
                next = currFarChild;
            } else {
                next = parent;
            }
        } else if (prev == currFarChild) {
            // if we did come from the far child, then both children are
            // done, and we can only go up.
            next = parent;
        } else {
            if (child < N) {
                next = currCloseChild;
            } else {
                next = parent;
            }
        }
        
        if (next == -1) {
            // if (curr == 0 && from_child)
            // this can only (and will) happen if and only if we come from a
            // child, arrive at the root, and decide to go to the parent of
            // the root ... while means we're done.
            return;
        }
        
        prev = curr;
        curr = next;
    }
}

// KDTree KNN搜索函数（对应CPU的knn函数）
fn kdTreeKNNSearch(queryPoint: vec2<f32>, k: i32, searchRadius: f32) -> FixedCandidateList {
    var result = initCandidateList(searchRadius, k);
    
    // 确保有节点可以搜索
    if (uniforms.totalNodes > 0u) {
        kdTreeTraverseStackFree(&result, queryPoint, i32(uniforms.totalNodes), 0.0);
    }
    
    return result;
}

// 使用KDTree的最近邻插值
fn kdTreeNearestNeighborInterpolation(dataPos: vec2<f32>) -> f32 {
    var knnResult = kdTreeKNNSearch(dataPos, 1, uniforms.searchRadius);
    
    let pointID = getPointID(&knnResult, 0);
    if (pointID >= 0 && pointID < i32(uniforms.totalNodes)) 
    {
        let value = kdTreePoints[pointID].value;
        let val2 = bruteNearestKD(dataPos);
        let err = abs(value - val2);
        return val2;
    }
    
    return -1.0;
}

// // 使用KDTree的KNN质心插值（反距离加权）
// fn kdTreeKNNCentroidInterpolation(dataPos: vec2<f32>) -> f32 {
//     var knnResult = kdTreeKNNSearch(dataPos, 3, uniforms.searchRadius);
    
//     let firstPointID = getPointID(&knnResult, 0);
//     if (firstPointID < 0) {
//         return -1.0;
//     }
    
//     // 检查是否有重合点
//     let firstDist2 = getDist2(&knnResult, 0);
//     if (firstDist2 < 0.0001) {
//         return kdTreePoints[firstPointID].value;
//     }
    
//     var weightedSum = 0.0;
//     var weightSum = 0.0;
    
//     for (var i = 0; i < 3; i++) {
//         let pointID = getPointID(&knnResult, i);
//         if (pointID >= 0 && pointID < i32(uniforms.totalNodes)) {
//             let dist2 = getDist2(&knnResult, i);
//             if (dist2 > 0.0001) {  // 避免除零
//                 let weight = 1.0 / dist2;
//                 weightedSum += kdTreePoints[pointID].value * weight;
//                 weightSum += weight;
//             }
//         }
//     }
    
//     if (weightSum > 0.0) {
//         return weightedSum / weightSum;
//     }
    
//     return -1.0;
// }

fn kdTreeIDWWithPower(dataPos: vec2<f32>, k: i32, power: f32) -> f32 {
    var knnResult = kdTreeKNNSearch(dataPos, k, uniforms.searchRadius);
    
    let firstPointID = getPointID(&knnResult, 0);
    if (firstPointID < 0) {
        return -1.0;
    }
    
    let firstDist2 = getDist2(&knnResult, 0);
    if (firstDist2 < 0.0001) {
        return kdTreePoints[firstPointID].value;
    }
    
    var weightedSum = 0.0;
    var weightSum = 0.0;
    
    for (var i = 0; i < k; i++) {
        let pointID = getPointID(&knnResult, i);
        if (pointID >= 0 && pointID < i32(uniforms.totalNodes)) {
            let dist2 = getDist2(&knnResult, i);
            if (dist2 > 0.0001) {
                let dist = sqrt(dist2);
                let weight = 1.0 / pow(dist, power);  // 可调整的幂次
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


fn interpolateValue(dataPos: vec2<f32>) -> f32 {
    if (uniforms.interpolationMethod == 0u) {
        return kdTreeNearestNeighborInterpolation(dataPos);
    }
    else if (uniforms.interpolationMethod == 1u) {
        return kdTreeIDWWithPower(dataPos, 3, 2.0);
    }
    else if (uniforms.interpolationMethod == 2u) {
        return kdTreeIDWWithPower(dataPos, 5, 2.0);
    }
    return 0.0;
}




@compute @workgroup_size(16, 16)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let dims = textureDimensions(outputTexture);
    
    // 边界检查
    if (global_id.x >= dims.x || global_id.y >= dims.y) {
        return;
    }
    
    // 当前像素在纹理空间
    let pixelCoord = vec2<f32>(f32(global_id.x), f32(global_id.y));
    let uv = pixelCoord / vec2<f32>(f32(dims.x), f32(dims.y));
    
    // 映射到数据空间
    let dataPos = vec2<f32>(
        uv.x * uniforms.gridWidth,
        uv.y * uniforms.gridHeight
    );
    
    // 使用选定的插值方法
    let interpolatedValue = interpolateValue(dataPos);
    
    var color = vec4<f32>(1.0, 0.0, 0.0, 1.0); // 默认红色（未找到数据）
    
    if (interpolatedValue != -1.0) {
        let normalized = clamp(
            (interpolatedValue - uniforms.minValue) / (uniforms.maxValue - uniforms.minValue),
            0.0, 1.0
        );
        color = getColorFromTF(normalized);
    }

    textureStore(outputTexture, vec2<i32>(global_id.xy), color);
}