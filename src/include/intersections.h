#pragma once

class Material {
public:
    glm::vec3 albedo;
    bool isEmissive;
    bool isMetal;
    bool isRefractive;
    float emissivity;
    float roughness;

    Material() {
        albedo = glm::vec3(0.0f);
        isEmissive = false;
        emissivity = 0.0f;
        isMetal = false;
        isRefractive = false;
        roughness = 0.5f;
    }
};

class HitData {
public:
    glm::vec3 hitPos;
    glm::vec3 hitNormal;
    Material material;
    float t;
    bool rayHit;

    HitData() {
        hitPos = glm::vec3(0.0f);
        hitNormal = glm::vec3(0.0f);
        material = Material();
        t = std::numeric_limits<float>::max(); // absurdly large value
        rayHit = false;
    }
};

void IntersectSphere(HitData& hitData, const Material& material, const glm::vec3& sphereOrigin, const float& radius, const glm::vec3& rayOrigin, const glm::vec3& direction) {
    // Analytical Sphere Intersection
    glm::vec3 originsDelta = sphereOrigin - rayOrigin;
    float tca = glm::dot(originsDelta, direction);
    float d2 = glm::dot(originsDelta, originsDelta) - tca * tca;
    if (d2 > radius * radius) return; 

    float thc = sqrt(radius * radius - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;

    // Intersections are behind the ray origin
    if (t0 < 0.0f && t1 < 0.0f) return;

    float t = (t0 < t1) ? t0 : t1;
          t = (t0 < 0.0f) ? t1 : t;

    hitData.hitPos = rayOrigin + direction * t;
    hitData.hitNormal = glm::normalize(hitData.hitPos - sphereOrigin);
    hitData.t = t;
    hitData.rayHit = true;
    hitData.material = material;
}

void IntersectPlane(HitData& hitData, const Material& material, const glm::vec3& planeOrigin, const glm::vec3& planeNormal, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
    // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection.html
    // Equation 5
    hitData.hitNormal = planeNormal;
    float denom = glm::dot(hitData.hitNormal, rayDirection);

    if (std::abs(denom) < 1e-6) return;

    glm::vec3 originsDelta = planeOrigin - rayOrigin;
    hitData.t = glm::dot(originsDelta, hitData.hitNormal) / denom;

    if (hitData.t >= 0) {
        hitData.hitPos = hitData.t * rayDirection + rayOrigin;
        hitData.rayHit = true;
        hitData.material = material;
    }
}

void IntersectDisk(
    HitData& hitData, 
    const Material& material, 
    const glm::vec3& origin,
    const float& radius, 
    const glm::vec3& normal, 
    const glm::vec3& rayOrigin, 
    const glm::vec3& rayDirection
) {
    IntersectPlane(hitData, material, origin, normal, rayOrigin, rayDirection);

    if (hitData.rayHit) {
        float dist = glm::distance(hitData.hitPos, origin);
        hitData.rayHit = dist < radius ? true : false;
    }
}

bool CompareHitData(const HitData& a, const HitData& b) {
    return a.t < b.t;
}

HitData SortIntersections(std::vector<HitData>& hitData) {
    std::sort(hitData.begin(), hitData.end(), CompareHitData);

    HitData closestHit{};
    bool anyRayHit = false;

    for (const auto& hit : hitData) {
        if (hit.rayHit) {
            closestHit = hit;
            anyRayHit = true;
            break;
        }
    }

    // Anyhit
    closestHit.rayHit = anyRayHit;

    return closestHit;
}

void RayTraceScene(HitData& hitData, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const int recursion = 0) {
    if (recursion > MAX_RECURSIONS) return;
    std::vector<HitData> hitDataTotal{};

    Material planeMaterial{};
    planeMaterial.albedo = glm::vec3(1.0f, 1.0f, 1.0f);
    planeMaterial.emissivity = 0.0f;
    planeMaterial.isEmissive = false;

    Material emitterMaterial{};
    emitterMaterial.albedo = glm::vec3(1.0f, 1.0f, 1.0f);
    emitterMaterial.emissivity = 5.0f;
    emitterMaterial.isEmissive = true;

    HitData hitDataTemp;
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(0.0f, 1.0f, 0.0f);
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(1.0f);
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(1.0f);
    IntersectSphere(hitDataTemp, planeMaterial, glm::vec3(-0.5f, -0.66f, 0.5f), 0.33f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(1.0f);
    planeMaterial.isMetal = true;
    IntersectSphere(hitDataTemp, planeMaterial, glm::vec3(0.00f, -0.66f, 0.0f), 0.33f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    planeMaterial.albedo = glm::vec3(1.0f);
    planeMaterial.isMetal = false;
    IntersectSphere(hitDataTemp, planeMaterial, glm::vec3(0.5f, -0.66f, 0.5f), 0.33f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    IntersectDisk(hitDataTemp, emitterMaterial, glm::vec3(0.0f, 0.99f, 0.0f), 0.5f, glm::vec3(0.0f, -1.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);
    
    // Choose the closest intersection
    // using std::sort might not be the fastest solution. needs investigation
    hitData = SortIntersections(hitDataTotal);

    // Hacky way to add reflections. It would be more intuitive to add reflections via 
    if (hitData.material.isMetal) {
        RayTraceScene(hitData, hitData.hitPos + hitData.hitNormal * glm::vec3(0.001f), glm::reflect(rayDirection, hitData.hitNormal), recursion + 1);
    }
}