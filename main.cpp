#include <iostream>
#include <cmath>
#include <array>
#include <chrono>
#include <algorithm>
#include <vector>
#include <random>

// STB for image writes
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// GLM since I don't want to implement MAD myself lol suck it arduino
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// A bunch of defines
#define VIEWPORT_WIDTH 512
#define VIEWPORT_HEIGHT 512

const float PI = 3.141592f;
const float TAU = 2.0f * PI;
const float HPI = PI * 0.5f;
const float rPI = 1.0f / PI;
const float rTAU = 1.0f / TAU;
const float PHI = std::sqrt(5.0f) * 0.5f + 0.5f;

class CameraTransforms {
public:
    glm::mat4 projectionMatrix;
    glm::mat4 projectionMatrixInverse;
    glm::mat4 viewMatrix;
    glm::mat4 viewMatrixInverse;

    CameraTransforms(const float& fov) {
        projectionMatrix = glm::perspective(glm::radians(fov), (float) VIEWPORT_WIDTH / (float) VIEWPORT_HEIGHT, 0.1f, 100.0f);
        projectionMatrixInverse = glm::inverse(projectionMatrix);
        viewMatrix = glm::rotate(glm::mat4(1.0f), 3.141592f / 16.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        viewMatrix = viewMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        viewMatrixInverse = glm::inverse(viewMatrix);
    }

    // TODO: Animation support
    void onNewFrame(const std::size_t& index) {
        viewMatrix = glm::rotate(glm::mat4(1.0f), 3.141592f / 8.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        viewMatrix = viewMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(std::sin(index / 60.0f) * 0.1, 0.0f, 0.0f));
        viewMatrixInverse = glm::inverse(viewMatrix);
    }
};

class Material {
public:
    glm::vec3 albedo;
    bool isEmissive;
    float emissivity;

