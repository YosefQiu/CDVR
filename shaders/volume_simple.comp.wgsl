// // volume_perlin.comp.wgsl
// struct SparsePoint {
//     x: f32,
//     y: f32,
//     z: f32,
//     value: f32,
//     padding: f32
// };

// struct Uniforms {
//     // 第一组：16字节对齐的float4
//     minValue: f32,
//     maxValue: f32,
//     gridWidth: f32,
//     gridHeight: f32,
    
//     // 第二组：16字节对齐的float4 (新增gridDepth)
//     gridDepth: f32,
//     searchRadius: f32,
//     padding1: f32,
//     padding2: f32,
    
//     // 第三组：16字节对齐的uint4
//     totalNodes: u32,
//     totalPoints: u32,
//     numLevels: u32,
//     interpolationMethod: u32,
// };

// struct GPUPoint {
//     x: f32,
//     y: f32,
//     value: f32,
//     padding: f32
// };

// @group(0) @binding(0) var outputTexture: texture_storage_3d<rgba16float, write>;
// @group(0) @binding(1) var<uniform> uniforms: Uniforms;
// @group(0) @binding(2) var<storage, read> sparsePoints: array<SparsePoint>;
// @group(1) @binding(0) var inputTF: texture_2d<f32>;
// @group(2) @binding(0) var<storage, read> kdTreePoints: array<GPUPoint>;

// // ============ Perlin Noise Implementation ============

// fn hash(p: vec3<f32>) -> f32 {
//     var p3 = fract(p * 0.1031);
//     p3 += dot(p3, p3.zyx + 31.32);
//     return fract((p3.x + p3.y) * p3.z);
// }

// fn hash3(p: vec3<f32>) -> vec3<f32> {
//     var p3 = fract(p * vec3<f32>(0.1031, 0.1030, 0.0973));
//     p3 += dot(p3, p3.yxz + 33.33);
//     return fract((p3.xxy + p3.yxx) * p3.zyx);
// }

// fn noise(p: vec3<f32>) -> f32 {
//     let i = floor(p);
//     let f = fract(p);
    
//     // 三次插值函数
//     let u = f * f * (3.0 - 2.0 * f);
    
//     // 8个角的值
//     let c000 = hash(i + vec3<f32>(0.0, 0.0, 0.0));
//     let c100 = hash(i + vec3<f32>(1.0, 0.0, 0.0));
//     let c010 = hash(i + vec3<f32>(0.0, 1.0, 0.0));
//     let c110 = hash(i + vec3<f32>(1.0, 1.0, 0.0));
//     let c001 = hash(i + vec3<f32>(0.0, 0.0, 1.0));
//     let c101 = hash(i + vec3<f32>(1.0, 0.0, 1.0));
//     let c011 = hash(i + vec3<f32>(0.0, 1.0, 1.0));
//     let c111 = hash(i + vec3<f32>(1.0, 1.0, 1.0));
    
//     // 三线性插值
//     let x00 = mix(c000, c100, u.x);
//     let x10 = mix(c010, c110, u.x);
//     let x01 = mix(c001, c101, u.x);
//     let x11 = mix(c011, c111, u.x);
    
//     let y0 = mix(x00, x10, u.y);
//     let y1 = mix(x01, x11, u.y);
    
//     return mix(y0, y1, u.z);
// }

// fn fbm(p: vec3<f32>) -> f32 {
//     var value = 0.0;
//     var amplitude = 0.5;
//     var frequency = 1.0;
//     var p_var = p;
    
//     // 8个八度的分形噪声，创建更多层次
//     for (var i = 0; i < 8; i++) {
//         value += amplitude * noise(p_var * frequency);
//         frequency *= 2.0;
//         amplitude *= 0.5;
//         p_var = p_var * 2.0 + vec3<f32>(1.3, 1.7, 2.1);
//     }
    
//     return value;
// }

// fn generatePerlinValue(pos: vec3<f32>) -> f32 {
//     // 创建多个不同尺度的噪声岛屿
//     let noisePos = pos * 0.08; // 降低频率，创建更大的结构
    
