struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    gridWidth: f32,
    gridHeight: f32,
    minValue: f32,
    maxValue: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var dataTexture: texture_2d<f32>;
@group(0) @binding(2) var dataSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoords: vec2<f32>,
};

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
    return textureSample(dataTexture, dataSampler, uv);
}
