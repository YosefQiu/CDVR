// volume_real_data.comp.wgsl
struct SparsePoint {
    x: f32,
    y: f32,
    z: f32,
    value: f32,
    padding: f32
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

struct GPUPoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32
};

@group(0) @binding(0) var outputTexture: texture_storage_3d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: Uniforms;
@group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
@group(1) @binding(0) var inputTF: texture_2d<f32>;
@group(2) @binding(0) var<storage, read> kdTreePoints: array<GPUPoint>;

// ============ 数据插值实现 ============

fn distance3D(x1: f32, y1: f32, z1: f32, x2: f32, y2: f32, z2: f32) -> f32 {
    let dx = x1 - x2;
    let dy = y1 - y2;
    let dz = z1 - z2;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// 最近邻插值
fn nearestNeighborInterpolation(dataPos: vec3<f32>) -> f32 {
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
        return -1.0; // 表示没有找到有效数据
    }
    
    return nearestValue;
}

// 反距离权重插值 (IDW)
fn inverseDistanceWeighting(dataPos: vec3<f32>) -> f32 {
    var weightSum = 0.0;
    var valueSum = 0.0;
    let power = 2.0; // IDW的幂次，通常为2
    let minDistance = 0.001; // 避免除零
    var foundPoints = 0u;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec3<f32>(point.x, point.y, point.z);
        let dist = distance3D(dataPos.x, dataPos.y, dataPos.z, pointPos.x, pointPos.y, pointPos.z);
        
        // 只考虑搜索半径内的点
        if (dist <= uniforms.searchRadius) {
            foundPoints++;
            
            if (dist < minDistance) {
                // 如果非常接近某个点，直接返回该点的值
                return point.value;
            }
            
            let weight = 1.0 / pow(dist, power);
            weightSum += weight;
            valueSum += weight * point.value;
        }
    }
    
    if (foundPoints == 0u || weightSum == 0.0) {
        return -1.0; // 表示没有找到有效数据
    }
    
    return valueSum / weightSum;
}

// 简化的K最近邻插值（避免动态数组索引）
fn knnInterpolation(dataPos: vec3<f32>) -> f32 {
    // 存储最近的3个点
    var dist1 = 999999.0;
    var dist2 = 999999.0; 
    var dist3 = 999999.0;
    var value1 = 0.0;
    var value2 = 0.0;
    var value3 = 0.0;
    var foundCount = 0u;
    
    for (var i = 0u; i < uniforms.totalPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec3<f32>(point.x, point.y, point.z);
        let dist = distance3D(dataPos.x, dataPos.y, dataPos.z, pointPos.x, pointPos.y, pointPos.z);
        
        // 只考虑搜索半径内的点
        if (dist <= uniforms.searchRadius) {
            foundCount++;
            
            // 插入排序，保持最近的3个点
            if (dist < dist1) {
                // 新的最近点
                dist3 = dist2;
                value3 = value2;
                dist2 = dist1;
                value2 = value1;
                dist1 = dist;
                value1 = point.value;
            } else if (dist < dist2) {
                // 新的第二近点
                dist3 = dist2;
                value3 = value2;
                dist2 = dist;
                value2 = point.value;
            } else if (dist < dist3) {
                // 新的第三近点
                dist3 = dist;
                value3 = point.value;
            }
        }
    }
    
    if (foundCount == 0u) {
        return -1.0; // 没有找到邻居
    }
    
    if (dist1 < 0.001) {
        return value1; // 非常接近第一个点
    }
    
    // 使用反距离权重插值
    var weightSum = 0.0;
    var valueSum = 0.0;
    
    // 第一个点
    let weight1 = 1.0 / (dist1 * dist1);
    weightSum += weight1;
    valueSum += weight1 * value1;
    
    // 第二个点（如果存在）
    if (foundCount > 1u && dist2 < 999999.0) {
        let weight2 = 1.0 / (dist2 * dist2);
        weightSum += weight2;
        valueSum += weight2 * value2;
    }
    
    // 第三个点（如果存在）
    if (foundCount > 2u && dist3 < 999999.0) {
        let weight3 = 1.0 / (dist3 * dist3);
        weightSum += weight3;
        valueSum += weight3 * value3;
    }
    
    return valueSum / weightSum;
}

