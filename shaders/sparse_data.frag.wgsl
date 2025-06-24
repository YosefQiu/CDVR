struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    gridWidth: f32,
    gridHeight: f32,
    minValue: f32,
    maxValue: f32,
}

struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> sparsePoints: array<SparsePoint>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoords: vec2<f32>,
}


// 最近邻插值
fn findNearestValue(coord: vec2<f32>) -> f32 {
    let numPoints = arrayLength(&sparsePoints);
    var minDist = 999999.0;
    var nearestValue = 0.0;
    
    for (var i = 0u; i < numPoints; i = i + 1u) {
        let point = sparsePoints[i];
        let dist = distance(coord, vec2<f32>(point.x, point.y));
        if (dist < minDist) {
            minDist = dist;
            nearestValue = point.value;
        }
    }
    
    return nearestValue;
}

// 颜色映射函数 (coolwarm colormap)
fn coolwarmColormap(t: f32) -> vec3<f32> {
    let t_clamped = clamp(t, 0.0, 1.0);
    
    // 简化的coolwarm颜色映射
    if (t_clamped < 0.5) {
        // 蓝色到白色
        let s = t_clamped * 2.0;
        return mix(vec3<f32>(0.0, 0.0, 1.0), vec3<f32>(1.0, 1.0, 1.0), s);
    } else {
        // 白色到红色
        let s = (t_clamped - 0.5) * 2.0;
        return mix(vec3<f32>(1.0, 1.0, 1.0), vec3<f32>(1.0, 0.0, 0.0), s);
    }
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    // 将纹理坐标转换为网格坐标
    let gridCoord = vec2<f32>(
        input.texCoords.x * uniforms.gridWidth,
        input.texCoords.y * uniforms.gridHeight
    );
    
    // 最近邻插值
    let value = findNearestValue(gridCoord);
    
    // 归一化到[0,1]
    let normalized = (value - uniforms.minValue) / 
                     (uniforms.maxValue - uniforms.minValue);
    
    // 应用颜色映射
    let color = coolwarmColormap(normalized);
    
    return vec4<f32>(color, 1.0);
}
