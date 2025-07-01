struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32
};

struct KDNode {
    splitValue: f32,
    splitDim: u32,
    leftChild: u32,
    rightChild: u32,
    pointStart: u32,
    pointCount: u32,
    boundingBox: vec4<f32>,
};

struct LeafPoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32,
};

struct Uniforms {
    minValue: f32,
    maxValue: f32,
    gridWidth: f32,
    gridHeight: f32,
    rootNodeIndex: u32,
    totalNodes: u32,
    totalPoints: u32,
    searchRadius: f32,
    interpolationMethod: u32,
};

struct KNNResult {
    indices: array<u32, 3>,
    distances: array<f32, 3>,
    values: array<f32, 3>,
    count: u32,
};

@group(0) @binding(0) var outputTexture: texture_storage_2d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
@group(1) @binding(0) var inputTF: texture_2d<f32>;
@group(2) @binding(0) var<storage, read> kdNodes: array<KDNode>;
@group(2) @binding(1) var<storage, read> leafPoints: array<LeafPoint>;

fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}

fn isInBoundingBox(pos: vec2<f32>, bbox: vec4<f32>) -> bool {
    return pos.x >= bbox.x && pos.x <= bbox.z && 
           pos.y >= bbox.y && pos.y <= bbox.w;
}

fn distanceToBoundingBox(pos: vec2<f32>, bbox: vec4<f32>) -> f32 {
    let dx = max(0.0, max(bbox.x - pos.x, pos.x - bbox.z));
    let dy = max(0.0, max(bbox.y - pos.y, pos.y - bbox.w));
    return sqrt(dx * dx + dy * dy);
}

// 修复：完整的KNN搜索函数
fn findKNN(dataPos: vec2<f32>, k: u32) -> KNNResult {
    var result: KNNResult;
    result.count = 0u;
    
    // 修复：初始化所有数组
    for (var i = 0u; i < 3u; i++) {
        result.distances[i] = uniforms.searchRadius;
        result.indices[i] = 0u;
        result.values[i] = 0.0;  // 修复：添加values初始化
    }
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distance(dataPos, pointPos);
        
        if (dist < uniforms.searchRadius) {
            var insertPos = k;
            for (var j = 0u; j < k; j++) {
                if (dist < result.distances[j]) {
                    insertPos = j;
                    break;
                }
            }
            
            if (insertPos < k && insertPos < 3u) {
                // 修复：后移所有数组
                for (var j = k - 1u; j > insertPos; j--) {
                    if (j > 0u && j < 3u && (j - 1u) < 3u) {
                        result.distances[j] = result.distances[j - 1u];
                        result.indices[j] = result.indices[j - 1u];
                        result.values[j] = result.values[j - 1u];  // 修复：添加values移动
                    }
                }
                
                // 修复：插入所有字段
                result.distances[insertPos] = dist;
                result.indices[insertPos] = i;
                result.values[insertPos] = point.value;  // 修复：添加value存储
                
                if (result.count < k) {
                    result.count++;
                }
            }
        }
    }
    
    return result;
}

