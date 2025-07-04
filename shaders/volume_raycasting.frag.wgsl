// volume_raycasting.frag.wgsl
// Ray casting的片段着色器

struct RS_Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>,
    cameraPos: vec3<f32>,
    rayStepSize: f32,
    volumeSize: vec3<f32>,
    volumeOpacity: f32,
}

@group(0) @binding(0) var<uniform> uniforms: RS_Uniforms;
@group(0) @binding(1) var volumeTexture: texture_3d<f32>;
@group(0) @binding(2) var volumeSampler: sampler;
@group(0) @binding(3) var transferFunction: texture_2d<f32>;
@group(0) @binding(4) var tfSampler: sampler;

// 射线与立方体求交函数
fn rayBoxIntersection(rayOrigin: vec3<f32>, rayDir: vec3<f32>) -> vec2<f32> {
    let boxMin = vec3<f32>(0.0, 0.0, 0.0);
    let boxMax = vec3<f32>(1.0, 1.0, 1.0);

    let invDir = 1.0 / rayDir;
    let t1 = (boxMin - rayOrigin) * invDir;
    let t2 = (boxMax - rayOrigin) * invDir;

    let tMin = min(t1, t2);
    let tMax = max(t1, t2);

    let tNear = max(max(tMin.x, tMin.y), tMin.z);
    let tFar = min(min(tMax.x, tMax.y), tMax.z);

    return vec2<f32>(max(tNear, 0.0), tFar);
}

@fragment
fn main(
    @location(0) worldPos: vec3<f32>,
    @location(1) localPos: vec3<f32>,
    @location(2) viewDir: vec3<f32>
) -> @location(0) vec4<f32> {

    // 射线起点（在体数据的局部坐标系中）
    let rayOrigin = localPos;
    let rayDir = normalize(viewDir);

    // 计算射线与体数据边界盒的交点
    let intersection = rayBoxIntersection(rayOrigin, rayDir);
    let tNear = intersection.x;
    let tFar = intersection.y;

    // 如果射线没有与边界盒相交，返回透明
    if (tNear >= tFar || tFar <= 0.0) {
        discard;
    }

    // Ray marching参数
    let stepSize = uniforms.rayStepSize;
    let maxSteps = i32((tFar - tNear) / stepSize) + 1;

    // 初始化颜色累积
    var finalColor = vec4<f32>(0.0, 0.0, 0.0, 0.0);

    // 射线行进
    for (var i = 0; i < maxSteps; i = i + 1) {
        let t = tNear + f32(i) * stepSize;
        if (t > tFar) {
            break;
        }

        // 当前采样位置
        let samplePos = rayOrigin + rayDir * t;

        // 检查是否在体数据范围内
        if (any(samplePos < vec3<f32>(0.0)) || any(samplePos > vec3<f32>(1.0))) {
            continue;
        }

        // 从体纹理中采样
        let volumeSample = textureSample(volumeTexture, volumeSampler, samplePos);
        let density = volumeSample.a;  // 密度存储在alpha通道

        // 如果密度很小，跳过这个采样点
        if (density < 0.01) {
            continue;
        }

        // 从传输函数中获取颜色
        let tfColor = textureSample(transferFunction, tfSampler, vec2<f32>(density, 0.5));

        // 应用体积不透明度
        var sampleColor = vec4<f32>(tfColor.rgb, tfColor.a * uniforms.volumeOpacity * density);

        // 前向混合（front-to-back blending）
        sampleColor.a = sampleColor.a * (1.0 - finalColor.a);
        finalColor = finalColor + sampleColor;

        // 早期射线终止：如果不透明度接近1，停止采样
        if (finalColor.a > 0.99) {
            break;
        }
    }

    // 确保alpha值在合理范围内
    finalColor.a = clamp(finalColor.a, 0.0, 1.0);

    return finalColor;
}
