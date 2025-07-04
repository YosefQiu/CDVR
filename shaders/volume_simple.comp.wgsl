// volume_simple.comp.wgsl
// 简单的计算着色器：将传输函数纹理直接拷贝到3D体纹理中

struct CS_Uniforms {
    gridWidth: f32,
    gridHeight: f32,
    gridDepth: f32,
    padding1: f32,
}

@group(0) @binding(0) var volumeTexture: texture_storage_3d<rgba16float, write>;
@group(0) @binding(1) var<uniform> uniforms: CS_Uniforms;
@group(0) @binding(2) var transferFunction: texture_2d<f32>;

@compute @workgroup_size(8, 8, 8)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let dims = vec3<u32>(u32(uniforms.gridWidth), u32(uniforms.gridHeight), u32(uniforms.gridDepth));

    // 检查边界
    if (global_id.x >= dims.x || global_id.y >= dims.y || global_id.z >= dims.z) {
        return;
    }

    // 将3D坐标转换为0-1范围的纹理坐标
    let texCoord = vec3<f32>(global_id) / vec3<f32>(dims);

    // 简单的测试模式：根据位置生成密度值
    // 创建一个球形渐变
    let center = vec3<f32>(0.5, 0.5, 0.5);
    let distance = length(texCoord - center);
    let radius = 0.3;

    var density: f32;
    if (distance < radius) {
        // 在球内，密度从中心向外递减
        density = 1.0 - (distance / radius);
    } else {
        // 球外，密度为0
        density = 0.0;
    }

    // 从传输函数纹理中采样颜色
    // 使用密度值作为x坐标，y坐标固定为0.5
    let tfCoord = vec2<f32>(density, 0.5);
    let tfValue = textureLoad(transferFunction, vec2<i32>(i32(tfCoord.x * 255.0), 0), 0);

    // 写入3D纹理
    // 我们将密度存储在alpha通道，颜色存储在rgb通道
    let outputColor = vec4<f32>(tfValue.rgb, density);

    textureStore(volumeTexture, global_id, outputColor);
}