//     // 主要的形状噪声
//     let primaryNoise = fbm(noisePos);
    
//     // 添加不同尺度的细节
//     let detailNoise1 = noise(noisePos * 3.0) * 0.4;
//     let detailNoise2 = noise(noisePos * 6.0) * 0.2;
//     let detailNoise3 = noise(noisePos * 12.0) * 0.1;
    
//     // 创建扭曲效果
//     let warpX = noise(noisePos + vec3<f32>(100.0, 0.0, 0.0)) * 2.0;
//     let warpY = noise(noisePos + vec3<f32>(0.0, 100.0, 0.0)) * 2.0;
//     let warpZ = noise(noisePos + vec3<f32>(0.0, 0.0, 100.0)) * 2.0;
//     let warpedPos = noisePos + vec3<f32>(warpX, warpY, warpZ) * 0.3;
    
//     let warpedNoise = fbm(warpedPos) * 0.6;
    
//     // 组合所有噪声
//     var finalValue = primaryNoise + detailNoise1 + detailNoise2 + detailNoise3 + warpedNoise;
    
//     // 创建多个密度中心（类似多个分子或云团）
//     let center1 = vec3<f32>(4.0, 4.0, 4.0);
//     let center2 = vec3<f32>(12.0, 8.0, 6.0);
//     let center3 = vec3<f32>(8.0, 12.0, 10.0);
//     let center4 = vec3<f32>(6.0, 6.0, 12.0);
    
//     let dist1 = length(pos - center1);
//     let dist2 = length(pos - center2);
//     let dist3 = length(pos - center3);
//     let dist4 = length(pos - center4);
    
//     // 创建多个球形影响区域
//     let influence1 = exp(-dist1 * 0.3) * 0.8;
//     let influence2 = exp(-dist2 * 0.4) * 0.6;
//     let influence3 = exp(-dist3 * 0.35) * 0.7;
//     let influence4 = exp(-dist4 * 0.45) * 0.5;
    
//     let totalInfluence = max(max(influence1, influence2), max(influence3, influence4));
    
//     finalValue = finalValue * totalInfluence;
    
//     // 增加对比度，让等值面更明显
//     finalValue = pow(clamp(finalValue, 0.0, 1.0), 1.2);
    
//     return finalValue;
// }

// // ============ Transfer Function ============

// fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
//     let tfWidth = textureDimensions(inputTF).x;
//     let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
//     let texelCoord = vec2<i32>(texelX, 0);
//     return textureLoad(inputTF, texelCoord, 0);
// }

// // 如果没有传递函数纹理，使用默认的颜色映射
// fn getDefaultColor(normalizedValue: f32) -> vec4<f32> {
//     // 创建一个更丰富的颜色映射
//     let v = clamp(normalizedValue, 0.0, 1.0);
    
//     if (v < 0.1) {
//         // 深蓝色到蓝色 (低值区域)
//         let t = v * 10.0;
//         return vec4<f32>(0.0, 0.0, 0.3 + t * 0.7, 0.2 + t * 0.6);
//     } else if (v < 0.3) {
//         // 蓝色到青色
//         let t = (v - 0.1) * 5.0;
//         return vec4<f32>(0.0, t * 0.8, 1.0, 0.4 + t * 0.4);
//     } else if (v < 0.5) {
//         // 青色到绿色
//         let t = (v - 0.3) * 5.0;
//         return vec4<f32>(t * 0.3, 0.8 + t * 0.2, 1.0 - t * 0.3, 0.6 + t * 0.2);
//     } else if (v < 0.7) {
//         // 绿色到黄色
//         let t = (v - 0.5) * 5.0;
//         return vec4<f32>(0.3 + t * 0.7, 1.0, 0.7 - t * 0.7, 0.7 + t * 0.2);
//     } else if (v < 0.9) {
//         // 黄色到橙色
//         let t = (v - 0.7) * 5.0;
//         return vec4<f32>(1.0, 1.0 - t * 0.3, 0.0, 0.8 + t * 0.1);
//     } else {
//         // 橙色到红色 (高值区域)
//         let t = (v - 0.9) * 10.0;
//         return vec4<f32>(1.0, 0.7 - t * 0.5, t * 0.2, 0.9 + t * 0.1);
//     }
// }