// 直接查找（如果数据是规则网格）
fn directLookup(dataPos: vec3<f32>) -> f32 {
    // 如果你的数据是规则网格，可以直接计算索引
    let x = i32(round(dataPos.x));
    let y = i32(round(dataPos.y));
    let z = i32(round(dataPos.z));
    
    // 检查边界
    if (x < 0 || y < 0 || z < 0 || 
        x >= i32(uniforms.gridWidth) || 
        y >= i32(uniforms.gridHeight) || 
        z >= i32(uniforms.gridDepth)) {
        return -1.0;
    }
    
    // 计算线性索引
    let index = u32(z) * u32(uniforms.gridWidth * uniforms.gridHeight) + 
                u32(y) * u32(uniforms.gridWidth) + u32(x);
    
    if (index >= uniforms.totalPoints) {
        return -1.0;
    }
    
    return sparsePoints[index].value;
}

// 主插值函数
fn interpolateValue(dataPos: vec3<f32>) -> f32 {
    switch uniforms.interpolationMethod {
        case 0u: {
            return nearestNeighborInterpolation(dataPos);
        }
        case 1u: {
            return inverseDistanceWeighting(dataPos);
        }
        case 2u: {
            return knnInterpolation(dataPos);
        }
        case 3u: {
            return directLookup(dataPos);
        }
        default: {
            return nearestNeighborInterpolation(dataPos);
        }
    }
}

// ============ Transfer Function ============

fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    if (textureDimensions(inputTF).x == 0) {
        return getDefaultColor(normalizedValue);
    }
    
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}

fn getDefaultColor(normalizedValue: f32) -> vec4<f32> {
    let v = clamp(normalizedValue, 0.0, 1.0);
    
    if (v < 0.2) {
        // 蓝色
        let t = v * 5.0;
        return vec4<f32>(0.0, 0.0, 0.5 + t * 0.5, 0.3 + t * 0.5);
    } else if (v < 0.4) {
        // 青色
        let t = (v - 0.2) * 5.0;
        return vec4<f32>(0.0, t * 0.8, 1.0, 0.5 + t * 0.3);
    } else if (v < 0.6) {
        // 绿色
        let t = (v - 0.4) * 5.0;
        return vec4<f32>(t * 0.5, 0.8 + t * 0.2, 1.0 - t * 0.5, 0.6 + t * 0.2);
    } else if (v < 0.8) {
        // 黄色
        let t = (v - 0.6) * 5.0;
        return vec4<f32>(0.5 + t * 0.5, 1.0, 0.5 - t * 0.5, 0.7 + t * 0.2);
    } else {
        // 红色
        let t = (v - 0.8) * 5.0;
        return vec4<f32>(1.0, 1.0 - t * 0.7, 0.0, 0.8 + t * 0.2);
    }
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
    
    var color = vec4<f32>(0.0, 0.0, 0.0, 0.0); // 默认透明
    
    if (interpolatedValue != -1.0) {
        // 标准化值到[0,1]
        let normalized = clamp(
            (interpolatedValue - uniforms.minValue) / (uniforms.maxValue - uniforms.minValue),
            0.0, 1.0
        );
        
        color = getColorFromTF(normalized);
        
        // 根据值调整透明度
        color.a *= smoothstep(0.1, 0.9, normalized);
    }
    
    // 调试：显示数据范围
    if (global_id.x < 2u && global_id.y < 2u && global_id.z < 2u) {
        // 左下前角显示白色，确认着色器在运行
        color = vec4<f32>(1.0, 1.0, 1.0, 1.0);
    }
    
    // 如果没有数据，显示一些调试信息
    if (uniforms.totalPoints == 0u) {
        // 没有数据点时显示红色
        color = vec4<f32>(1.0, 0.0, 0.0, 0.5);
    }
    
    textureStore(outputTexture, vec3<i32>(global_id.xyz), color);
}