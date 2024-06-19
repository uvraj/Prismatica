#pragma once

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

glm::vec3 Render(const std::size_t& x, const std::size_t& y, const glm::mat4& viewMatrix, const std::size_t& sample) {
    glm::vec3 sceneColor{0.0f};

    glm::vec2 nCoord = glm::vec2{(float) x / VIEWPORT_WIDTH, (float) y / VIEWPORT_HEIGHT};
    glm::vec2 screenPos = nCoord * glm::vec2(2.0f) - glm::vec2(1.0f);
    glm::vec3 rayTarget = glm::vec3(std::sin(screenPos.x * 0.5f), screenPos.y * 0.5f * ((float) VIEWPORT_HEIGHT / (float) VIEWPORT_WIDTH), -std::cos(screenPos.x * 0.5f));
    glm::vec3 rayDir = normalize(rayTarget);
    glm::vec3 rayDirWorld = glm::mat3(viewMatrix) * rayDir;
    glm::vec3 rayOrigin = glm::vec3(0.0f, 0.0f, 4.0f);

    std::random_device randDevice;
    std::default_random_engine rngGen{randDevice()};
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};

    auto RNG = std::bind(distribution, rngGen);

    glm::vec2 rand = glm::vec2(0.0f);

    HitData hitData{};

    // Camera Ray
    RayTraceScene(hitData, rayOrigin, rayDirWorld);
    sceneColor = hitData.material.emissivity * hitData.material.albedo;

    rand.x = RNG();
    rand.y = RNG();
    glm::vec3 randomDir = UniformSampleHemisphere(hitData.hitNormal, rand);

    // TODO: Path trace recursively, manually adding bounces isn't the cleanest approach
    HitData randomHit{};
    RayTraceScene(randomHit, hitData.hitPos + hitData.hitNormal * 0.001f, randomDir);

    if (hitData.rayHit && !hitData.material.isEmissive) {
        if (randomHit.rayHit && randomHit.material.isEmissive) {
            sceneColor += hitData.material.albedo * randomHit.material.emissivity * randomHit.material.albedo * glm::dot(hitData.hitNormal, randomDir) * 2.0f; 
            return sceneColor;
        }

        rand.x = RNG();
        rand.y = RNG();
        glm::vec3 randomDir2 = UniformSampleHemisphere(randomHit.hitNormal, rand);

        HitData randomHit2{};
        RayTraceScene(randomHit2, randomHit.hitPos + randomHit.hitNormal * 0.001f, randomDir2);

        if (randomHit.rayHit && !randomHit.material.isEmissive) {
            glm::vec3 contrib = randomHit.material.albedo * randomHit2.material.emissivity * randomHit2.material.albedo * glm::dot(randomHit.hitNormal, randomDir2) * 2.0f; 
            sceneColor += hitData.material.albedo * 2.0f * contrib * glm::dot(hitData.hitNormal, randomDir); 
        }
    }

    return sceneColor;
}

void DispatchTile(std::array<glm::vec3, VIEWPORT_WIDTH * VIEWPORT_HEIGHT>& frameBuffer, std::size_t tileID, const glm::mat4 cameraViewMatrix) {
    std::cout << RENDERER_HINT << "Dispatching tile with ID: " << tileID << '\n';
    std::size_t startY = tileID * VIEWPORT_HEIGHT / THREADS;
    std::size_t endY = (tileID + 1) * VIEWPORT_HEIGHT / THREADS;

    for (std::size_t x = 0; x < VIEWPORT_WIDTH; x++) {
        for (std::size_t y = startY; y < endY; y++) {
            // Invoke the meat of the implementation
            glm::vec3 sceneColor = glm::vec3(0.0);

            for (std::size_t sample = 0; sample < SAMPLES; sample++) {
                sceneColor += Render(x, y, cameraViewMatrix, sample);
            }

            sceneColor /= (float) SAMPLES;
            sceneColor = ACESFilm(sceneColor);
            sceneColor = LinearToSrgb(sceneColor);

            frameBuffer.at(y * VIEWPORT_WIDTH + x) = sceneColor;
        }
    }
}