fn nearestNeighborInterpolation(dataPos: vec2<f32>) -> f32 {
    var minDist = uniforms.searchRadius;
    var nearestValue = 0.0;
    var found = false;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distance(dataPos, pointPos);
        
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

// 修复：使用展开循环避免动态索引
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

// 修复：使用展开循环避免动态索引
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

fn updateKNN(result: ptr<function, KNNResult>, dist: f32, value: f32, pointIdx: u32) {
    if (dist >= uniforms.searchRadius) {
        return;
    }
    
    var insertPos = (*result).count;
    for (var i = 0u; i < (*result).count && i < 3u; i++) {
        if (dist < (*result).distances[i]) {
            insertPos = i;
            break;
        }
    }
    
    if (insertPos < 3u) {
        for (var j = min((*result).count, 2u); j > insertPos; j--) {
            if (j < 3u && (j - 1u) < 3u) {
                (*result).distances[j] = (*result).distances[j - 1u];
                (*result).values[j] = (*result).values[j - 1u];
                (*result).indices[j] = (*result).indices[j - 1u];
            }
        }
        
        (*result).distances[insertPos] = dist;
        (*result).values[insertPos] = value;
        (*result).indices[insertPos] = pointIdx;
        
        if ((*result).count < 3u) {
            (*result).count++;
        }
    }
}

fn searchInLeaf(result: ptr<function, KNNResult>, pos: vec2<f32>, node: KDNode) {
    for (var i = 0u; i < node.pointCount; i++) {
        let pointIdx = node.pointStart + i;
        if (pointIdx >= uniforms.totalPoints) {
            break;
        }
        
        let point = leafPoints[pointIdx];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distance(pos, pointPos);
        
        updateKNN(result, dist, point.value, pointIdx);
    }
}

fn kdTreeKNNSearch(pos: vec2<f32>) -> KNNResult {
    var result: KNNResult;
    result.count = 0u;
    
    for (var i = 0u; i < 3u; i++) {
        result.distances[i] = uniforms.searchRadius;
        result.values[i] = 0.0;
        result.indices[i] = 0u;
    }
    
    var stack: array<u32, 16>;
    var stackTop = 0u;
    stack[0] = uniforms.rootNodeIndex;
    
    while (stackTop < 16u) {
        let nodeIdx = stack[stackTop];
        stackTop--;
        
        if (nodeIdx == 0xFFFFFFFFu || nodeIdx >= uniforms.totalNodes) {
            if (stackTop == 0u) { break; }
            continue;
        }
        
        let node = kdNodes[nodeIdx];
        
        let distToBBox = distanceToBoundingBox(pos, node.boundingBox);
        if (distToBBox > uniforms.searchRadius) {
            if (stackTop == 0u) { break; }
            continue;
        }
        
        if (node.splitDim == 0xFFFFFFFFu) {
            searchInLeaf(&result, pos, node);
        } else {
            let splitPos = pos[node.splitDim];
            let isLeftSide = splitPos < node.splitValue;
            let dist = abs(splitPos - node.splitValue);
            
            if (isLeftSide) {
                if (node.leftChild != 0xFFFFFFFFu && stackTop < 15u) {
                    stackTop++;
                    stack[stackTop] = node.leftChild;
                }
                if (dist < uniforms.searchRadius && node.rightChild != 0xFFFFFFFFu && stackTop < 15u) {
                    stackTop++;
                    stack[stackTop] = node.rightChild;
                }
            } else {
                if (node.rightChild != 0xFFFFFFFFu && stackTop < 15u) {
                    stackTop++;
                    stack[stackTop] = node.rightChild;
                }
                if (dist < uniforms.searchRadius && node.leftChild != 0xFFFFFFFFu && stackTop < 15u) {
                    stackTop++;
                    stack[stackTop] = node.leftChild;
                }
            }
        }
        
        if (stackTop == 0u) { break; }
    }
    
    return result;
}

fn kdTreeNearestNeighbor(dataPos: vec2<f32>) -> f32 {
    let knn = kdTreeKNNSearch(dataPos);
    
    if (knn.count == 0u) {
        return -1.0;
    }
    
    return knn.values[0];
}

fn kdTreeKNNAverage(dataPos: vec2<f32>) -> f32 {
    let knn = kdTreeKNNSearch(dataPos);
    
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

fn kdTreeKNNWeighted(dataPos: vec2<f32>) -> f32 {
    let knn = kdTreeKNNSearch(dataPos);
    
    if (knn.count == 0u) {
        return -1.0;
    }
    
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
// 主插值函数 - 通过注释切换方法
fn interpolateValue(dataPos: vec2<f32>) -> f32 {
    // 方法选择：取消注释想要使用的方法，注释掉其他方法
    
    // 方法1: 最近邻插值
    // return nearestNeighborInterpolation(dataPos);
    
    // 方法2: KNN平均插值
    // return knnAverageInterpolation(dataPos);
    
    // 方法3: KNN质心插值（反距离加权）
    // return knnCentroidInterpolation(dataPos);

    return kdTreeKNNWeighted(dataPos);
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


