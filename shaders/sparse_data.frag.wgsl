@group(0) @binding(1) var dataTexture: texture_2d<f32>;
@group(0) @binding(2) var dataSampler: sampler;

struct FragmentInput {
    @location(0) uv: vec2<f32>,
}
        
@fragment
fn main(input: FragmentInput) -> @location(0) vec4<f32> {
    let sampledColor = textureSample(dataTexture, dataSampler, input.uv);
    return sampledColor;  // 显示实际采样结果
}