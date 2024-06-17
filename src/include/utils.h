#pragma once

#define MQTT_HINT "[\e[0;36mMQTT\033[0m]\t\t"
#define RENDERER_HINT "[\e[0;32mRENDERER\033[0m]\t"

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