// volume_raycasting.vert.wgsl
// Ray casting的顶点着色器

struct RS_Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>,
    cameraPos: vec3<f32>,
    rayStepSize: f32,
    volumeSize: vec3<f32>,
    volumeOpacity: f32,
}

struct VertexOutput {
    @builtin(position) clipPos: vec4<f32>,
    @location(0) worldPos: vec3<f32>,
    @location(1) localPos: vec3<f32>,
    @location(2) viewDir: vec3<f32>,
}

@group(0) @binding(0) var<uniform> uniforms: RS_Uniforms;
@group(0) @binding(1) var volumeTexture: texture_3d<f32>;
@group(0) @binding(2) var volumeSampler: sampler;
@group(0) @binding(3) var transferFunction: texture_2d<f32>;
@group(0) @binding(4) var tfSampler: sampler;

@vertex
fn main(@location(0) position: vec3<f32>) -> VertexOutput {
    var output: VertexOutput;

    // 将顶点位置从-0.5~0.5变换到模型空间
    let modelPos = uniforms.modelMatrix * vec4<f32>(position, 1.0);
    output.worldPos = modelPos.xyz;

    // 计算局部坐标（0到1范围，用于纹理采样）
    output.localPos = position + vec3<f32>(0.5, 0.5, 0.5);

    // 计算视线方向（从相机到顶点）
    output.viewDir = normalize(output.worldPos - uniforms.cameraPos);

    // 变换到裁剪空间
    output.clipPos = uniforms.projMatrix * uniforms.viewMatrix * modelPos;

    return output;
}
