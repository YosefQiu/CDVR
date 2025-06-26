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

@group(1) @binding(0) var transferFunction: texture_2d<float>;

// 简单的颜色映射函数
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

// fn getColorFromTransferFunction(normalizedValue: f32) -> vec4<f32> {
//     let t_clamped = clamp(normalizedValue, 0.0, 1.0);
    
//     let texWidth = 256;
//     let pixelIndex = i32(t_clamped * f32(texWidth - 1));
//     let clampedIndex = clamp(pixelIndex, 0, texWidth - 1);
    
//     // 从 Transfer Function 加载颜色
//     // textureLoad 会自动将 RGBA8Unorm (0-255) 转换为 vec4<f32> (0.0-1.0)
//     let tfColor = textureLoad(transferFunction, vec2<i32>(clampedIndex, 0), 0);
    
//     // 调试：检查转换后的值
//     // 预期：对于 (58, 76, 192, x)，应该得到约 (0.227, 0.298, 0.753, x/255)
    
//     if (tfColor.r == 0.0 && tfColor.g == 0.0 && tfColor.b == 0.0) {
//         // 仍然是全黑，表示纹理数据或绑定有问题
//         return vec4<f32>(1.0, 0.0, 0.0, 1.0);  // 红色表示错误
//     }
    
//     // 成功加载 Transfer Function 颜色
//     return vec4<f32>(tfColor.rgb, 1.0);  // 使用 RGB，忽略 Alpha
// }


@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let dims = textureDimensions(outputTexture);

    // 边界检查
    if (global_id.x >= dims.x || global_id.y >= dims.y) {
        return;
    }

    // 当前像素在纹理空间
    let pixelCoord = vec2<f32>(f32(global_id.x), f32(global_id.y));
    let uv = pixelCoord / vec2<f32>(f32(dims.x), f32(dims.y));

    // 映射到数据空间
    let dataPos = vec2<f32>(
        uv.x * uniforms.gridWidth,
        uv.y * uniforms.gridHeight
    );

    var minDist = uniforms.searchRadius;
    var nearestValue = 0.0;
    var found = false;

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

    var color = vec4<f32>(1.0, 0.0, 0.0, 1.0);

    if (found) {
        let normalized = clamp(
            (nearestValue - uniforms.minValue) / (uniforms.maxValue - uniforms.minValue),
            0.0, 1.0
        );
        color = vec4<f32>(coolwarmColormap(normalized), 1.0);
    }

    textureStore(outputTexture, vec2<i32>(global_id.xy), color);
}