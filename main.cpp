#include <iostream>
#include <cmath>
#include <array>
#include <chrono>

// STB for image writes
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// GLM since I don't want to implement MAD myself lol suck it arduino
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// A bunch of defines
#define VIEWPORT_WIDTH 1920
#define VIEWPORT_HEIGHT 1080

class CameraTransforms {
public:
    glm::mat4 projectionMatrix;
    glm::mat4 projectionMatrixInverse;
    glm::mat4 viewMatrix;
    glm::mat4 viewMatrixInverse;

    CameraTransforms(const float& fov) {
        projectionMatrix = glm::perspective(glm::radians(fov), (float) VIEWPORT_WIDTH / (float) VIEWPORT_HEIGHT, 0.1f, 100.0f);
        projectionMatrixInverse = glm::inverse(projectionMatrix);
        viewMatrix = glm::rotate(glm::mat4(1.0f), 3.141592f / 8.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        // viewMatrix = viewMatrix * glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        viewMatrixInverse = glm::inverse(viewMatrix);
    }

    void onNewFrame() {
        // TODO: Animation support
        return;
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

void IntersectSphere(glm::vec3& hitPos, glm::vec3& hitNormal, bool& rayHit, const glm::vec3& sphereOrigin, const glm::vec3& rayOrigin, const glm::vec3& direction, const float& radius) {
    hitPos = glm::vec3(0.0f);
    hitNormal = glm::vec3(0.0f);
    rayHit = false;

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

    hitPos = rayOrigin + direction * t;
    hitNormal = glm::normalize(hitPos - sphereOrigin);
    rayHit = true;
}

void IntersectPlane(glm::vec3& hitPos, bool& rayHit, const glm::vec3& normal, const glm::vec3& origin, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float& t) {
    // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection.html
    // Equation 5

    hitPos = glm::vec3(0.0f);
    rayHit = false;
    t = 0.0f;

    float denom = glm::dot(normal, rayDirection);

    if (std::abs(denom) > 1e-6) {
        glm::vec3 originsDelta = origin - rayOrigin;
        t = glm::dot(originsDelta, normal) / denom;

        if (t >= 0) {
            hitPos = t * rayDirection + rayOrigin;
            rayHit = true;
        }
    }
}

void RayTraceScene(glm::vec3& hitPos, glm::vec3& hitNormal, bool& rayHit, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
    hitPos = glm::vec3(0.0f);
    hitNormal = glm::vec3(0.0f);

    glm::vec3 planeOrigin1 = glm::vec3(0.0f, -0.5f, -1.0f);
    glm::vec3 hitPosPlane1{};
    glm::vec3 planeNormal1 = glm::vec3(0.0f, 1.0f, 0.0f);
    bool rayHitPlane1 = false;
    float t1{};

    IntersectPlane(hitPosPlane1, rayHitPlane1, planeNormal1, planeOrigin1, rayOrigin, rayDirection, t1);

    glm::vec3 hitPosSphere{};
    glm::vec3 hitNormalSphere{};
    glm::vec3 sphereOrigin = glm::vec3(0.0f, 0.5, -3.5f);
    bool rayHitSphere{false};
    IntersectSphere(hitPosSphere, hitNormalSphere, rayHitSphere, sphereOrigin, rayOrigin, rayDirection, 0.75f);
    
    // Choose the closer intersection point

    if (rayHitPlane1 && rayHitSphere) {
        if (length(hitPosPlane1 - rayOrigin) < length(hitPosSphere - rayOrigin)) {
            hitPos = hitPosPlane1;
            hitNormal = planeNormal1;
        } else {
            hitPos = hitPosSphere,
            hitNormal = hitNormalSphere;
        }
    } else if (rayHitPlane1) {
        hitPos = hitPosPlane1;
        hitNormal = planeNormal1;
    } else if (rayHitSphere) {
        hitPos = hitPosSphere,
        hitNormal = hitNormalSphere;
    }

    rayHit = rayHitSphere || rayHitPlane1;
}

glm::vec3 CalculatePointLightContribution(const glm::vec3& illuminance, const glm::vec3& worldPos, const glm::vec3& pointLightPos, const glm::vec3& normal) {
    float nDotL = glm::dot(normal, glm::normalize(pointLightPos - worldPos));
          nDotL = glm::clamp(nDotL, 0.0f, 1.0f);
    float attenuation = 1.0 / std::pow(glm::distance(worldPos, pointLightPos), 2.0f);
    return illuminance * nDotL * attenuation;
}

glm::vec3 Render(const std::size_t& x, const std::size_t& y, const CameraTransforms& cameraTransforms) {
    glm::vec3 sceneColor{0.0f};

    glm::vec2 nCoord = glm::vec2{(float) x / VIEWPORT_WIDTH, (float) y / VIEWPORT_HEIGHT};
    glm::vec3 screenPos = glm::vec3(nCoord, -1.0);
    glm::vec4 clipPos = glm::vec4(screenPos * glm::vec3(2.0) - glm::vec3(1.0), 1.0);
    glm::vec4 viewPos = cameraTransforms.projectionMatrixInverse * clipPos;
              viewPos = glm::vec4(viewPos.x / viewPos.w, viewPos.y / viewPos.w, viewPos.z / viewPos.w, 1.0);
    glm::vec4 worldPos = cameraTransforms.viewMatrixInverse * viewPos;

    glm::vec3 worldVector = glm::normalize(glm::vec3(worldPos.x, worldPos.y, worldPos.z));

    bool rayHit{false};
    glm::vec3 hitPos{};
    glm::vec3 hitNormal;

    glm::vec3 rayOrigin = glm::vec3(0.0f, 1.0f, 0.0f);
    RayTraceScene(hitPos, hitNormal, rayHit, rayOrigin, worldVector);

    glm::vec3 pointLightPos = glm::vec3(0.0f, -0.4f, -2.0f);

    if (rayHit) {
        sceneColor += CalculatePointLightContribution(glm::vec3(1.0f, 0.0f, 0.0f), hitPos, glm::vec3(-0.6f, -0.3f, -2.0f), hitNormal);
        sceneColor += CalculatePointLightContribution(glm::vec3(0.0f, 1.0f, 0.0f), hitPos, glm::vec3(0.0f, -0.3f, -2.0f),  hitNormal);
        sceneColor += CalculatePointLightContribution(glm::vec3(0.0f, 0.0f, 1.0f), hitPos, glm::vec3(0.6f, -0.3f, -2.0f),  hitNormal);
    }

    //return hitNormal * glm::vec3(0.1f);
    return sceneColor;
}

int main() {
    stbi_flip_vertically_on_write(1);
    static std::array<unsigned char, VIEWPORT_HEIGHT * VIEWPORT_WIDTH * 3> frameBuffer{};

    CameraTransforms cameraTransforms(70.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for(std::size_t y = 0; y < VIEWPORT_HEIGHT; y++) {
        for(std::size_t x = 0; x < VIEWPORT_WIDTH; x++) {
            // Invoke the meat of the implementation
            glm::vec3 sceneColor = Render(x, y, cameraTransforms);

            sceneColor = Reinhard(sceneColor);
            sceneColor = ACESFilm(sceneColor);

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
    std::cout << "Wrote " << frameBuffer.size() << " bytes (hopefully)\n";

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop - start);

    std::cout << "Execution took " << duration.count() << "ms\n";

    return 0;
}