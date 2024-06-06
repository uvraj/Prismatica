#include <iostream>
#include <cmath>
#include <array>

// STB for image writes
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// GLM since I don't want to implement MAD myself lol suck it
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// A bunch of defines
#define VIEWPORT_WIDTH 256
#define VIEWPORT_HEIGHT 256
#define IMAGE_CHANNELS 3

std::size_t map2Dto1D(std::size_t x, std::size_t y) {
    return x + y * VIEWPORT_WIDTH;
}

int main() {
    stbi_flip_vertically_on_write(1);
    static std::array<unsigned char, VIEWPORT_HEIGHT * VIEWPORT_WIDTH * 3> data{};

    glm::mat4 projectionMatrix = glm::perspective(glm::radians(100.0f), (float) VIEWPORT_WIDTH / (float) VIEWPORT_HEIGHT, 0.1f, 100.0f);
    glm::mat4 projectionMatrixInverse = glm::inverse(projectionMatrix);
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::rotate(viewMatrix, glm::radian3.141592 / 4.0, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewMatrixInverse = inverse(viewMatrix);

    for(int y = 0; y < VIEWPORT_HEIGHT; y++) {
        for(int x = 0; x < VIEWPORT_WIDTH; x++) {
            glm::vec2 nCoord = glm::vec2{(float) x / VIEWPORT_WIDTH, (float) y / VIEWPORT_HEIGHT};
            glm::vec3 screenPos = glm::vec3(nCoord, -1.0);
            glm::vec4 clipPos = glm::vec4(screenPos * glm::vec3(2.0) - glm::vec3(1.0), 1.0);
            glm::vec4 viewPos = projectionMatrixInverse * clipPos;
            glm::vec4 worldPos = viewMatrixInverse * clipPos;

            glm::vec3 worldVector = glm::normalize(glm::vec3(worldPos.x, worldPos.y, worldPos.z));

            int r = worldVector.x * 255;
            int g = worldVector.y * 255;
            int b = worldVector.z * 255;

            // Return 0 if the values are negative
            r = r < 0 ? 0 : r;
            g = g < 0 ? 0 : g;
            b = b < 0 ? 0 : b;

            data.at(map2Dto1D(x * 3, y * 3)) = r;
            data.at(map2Dto1D(x * 3, y * 3) + 1) = g;
            data.at(map2Dto1D(x * 3, y * 3) + 2) = b;
        }
    }

    stbi_write_png("test.png", VIEWPORT_WIDTH, VIEWPORT_HEIGHT, IMAGE_CHANNELS, data.data(), 0);
    std::cout << "Wrote " << data.size() << " bytes (hopefully)";

    return 0;
}