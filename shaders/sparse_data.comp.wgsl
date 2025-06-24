struct SparsePoint {
    x: f32,
    y: f32,
    value: f32,
    padding: f32,
}

struct ComputeUniforms {
    minValue: f32,
    maxValue: f32,
    gridWidth: f32,
    gridHeight: f32,
    numPoints: u32,
    searchRadius: f32,
    padding: vec2<f32>,
}

@group(0) @binding(0) var<uniform> uniforms: ComputeUniforms;
@group(0) @binding(1) var<storage, read> sparsePoints: array<SparsePoint>;
@group(0) @binding(2) var outputTexture: texture_storage_2d<rgba8unorm, write>;

// 简单的颜色映射函数
fn valueToColor(value: f32) -> vec4<f32> {
    // 归一化到 [0, 1]
    let normalized = clamp((value - uniforms.minValue) / (uniforms.maxValue - uniforms.minValue), 0.0, 1.0);
    
    // 冷暖色映射：蓝色(冷) -> 红色(热)
    let r = normalized;
    let g = 0.2;
    let b = 1.0 - normalized;
    
    return vec4<f32>(r, g, b, 1.0);
}

@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let dims = textureDimensions(outputTexture);
    
    // 边界检查
    if (global_id.x >= dims.x || global_id.y >= dims.y) {
        return;
    }
    
    // 计算当前像素在数据空间中的位置
    let pixelCoord = vec2<f32>(f32(global_id.x), f32(global_id.y));
    let uv = pixelCoord / vec2<f32>(f32(dims.x), f32(dims.y));
    
    // 转换到数据空间坐标 (0 到 gridWidth/Height)
    let dataPos = vec2<f32>(
        uv.x * uniforms.gridWidth,
        (1.0 - uv.y) * uniforms.gridHeight  // 翻转Y轴以匹配OpenGL坐标系
    );
    
    // 查找最近的稀疏点
    var minDist = uniforms.searchRadius;
    var nearestValue = 0.0;
    var found = false;
    
    // 遍历所有稀疏点（后续可以优化为空间划分）
    for (var i = 0u; i < uniforms.numPoints; i++) {
        let point = sparsePoints[i];
        let pointPos = vec2<f32>(point.x, point.y);
        let dist = distance(dataPos, pointPos);
        
        if (dist < minDist) {
            minDist = dist;
            nearestValue = point.value;
            found = true;
        }
    }
    
    // 生成颜色
    var color = vec4<f32>(0.0, 0.0, 0.0, 1.0); // 默认黑色背景
    
    if (found) {
        // 简单的距离衰减
        let weight = 1.0 - (minDist / uniforms.searchRadius);
        let interpolatedValue = nearestValue * weight;
        color = valueToColor(interpolatedValue);
    }
    
    // 写入纹理
    textureStore(outputTexture, vec2<i32>(global_id.xy), color);
}