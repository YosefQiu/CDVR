struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32
};

struct Uniforms {
    minValue: f32,
    maxValue: f32,
    gridWidth: f32,
    gridHeight: f32,
    numPoints: u32,
    searchRadius: f32,
    padding: vec2<f32>,
};

struct KNNResult {
    indices: array<u32, 3>,
    distances: array<f32, 3>,
    count: u32,
};

@group(0) @binding(0) var outputTexture: texture_storage_2d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
@group(1) @binding(0) var inputTF: texture_2d<f32>;

fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}

// 寻找K个最近邻
fn findKNN(dataPos: vec2<f32>, k: u32) -> KNNResult {
    var result: KNNResult;
    result.count = 0u;  // 修复：使用u32字面量
    
    // 初始化距离为最大值
    for (var i = 0u; i < 3u; i++) {
        result.distances[i] = uniforms.searchRadius;
        result.indices[i] = 0u;
    }
    
    // 遍历所有点，维护k个最近的点
    for (var i = 0u; i < uniforms.numPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distance(dataPos, pointPos);
        
        if (dist < uniforms.searchRadius) {
            // 插入排序，保持距离数组有序
            var insertPos = k;
            for (var j = 0u; j < k; j++) {
                if (dist < result.distances[j]) {
                    insertPos = j;
                    break;
                }
            }
            
            if (insertPos < k && insertPos < 3u) {  // 修复：添加数组边界检查
                // 后移元素为新元素腾出空间
                for (var j = k - 1u; j > insertPos; j--) {
                    if (j > 0u && j < 3u && (j - 1u) < 3u) {  // 修复：添加边界检查
                        result.distances[j] = result.distances[j - 1u];
                        result.indices[j] = result.indices[j - 1u];
                    }
                }
                
                // 插入新元素
                result.distances[insertPos] = dist;
                result.indices[insertPos] = i;
                
                if (result.count < k) {
                    result.count++;
                }
            }
        }
    }
    
    return result;
}

// 方法1: 最近邻插值
fn nearestNeighborInterpolation(dataPos: vec2<f32>) -> f32 {
    var minDist = uniforms.searchRadius;
    var nearestValue = 0.0;
    var found = false;
    
    for (var i = 0u; i < uniforms.numPoints; i++) {
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
    return -1.0; // 表示未找到
}

// 方法2: KNN平均插值
fn knnAverageInterpolation(dataPos: vec2<f32>) -> f32 {
    let knn = findKNN(dataPos, 3u);
    
    if (knn.count == 0u) {
        return -1.0; // 表示未找到
    }
    
    var sum = 0.0;
    // 修复：使用展开循环来处理动态索引
    if (knn.count >= 1u) {
        sum += sparsePoints[knn.indices[0]].value;
    }
    if (knn.count >= 2u) {
        sum += sparsePoints[knn.indices[1]].value;
    }
    if (knn.count >= 3u) {
        sum += sparsePoints[knn.indices[2]].value;
    }
    
    return sum / f32(knn.count);
}

// 方法3: KNN质心插值（反距离加权）
fn knnCentroidInterpolation(dataPos: vec2<f32>) -> f32 {
    let knn = findKNN(dataPos, 3u);
    
    if (knn.count == 0u) {
        return -1.0; // 表示未找到
    }
    
    // 如果有点距离为0（重合），直接返回该点的值
    if (knn.count >= 1u && knn.distances[0] < 0.0001) {
        return sparsePoints[knn.indices[0]].value;
    }
    if (knn.count >= 2u && knn.distances[1] < 0.0001) {
        return sparsePoints[knn.indices[1]].value;
    }
    if (knn.count >= 3u && knn.distances[2] < 0.0001) {
        return sparsePoints[knn.indices[2]].value;
    }
    
    // 反距离加权插值
    var weightedSum = 0.0;
    var weightSum = 0.0;
    
    // 修复：使用展开循环来处理动态索引
    if (knn.count >= 1u) {
        let weight = 1.0 / (knn.distances[0] * knn.distances[0]);
        weightedSum += sparsePoints[knn.indices[0]].value * weight;
        weightSum += weight;
    }
    if (knn.count >= 2u) {
        let weight = 1.0 / (knn.distances[1] * knn.distances[1]);
        weightedSum += sparsePoints[knn.indices[1]].value * weight;
        weightSum += weight;
    }
    if (knn.count >= 3u) {
        let weight = 1.0 / (knn.distances[2] * knn.distances[2]);
        weightedSum += sparsePoints[knn.indices[2]].value * weight;
        weightSum += weight;
    }
    
    return weightedSum / weightSum;
}

// 主插值函数 - 通过注释切换方法
fn interpolateValue(dataPos: vec2<f32>) -> f32 {
    // 方法选择：取消注释想要使用的方法，注释掉其他方法
    
    // 方法1: 最近邻插值
    return nearestNeighborInterpolation(dataPos);
    
    // 方法2: KNN平均插值
    // return knnAverageInterpolation(dataPos);
    
    // 方法3: KNN质心插值（反距离加权）
    // return knnCentroidInterpolation(dataPos);
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