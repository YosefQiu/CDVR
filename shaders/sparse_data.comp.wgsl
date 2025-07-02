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

struct MyKNNResult {
    indices: array<u32, 3>,
    distances: array<f32, 3>,
    values: array<f32, 3>,
    count: u32,
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


// KD-Tree 辅助函数
fn parentOf(nodeIdx: u32) -> u32 {
    if (nodeIdx == 0u) {
        return 0xFFFFFFFFu; // UINT32_MAX
    }
    return (nodeIdx - 1u) / 2u;
}

fn leftChildOf(nodeIdx: u32) -> u32 {
    return 2u * nodeIdx + 1u;
}

fn rightChildOf(nodeIdx: u32) -> u32 {
    return 2u * nodeIdx + 2u;
}

fn splitDimOf(nodeIdx: u32) -> u32 {
    var depth = 0u;
    var n = nodeIdx;
    while (n > 0u) {
        n = parentOf(n);
        depth = depth + 1u;
    }
    return depth % 2u;
}

// 插入点到 KNN 结果中
fn insertPoint(result: ptr<function, MyKNNResult>, index: u32, dist: f32, value: f32, k: u32) {
    for (var i = 0u; i < k; i = i + 1u) {
        if (i >= (*result).count || dist < (*result).distances[i]) {
            // 向后移动元素
            for (var j = min(k - 1u, (*result).count); j > i; j = j - 1u) {
                if (j < 3u && j > 0u && (j - 1u) < 3u) {
                    (*result).distances[j] = (*result).distances[j - 1u];
                    (*result).indices[j] = (*result).indices[j - 1u];
                    (*result).values[j] = (*result).values[j - 1u];
                }
            }
            
            if (i < 3u) {
                (*result).distances[i] = dist;
                (*result).indices[i] = index;
                (*result).values[i] = value;
            }
            
            if ((*result).count < k) {
                (*result).count = (*result).count + 1u;
            }
            break;
        }
    }
}

fn exactStacklessTraversal(queryX: f32, queryY: f32, k: u32, maxSearchRadius: f32) -> MyKNNResult {
    var result: MyKNNResult;
    
    
    // 初始化结果
    result.count = 0u;
    for (var i = 0u; i < 3u; i = i + 1u) {
        result.distances[i] = 3.40282347e+38; // FLT_MAX
    }
    
    if (uniforms.totalNodes == 0u) {
        return result;
    }
    
    let N = uniforms.totalNodes;
    
    // current node: start at root
    var curr = 0;
    // previous node, initialize to "parent" of root node  
    var prev = -1;
    var currentMaxSearchRadius = maxSearchRadius;
    
    // Safety counter to prevent infinite loops
    var iterationCount = 0u;
    let maxIterations = uniforms.totalNodes * 4u; // Conservative upper bound
    
    // repeat until we break out:
    while (iterationCount < maxIterations) {
        iterationCount = iterationCount + 1u;
        
        var parent = -1;
        if (curr >= 0) {
            parent = (curr + 1) / 2 - 1;
        }
        
        if (curr >= i32(N)) {
            // we reached a child that does not exist; go back to parent
            prev = curr;
            curr = parent;
            continue;
        }
        
        if (curr < 0) {
            // Invalid current node, break
            break;
        }
        
        let from_parent = (prev < curr);
        
        if (from_parent) {
            // processNode(curr);
            let currentPoint = kdTreePoints[curr];
            let dist = distance2D(queryX, queryY, currentPoint.x, currentPoint.y);
            
            if (dist <= currentMaxSearchRadius) {
                insertPoint(&result, u32(curr), dist, currentPoint.value, k);
                
                // check if processing current node has led to a smaller search radius:
                if (result.count >= k) {
                    currentMaxSearchRadius = min(currentMaxSearchRadius, result.distances[k - 1u]);
                }
            }
        }
        
        // compute close and far child:
        let splitDim = splitDimOf(u32(curr));
        let currentPoint = kdTreePoints[curr];
        let splitPos = select(currentPoint.y, currentPoint.x, splitDim == 0u);
        let signedDist = select(queryY, queryX, splitDim == 0u) - splitPos;
        
        let closeSide = select(0, 1, signedDist > 0.0);
        let closeChild = 2 * curr + 1 + closeSide;
        let farChild = 2 * curr + 2 - closeSide;
        
        let farInRange = (abs(signedDist) <= currentMaxSearchRadius);
        
        // compute next node to step to
        var next = -1;
        if (from_parent) {
            next = closeChild;
        } else if (prev == closeChild) {
            next = select(parent, farChild, farInRange);
        } else {
            next = parent;
        }
        
        if (next == -1) {
            // the only way this can happen is if the entire tree under
            // node number 0 (i.e., the entire tree) is done traversing,
            // and the root node tries to step to its parent... in
            // which case we have traversed the entire tree and are done.
            break;
        }
        
        // aaaand... do the step
        prev = curr;
        curr = next;
    }
    
    return result;
}

// KD-Tree KNN质心插值（使用精确的无栈遍历）
fn kdTreeKnnCentroidInterpolationExact(dataPos: vec2<f32>) -> f32 {
    let knnResult = exactStacklessTraversal(dataPos.x, dataPos.y, 3u, 3.40282347e+38);
    
    if (knnResult.count == 0u) {
        return -1.0;
    }
    
    // 检查重合点
    if (knnResult.distances[0] < 0.0001) {
        return knnResult.values[0];
    }
    
    var weightedSum = 0.0;
    var weightSum = 0.0;
    
    // 展开循环避免动态索引
    if (knnResult.count >= 1u) {
        let weight = 1.0 / (knnResult.distances[0] * knnResult.distances[0]);
        weightedSum += knnResult.values[0] * weight;
        weightSum += weight;
    }
    if (knnResult.count >= 2u) {
        let weight = 1.0 / (knnResult.distances[1] * knnResult.distances[1]);
        weightedSum += knnResult.values[1] * weight;
        weightSum += weight;
    }
    if (knnResult.count >= 3u) {
        let weight = 1.0 / (knnResult.distances[2] * knnResult.distances[2]);
        weightedSum += knnResult.values[2] * weight;
        weightSum += weight;
    }
    
    return weightedSum / weightSum;
}

// KD-Tree最近邻插值（使用精确的无栈遍历）  
fn kdTreeNearestNeighborInterpolationExact(dataPos: vec2<f32>) -> f32 {
    let nnResult = exactStacklessTraversal(dataPos.x, dataPos.y, 1u, 3.40282347e+38);
    if (nnResult.count > 0u) {
        return nnResult.values[0];
    }
    return -1.0;
}

// 简化版KD-Tree测试 - 先确保基本搜索能工作

// 最简单的KD-Tree遍历：遍历所有节点（不做任何剪枝）



fn interpolateValue(dataPos: vec2<f32>) -> f32 {
    // 方法选择：取消注释想要使用的方法，注释掉其他方法

    // 方法1: 最近邻插值
    let res1 = nearestNeighborInterpolation(dataPos);

    // 方法2: KNN平均插值
    // return knnAverageInterpolation(dataPos);
    
    // 方法3: KNN质心插值（反距离加权）
    // return knnCentroidInterpolation(dataPos);

    let res2 = kdTreeNearestNeighborInterpolationExact(dataPos);
    
    return abs(res1 - res2);
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

    color = vec4<f32>(interpolatedValue, interpolatedValue, interpolatedValue, 1.0);
    
    textureStore(outputTexture, vec2<i32>(global_id.xy), color);
}