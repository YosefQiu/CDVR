// volume_raycasting.vert.wgsl
struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>,
};

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) texCoord: vec3<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) worldPos: vec3<f32>,
    @location(1) texCoord: vec3<f32>,
    @location(2) localPos: vec3<f32>,  // 用于光线投射
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    
    // 保存局部坐标（用于光线投射）
    output.localPos = input.position;
    
    // 计算世界坐标
    let worldPos = uniforms.modelMatrix * vec4<f32>(input.position, 1.0);
    output.worldPos = worldPos.xyz;
    
    // 计算裁剪空间坐标
    let viewPos = uniforms.viewMatrix * worldPos;
    output.position = uniforms.projMatrix * viewPos;
    
    // 传递3D纹理坐标
    output.texCoord = input.texCoord;
    
    return output;
}