// fn getIsosurfaceColor(normalizedValue: f32) -> vec4<f32> {
//     let v = clamp(normalizedValue, 0.0, 1.0);
    
//     // 定义多个等值面阈值，类似你图片中的效果
//     let threshold1 = 0.3;  // 低密度区域（蓝色）
//     let threshold2 = 0.5;  // 中密度区域（橙色/肉色）
//     let threshold3 = 0.7;  // 高密度区域（红色）
    
//     var color = vec4<f32>(0.0, 0.0, 0.0, 0.0);
    
//     if (v > threshold1 && v <= threshold2) {
//         // 蓝色区域（低密度）
//         let t = (v - threshold1) / (threshold2 - threshold1);
//         color = vec4<f32>(
//             0.3 + t * 0.2,      // 淡蓝到浅蓝
//             0.6 + t * 0.3,      
//             0.9 + t * 0.1,      
//             0.6 + t * 0.3       // 半透明
//         );
//     } else if (v > threshold2 && v <= threshold3) {
//         // 橙色/肉色区域（中密度）
//         let t = (v - threshold2) / (threshold3 - threshold2);
//         color = vec4<f32>(
//             0.8 + t * 0.2,      // 橙色到浅橙
//             0.6 + t * 0.2,      
//             0.4 + t * 0.1,      
//             0.7 + t * 0.2       // 更不透明
//         );
//     } else if (v > threshold3) {
//         // 红色区域（高密度）
//         let t = min((v - threshold3) / (1.0 - threshold3), 1.0);
//         color = vec4<f32>(
//             0.9 + t * 0.1,      // 深红色
//             0.3 - t * 0.1,      
//             0.2 - t * 0.1,      
//             0.8 + t * 0.2       // 最不透明
//         );
//     }
    
//     return color;
// }

// // ============ Interpolation Methods ============

// fn distance3D(x1: f32, y1: f32, z1: f32, x2: f32, y2: f32, z2: f32) -> f32 {
//     let dx = x1 - x2;
//     let dy = y1 - y2;
//     let dz = z1 - z2;
//     return sqrt(dx * dx + dy * dy + dz * dz);
// }

// fn interpolateValue(dataPos: vec3<f32>) -> f32 {
//     // 现在我们不需要插值了，直接生成Perlin噪声
//     return generatePerlinValue(dataPos);
// }

// // ============ Main Compute Shader ============

// @compute @workgroup_size(4, 4, 4)
// fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
//     let dims = textureDimensions(outputTexture);
    
//     // 边界检查
//     if (global_id.x >= dims.x || global_id.y >= dims.y || global_id.z >= dims.z) {
//         return;
//     }
    
//     // 当前像素在纹理空间
//     let pixelCoord = vec3<f32>(f32(global_id.x), f32(global_id.y), f32(global_id.z));
//     let uvw = pixelCoord / vec3<f32>(f32(dims.x), f32(dims.y), f32(dims.z));
    
//     // 映射到数据空间（16³）
//     let dataPos = vec3<f32>(
//         uvw.x * 16.0,
//         uvw.y * 16.0,
//         uvw.z * 16.0
//     );
    
//     // 生成Perlin噪声值
//     let noiseValue = interpolateValue(dataPos);
    
//     // 选择渲染方式
//     var color = vec4<f32>(0.0, 0.0, 0.0, 1.0); // 默认黑色
    
//     // 使用传递函数或默认颜色映射
//     if (textureDimensions(inputTF).x > 0) {
//         // 如果有传递函数纹理，使用它
//         color = getColorFromTF(noiseValue);
//     } else {
//         // 否则使用默认颜色映射
//         color = getDefaultColor(noiseValue);
//     }
    
