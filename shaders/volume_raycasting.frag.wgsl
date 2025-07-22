// volume_raycasting.frag.wgsl
struct Uniforms {
    viewMatrix: mat4x4<f32>,
    projMatrix: mat4x4<f32>,
    modelMatrix: mat4x4<f32>,
    invViewMatrix: mat4x4<f32>,
    invProjMatrix: mat4x4<f32>,
    invModelMatrix: mat4x4<f32>,
    cameraPosition: vec3<f32>,
};

struct FragmentInput {
    @location(0) texCoord: vec3<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var inputTexture: texture_3d<f32>;
@group(0) @binding(2) var textureSampler: sampler;


// 简化逆矩阵：仅适用于旋转 + 平移（无缩放）
fn inverseViewMatrix() -> mat4x4<f32> {
    return uniforms.invViewMatrix;
}

fn inverseModelMatrix() -> mat4x4<f32> {
    return uniforms.invModelMatrix;
}

// 从视图矩阵推算 camera 世界位置
fn getCameraPosition() -> vec3<f32> {
    let invView = inverseViewMatrix();
    return vec3<f32>(invView[3].x, invView[3].y, invView[3].z);
}

// 从纹理坐标计算世界空间光线
fn calculateWorldRay(texCoord: vec2<f32>) -> vec3<f32> {
    // texCoord [0,1] -> NDC [-1,1]
    let ndc = vec2<f32>(texCoord.x * 2.0 - 1.0, (1.0 - texCoord.y) * 2.0 - 1.0);
    
    // NDC -> 投影空间 -> 视图空间 -> 世界空间
    let nearPoint = uniforms.invProjMatrix * vec4<f32>(ndc, -1.0, 1.0);
    let farPoint = uniforms.invProjMatrix * vec4<f32>(ndc, 1.0, 1.0);
    
    let nearView = nearPoint.xyz / nearPoint.w;
    let farView = farPoint.xyz / farPoint.w;
    
    let nearWorld = (uniforms.invViewMatrix * vec4<f32>(nearView, 1.0)).xyz;
    let farWorld = (uniforms.invViewMatrix * vec4<f32>(farView, 1.0)).xyz;
    
    return normalize(farWorld - nearWorld);
}

// 光线与 unit cube 相交测试
fn rayBoxIntersection(rayOrigin: vec3<f32>, rayDir: vec3<f32>) -> vec2<f32> {
    let boxMin = vec3<f32>(-0.5, -0.5, -0.5);
    let boxMax = vec3<f32>( 0.5,  0.5,  0.5);

    let invDir = 1.0 / rayDir;
    let t1 = (boxMin - rayOrigin) * invDir;
    let t2 = (boxMax - rayOrigin) * invDir;

    let tMin = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    let tMax = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));

    return vec2<f32>(tMin, tMax);
}

// alpha correction
fn alphaCorrection(alpha: f32, stepSize: f32, referenceStepSize: f32) -> f32 {
    if (alpha <= 0.0) {
        return 0.0;
    }
    let ratio = stepSize / referenceStepSize;
    return 1.0 - pow(1.0 - alpha, ratio);
}



fn advancedVolumeRender(input: FragmentInput) -> vec4<f32> {
    // 计算光线方向
    let cameraPos = getCameraPosition();
    let rayDirWorld = calculateWorldRay(input.texCoord.xy);

    // 变换光线到立方体局部空间
    let localRayOrigin = (uniforms.invModelMatrix * vec4<f32>(cameraPos, 1.0)).xyz;
    let localRayDir = normalize((uniforms.invModelMatrix * vec4<f32>(rayDirWorld, 0.0)).xyz);
    
    
    // 计算光线与立方体的交点
    let intersection = rayBoxIntersection(localRayOrigin, normalize(localRayDir));
    let tNear = intersection.x;
    let tFar = intersection.y;
    
    if (tFar <= tNear) {
        return vec4<f32>(0.0, 0.0, 0.0, 0.0);
    }
    
    // 设置采样参数
    let density = 0.5;
    let stepSize =  0.005;  // 每步采样距离
    let referenceStepSize = 0.01;  // 参考步长，用于 alpha 校正
    let maxSteps = min(i32((tFar - tNear) / stepSize) + 1, 5000);
    
    // 光照设置
    let lightDir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    
    // 累积颜色
    var accumColor = vec4<f32>(0.0);
    var t = tNear;
    
    for (var i = 0; i < maxSteps && i < 5000; i++) {
        if (accumColor.a > 0.99) {
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

        var dst :vec4<f32> = accumColor;
        var src :vec4<f32> = sampleColor * lighting;
        src.a = src.a * density;  
        src.a = alphaCorrection(src.a, stepSize, referenceStepSize);

        // Front to back
        dst.r = dst.r + (1.0 - dst.a) * src.a * src.r;
        dst.g = dst.g + (1.0 - dst.a) * src.a * src.g;
        dst.b = dst.b + (1.0 - dst.a) * src.a * src.b;
        dst.a = dst.a + (1.0 - dst.a) * src.a;

        accumColor = dst;
        
        t += stepSize;
    }
    return accumColor;
}


@fragment
fn main(input: FragmentInput) -> @location(0) vec4<f32> {
    
    return advancedVolumeRender(input);
}