    Material() {
        albedo = glm::vec3(0.0f);
        isEmissive = false;
        emissivity = 0.0f;
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

std::size_t Map2Dto1D(const std::size_t& x, const std::size_t& y) {
    return x + y * VIEWPORT_WIDTH;
}

void PrintVec4(const glm::vec4& vec) {
    std::cout << "X: " << vec.x << " Y: " << vec.y << " Z: " << vec.z << " W: " << vec.w << "\n";
}

glm::vec3 Reinhard(const glm::vec3& linearValue) {
    return linearValue / (linearValue + glm::vec3(1.0f));
}

glm::vec3 ACESFilm(const glm::vec3& x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return glm::clamp((x*(a*x+b))/(x*(c*x+d)+e), glm::vec3(0.0f), glm::vec3(1.0f));
}

glm::vec3 LinearToSrgb(const glm::vec3& linearValue) {
    return glm::pow(linearValue, glm::vec3(1.0 / 2.2));
}

glm::vec3 UniformSampleHemisphere(glm::vec3 normal, glm::vec2 rand){
    // cos(theta) = r1 = y
    // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = sqrt(1 - cos^2(theta))
    float sinTheta = std::sqrt(1.0 - rand.x * rand.x);
    float phi = 2.0 * PI * rand.y;
    float x = sinTheta * std::cos(phi);
    float z = sinTheta * std::sin(phi);
    
    // Transform the local hemisphere direction to world space aligned with the normal
    glm::vec3 tangent, bitangent;
    if (glm::abs(normal.x) > 0.1) {
        tangent = glm::normalize(glm::cross(normal, glm::vec3(0.0, 1.0, 0.0)));
    } else {
        tangent = glm::normalize(glm::cross(normal, glm::vec3(1.0, 0.0, 0.0)));
    }
    bitangent = normalize(cross(normal, tangent));

    glm::vec3 hemisphereDir = tangent * x + normal * rand.x + bitangent * z;
    return normalize(hemisphereDir);
}

void IntersectSphere(HitData& hitData, const Material& material, const glm::vec3& sphereOrigin, const float& radius, const glm::vec3& rayOrigin, const glm::vec3& direction) {
    // Analytical Sphere Intersection
    glm::vec3 originsDelta = sphereOrigin - rayOrigin;
    float tca = glm::dot(originsDelta, direction);
    float d2 = glm::dot(originsDelta, originsDelta) - tca * tca;
    if (d2 > radius * radius) return; 

    float thc = sqrt(radius * radius - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;

    // Choose the closest intersection point
    float t = (t0 < t1) ? t0 : t1;
    if (t < 0) return; // Intersection point is behind the ray's origin

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

void RayTraceScene(HitData& hitData, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
    std::vector<HitData> hitDataTotal{};

    Material material{};
    material.albedo = glm::vec3(0.1f, 0.5f, 0.1f);
    material.emissivity = 0.0f;
    material.isEmissive = false;

    Material sphereMaterial{};
    material.albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    material.emissivity = 1.0f;
    material.isEmissive = true;

    HitData hitDataTemp;
    IntersectPlane(hitDataTemp, material, glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitData = HitData();
    IntersectPlane(hitDataTemp, material, glm::vec3(0.0f, -0.5f, -7.0f), glm::vec3(0.0f, 0.0f, 1.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitData = HitData();
    IntersectSphere(hitDataTemp, sphereMaterial, glm::vec3(0.0f, 0.5, -3.5f), 0.75f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);
    
    // Choose the closer intersection
    // using std::sort might not be the fastest solution. needs investigation.
    hitData = SortIntersections(hitDataTotal);
}

glm::vec3 CalculatePointLightContribution(const glm::vec3& illuminance, const glm::vec3& worldPos, const glm::vec3& pointLightPos, const glm::vec3& normal) {
    glm::vec3 rayDirection = glm::normalize(pointLightPos - worldPos);
    
    float nDotL = glm::dot(normal, rayDirection);
          nDotL = glm::clamp(nDotL, 0.0f, 1.0f);
    float attenuation = 1.0f / std::pow(glm::distance(worldPos, pointLightPos), 2.0f);

    HitData hitData{};

    // Shadow ray
    RayTraceScene(hitData, worldPos + normal * glm::vec3(0.001f), rayDirection);

    return illuminance * nDotL * attenuation * (float) !hitData.rayHit;
}

glm::vec3 Render(const std::size_t& x, const std::size_t& y, const CameraTransforms& cameraTransforms, const std::size_t& frameIdx) {
    glm::vec3 sceneColor{0.0f};

    glm::vec2 nCoord = glm::vec2{(float) x / VIEWPORT_WIDTH, (float) y / VIEWPORT_HEIGHT};
    glm::vec3 screenPos = glm::vec3(nCoord, -1.0f);
    glm::vec4 clipPos = glm::vec4(screenPos * glm::vec3(2.0f) - glm::vec3(1.0f), 1.0f);
    glm::vec4 viewPos = cameraTransforms.projectionMatrixInverse * clipPos;
              viewPos = glm::vec4(viewPos.x / viewPos.w, viewPos.y / viewPos.w, viewPos.z / viewPos.w, 1.0f);
    glm::vec4 worldPos = cameraTransforms.viewMatrixInverse * viewPos;

    glm::vec3 worldVector = glm::normalize(glm::vec3(worldPos.x, worldPos.y, worldPos.z));
    glm::vec3 rayOrigin = glm::vec3(0.0f, 1.0f, -0.2f);
    
    glm::vec2 rand = glm::vec2(1.0);

    HitData hitData{};

    // Camera Ray
    RayTraceScene(hitData, rayOrigin, worldVector);
    sceneColor = hitData.material.albedo;

    glm::vec3 rayDir = UniformSampleHemisphere(hitData.hitNormal, rand);

    //if (hitData.rayHit && !hitData.material.isEmissive) {
        
    //}

    // return glm::length(hitData.hitPos) * glm::vec3(0.01f);
    return sceneColor;
}

int main() {
    stbi_flip_vertically_on_write(1);

    static std::array<unsigned char, VIEWPORT_HEIGHT * VIEWPORT_WIDTH * 3> frameBuffer{};
    CameraTransforms cameraTransforms(70.0f);

    auto start = std::chrono::high_resolution_clock::now();

    // Inner loop: iterate over every pixel
    for(std::size_t y = 0; y < VIEWPORT_HEIGHT; y++) {
        for(std::size_t x = 0; x < VIEWPORT_WIDTH; x++) {
            // Invoke the meat of the implementation
            glm::vec3 sceneColor = Render(x, y, cameraTransforms, 0);

            sceneColor = ACESFilm(sceneColor);
            sceneColor = LinearToSrgb(sceneColor);

            // Assuming the scene color is unsigned and normalized
            int r = sceneColor.x * 255;
            int g = sceneColor.y * 255;
            int b = sceneColor.z * 255;

            // Return 0 if the values are negative
            r = r < 0 ? 0 : r;
            g = g < 0 ? 0 : g;
            b = b < 0 ? 0 : b;

            frameBuffer.at(Map2Dto1D(x * 3, y * 3)) = r;
            frameBuffer.at(Map2Dto1D(x * 3, y * 3) + 1) = g;
            frameBuffer.at(Map2Dto1D(x * 3, y * 3) + 2) = b;
        }
        // std::cout << "Rendered Line " << y << " out of " << VIEWPORT_HEIGHT << "\n";
    }


    stbi_write_png("test.png", VIEWPORT_WIDTH, VIEWPORT_HEIGHT, 3, frameBuffer.data(), 0);
    std::cout << "Wrote " << frameBuffer.size() << " elements (hopefully)\n";

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop - start);

    std::cout << "Execution took " << duration.count() << "ms\n";

    return 0;
}