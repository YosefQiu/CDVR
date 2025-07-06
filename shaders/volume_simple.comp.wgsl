// volume_simple.comp.wgsl
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
    
    // 方法2：使用距离中心的距离作为值
    // let centerDistance = length(normalizedPos - vec3<f32>(0.5, 0.5, 0.5));
    // let normalizedDistance = clamp(centerDistance / 0.866, 0.0, 1.0); // 0.866 = sqrt(3)/2
    // return getColorFromTF(normalizedDistance);
    
    // 方法3：使用正弦波模式
    // let waveValue = sin(normalizedPos.x * 6.28318) * sin(normalizedPos.y * 6.28318) * sin(normalizedPos.z * 6.28318);
    // let normalizedWave = waveValue * 0.5 + 0.5;
    // return getColorFromTF(normalizedWave);
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
        uvw.x * uniforms.gridWidth,
        uvw.y * uniforms.gridHeight,
        uvw.z * uniforms.gridDepth
    );
    
    // 选择渲染方式
    var color: vec4<f32>;

    color = generateColorCube(dataPos);

    textureStore(outputTexture, vec3<i32>(global_id.xyz), color);
}