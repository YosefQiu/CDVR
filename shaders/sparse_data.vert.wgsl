struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    gridWidth: f32,
    gridHeight: f32,
    minValue: f32,
    maxValue: f32,
}



@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoords: vec2<f32>,
}

@vertex
fn vs_main(@location(0) pos: vec2<f32>, @location(1) texCoords: vec2<f32>) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4<f32>(pos, 0.0, 1.0);
    output.texCoords = texCoords;
    return output;
}