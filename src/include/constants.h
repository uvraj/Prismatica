#pragma once

constexpr float PI = 3.141592653589793238462643383279502884f; // Might be a bit excessive
constexpr float TAU = 2.0f * PI;
constexpr float HPI = PI * 0.5f;
constexpr float rPI = 1.0f / PI;
constexpr float rTAU = 1.0f / TAU;
constexpr float PHI = std::sqrt(5.0f) * 0.5f + 0.5f;

constexpr int VIEWPORT_WIDTH    = 2560;
constexpr int VIEWPORT_HEIGHT   = 1920;
constexpr int SAMPLES           = 512;
constexpr int THREADS           = 16;
constexpr int MAX_RECURSIONS    = 3;