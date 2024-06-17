#pragma once

constexpr float PI = 3.141592653589793238462643383279502884f; // Might be a bit excessive
constexpr float TAU = 2.0f * PI;
constexpr float HPI = PI * 0.5f;
constexpr float rPI = 1.0f / PI;
constexpr float rTAU = 1.0f / TAU;
constexpr float PHI = 1.6180339887498948482f;

constexpr int VIEWPORT_WIDTH    = 1024;
constexpr int VIEWPORT_HEIGHT   = 1024;
constexpr int SAMPLES           = 64;
constexpr int THREADS           = 16;
constexpr int MAX_RECURSIONS    = 3;