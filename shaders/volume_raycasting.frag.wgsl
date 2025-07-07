// volume_raycasting.frag.wgsl
struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>,
};

struct FragmentInput {
    @location(0) worldPos: vec3<f32>,
    @location(1) texCoord: vec3<f32>,
    @location(2) localPos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var inputTexture: texture_3d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

// 获取相机位置（从view matrix的逆矩阵中提取）
fn getCameraPosition() -> vec3<f32> {
    let invView = transpose(uniforms.viewMatrix);  // 简化的逆矩阵（假设只有旋转和平移）
    return vec3<f32>(invView[3][0], invView[3][1], invView[3][2]);
}

// 计算光线与立方体的交点
fn rayBoxIntersection(rayOrigin: vec3<f32>, rayDir: vec3<f32>) -> vec2<f32> {
    let boxMin = vec3<f32>(-0.5, -0.5, -0.5);
    let boxMax = vec3<f32>(0.5, 0.5, 0.5);
    
    let invDir = 1.0 / rayDir;
    let t1 = (boxMin - rayOrigin) * invDir;
    let t2 = (boxMax - rayOrigin) * invDir;
    
    let tMin = min(t1, t2);
    let tMax = max(t1, t2);
    
    let tNear = max(max(tMin.x, tMin.y), tMin.z);
    let tFar = min(min(tMax.x, tMax.y), tMax.z);

    return vec2<f32>(max(tNear, 0.001), max(tFar, 0.001));
}


// Ray casting 体积渲染
fn raycastingVolumeRender(input: FragmentInput) -> vec4<f32> {
    // 计算光线方向
    let cameraPos = getCameraPosition();
    let rayDir = normalize(input.worldPos - cameraPos);
    
    // 将光线变换到局部坐标系
    let invModel = transpose(uniforms.modelMatrix);  // 简化的逆矩阵
    let localRayOrigin = (invModel * vec4<f32>(cameraPos, 1.0)).xyz;
    let localRayDir = (invModel * vec4<f32>(rayDir, 0.0)).xyz;
    
    // 计算光线与立方体的交点
    let intersection = rayBoxIntersection(localRayOrigin, normalize(localRayDir));
    let tNear = intersection.x;
    let tFar = intersection.y;
    
    if (tFar <= tNear) {
        return vec4<f32>(0.0, 0.0, 0.0, 0.0);  // 没有交点
    }
    
    // 设置采样参数
    let stepSize = 0.008; 
    let maxSteps = min(i32((tFar - tNear) / stepSize) + 1, 150);  // 限制最大步数
    
    // 累积颜色
    var accumColor = vec4<f32>(0.0);
    var t = tNear;
    
    for (var i = 0; i < maxSteps; i++) {
        if (accumColor.a > 0.95) {
            break;  // 提前终止
        }
        
        // 计算采样位置
        let samplePos = localRayOrigin + t * normalize(localRayDir);
        
        // 转换到纹理坐标 [0,1]
        let texCoord = samplePos + vec3<f32>(0.5);
        
        // 边界检查
        if (any(texCoord < vec3<f32>(0.0)) || any(texCoord > vec3<f32>(1.0))) {
            t += stepSize;
            continue;
        }
        
        // 采样体积数据
        let sampleColor = textureSample(inputTexture, textureSampler, texCoord);
        
        if (sampleColor.a > 0.001) {
            // 调整Alpha混合 - 关键修改
            let alpha = sampleColor.a * stepSize * 5.0;  // 降低密度系数
            let clampedAlpha = min(alpha, 0.1);  // 限制单次贡献
            let oneMinusAccumAlpha = 1.0 - accumColor.a;
                
            // Front-to-back alpha blending
            accumColor += sampleColor * clampedAlpha * oneMinusAccumAlpha;
        }
        
        t += stepSize;
        if (t > tFar) {
            break;
        }
    }
    
    return accumColor;
}

fn advancedVolumeRender(input: FragmentInput) -> vec4<f32> {
    // 计算光线方向
    let cameraPos = getCameraPosition();
    let rayDir = normalize(input.worldPos - cameraPos);
    
    // 将光线变换到局部坐标系
    let invModel = transpose(uniforms.modelMatrix);
    let localRayOrigin = (invModel * vec4<f32>(cameraPos, 1.0)).xyz;
    let localRayDir = (invModel * vec4<f32>(rayDir, 0.0)).xyz;
    
    // 计算光线与立方体的交点
    let intersection = rayBoxIntersection(localRayOrigin, normalize(localRayDir));
    let tNear = intersection.x;
    let tFar = intersection.y;
    
    if (tFar <= tNear) {
        return vec4<f32>(0.0, 0.0, 0.0, 0.0);
    }
    
    // 设置采样参数
    let stepSize = 0.001;  // 更小的步长，更好的质量
    let maxSteps = i32((tFar - tNear) / stepSize) + 1;
    
    // 光照设置
    let lightDir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    
    // 累积颜色
    var accumColor = vec4<f32>(0.0);
    var t = tNear;
    
    for (var i = 0; i < maxSteps && i < 300; i++) {
        if (accumColor.a > 0.98) {
            break;
        }
        
        // 计算采样位置
        let samplePos = localRayOrigin + t * normalize(localRayDir);
        let texCoord = samplePos + vec3<f32>(0.5);
        
        // 边界检查
        if (any(texCoord < vec3<f32>(0.02)) || any(texCoord > vec3<f32>(0.98))) {
            t += stepSize;
            continue;
        }
        
        // 采样体积数据
        let sampleColor = textureSample(inputTexture, textureSampler, texCoord);
        
        if (sampleColor.a > 0.01) {  // 只处理非透明的采样点
            // 计算梯度（简单的数值梯度）
            let eps = 0.01;
            let gradX = textureSample(inputTexture, textureSampler, texCoord + vec3<f32>(eps, 0.0, 0.0)).a
                      - textureSample(inputTexture, textureSampler, texCoord - vec3<f32>(eps, 0.0, 0.0)).a;
            let gradY = textureSample(inputTexture, textureSampler, texCoord + vec3<f32>(0.0, eps, 0.0)).a
                      - textureSample(inputTexture, textureSampler, texCoord - vec3<f32>(0.0, eps, 0.0)).a;
            let gradZ = textureSample(inputTexture, textureSampler, texCoord + vec3<f32>(0.0, 0.0, eps)).a
                      - textureSample(inputTexture, textureSampler, texCoord - vec3<f32>(0.0, 0.0, eps)).a;
            
            let normal = normalize(vec3<f32>(gradX, gradY, gradZ));
            
            // 简单的光照计算
            let diffuse = max(dot(normal, lightDir), 0.0);
            let lighting = 0.3 + 0.7 * diffuse;  // 环境光 + 漫反射
            
            // Alpha混合 - 修复：分别处理RGB和A分量
            let alpha = sampleColor.a * stepSize * 15.0;
            let oneMinusAccumAlpha = 1.0 - accumColor.a;
            let lightedColor = sampleColor * lighting;
            
            // 分别更新RGBA分量
            accumColor.x += lightedColor.x * alpha * oneMinusAccumAlpha;
            accumColor.y += lightedColor.y * alpha * oneMinusAccumAlpha;
            accumColor.z += lightedColor.z * alpha * oneMinusAccumAlpha;
            accumColor.w += alpha * oneMinusAccumAlpha;
        }
        
        t += stepSize;
    }
    
    return accumColor;
}


@fragment
fn main(input: FragmentInput) -> @location(0) vec4<f32> {
    return raycastingVolumeRender(input);
}