//     // 根据噪声值调整透明度，创建更好的体积效果
//     color.a *= smoothstep(0.05, 0.95, noiseValue);
    
//     // 添加一些边缘淡化
//     let edgeFactor = min(
//         min(uvw.x, 1.0 - uvw.x),
//         min(min(uvw.y, 1.0 - uvw.y), min(uvw.z, 1.0 - uvw.z))
//     );
//     color.a *= smoothstep(0.0, 0.1, edgeFactor);
    
//     // 调试：在立方体的一个角落显示特殊颜色来确认着色器工作
//     if (global_id.x < 2u && global_id.y < 2u && global_id.z < 2u) {
//         // 立方体左下前角显示白色，确认着色器在运行
//         color = vec4<f32>(1.0, 1.0, 1.0, 1.0);
//     }
    
//     textureStore(outputTexture, vec3<i32>(global_id.xyz), color);
// }

// volume_simple.comp.wgsl
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


fn getColorFromTF(normalizedValue: f32) -> vec4<f32> {
    let tfWidth = textureDimensions(inputTF).x;
    let texelX = clamp(i32(normalizedValue * f32(tfWidth)), 0, i32(tfWidth) - 1);
    let texelCoord = vec2<i32>(texelX, 0);
    return textureLoad(inputTF, texelCoord, 0);
}

fn distance3D(x1: f32, y1: f32, z1: f32, x2: f32, y2: f32, z2: f32) -> f32 {
    let dx = x1 - x2;
    let dy = y1 - y2;
    let dz = z1 - z2;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

fn distanceVec3(p1: vec3<f32>, p2: vec3<f32>) -> f32 {
    return distance(p1, p2);
}

struct MyKNNResult {
    indices: array<u32, 3>,
    distances: array<f32, 3>,
    values: array<f32, 3>,  // 添加这个字段
    count: u32,
};

fn findKNN(dataPos: vec3<f32>, k: u32) -> MyKNNResult {
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
        let pointPos = vec3<f32>(point.x, point.y, point.z);
        let dist = distance3D(dataPos.x, dataPos.y, dataPos.z, pointPos.x, pointPos.y, pointPos.z);

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

fn nearestNeighborInterpolation(dataPos: vec3<f32>) -> f32 {
    var minDist = 3.40282347e+38;
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
    
    if (found) {
        return nearestValue;
    }
    return -1.0;
}


// 基于3D位置生成彩色立方体，使用Transfer Function
fn generateColorCube(dataPos: vec3<f32>) -> vec4<f32> {
    // 将3D位置标准化到[0,1]
    let normalizedPos = vec3<f32>(
        dataPos.x / uniforms.gridWidth,
        dataPos.y / uniforms.gridHeight,
        dataPos.z / uniforms.gridDepth
    );
    
    // 方法1：直接使用位置的某个分量作为值，然后通过TF映射
    let positionValue = (normalizedPos.x + normalizedPos.y + normalizedPos.z) / 3.0;
    return getColorFromTF(positionValue);
}

fn interpolateValue(dataPos: vec3<f32>) -> f32 {
    if (uniforms.interpolationMethod == 0u) {
        return nearestNeighborInterpolation(dataPos);
    }
   
    return 0.0;
}


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
        uvw.x * 16.0, // 假设数据空间宽度为16
        uvw.y * 16.0, // 假设数据空间高度为64
        uvw.z * 16.0  // 假设数据空间深度为64
    );
    let interpolatedValue = interpolateValue(dataPos);

    // 选择渲染方式
    var color = vec4<f32>(1.0, 0.0, 0.0, 1.0); // 默认红色（未找到数据）

    if (interpolatedValue != -1.0) {
        let normalized = clamp(
            (interpolatedValue - uniforms.minValue) / (uniforms.maxValue - uniforms.minValue),
            0.0, 1.0
        );
        color = getColorFromTF(normalized);
    }


    textureStore(outputTexture, vec3<i32>(global_id.xyz), color);
}