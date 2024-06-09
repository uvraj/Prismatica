#include <iostream>
#include <cmath>
#include <array>
#include <chrono>
#include <algorithm>
#include <vector>
#include <random>
#include <sstream>
#include <thread>
#include <mutex>

#include <mqtt/client.h>

// STB for image writes
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// GLM since I don't want to implement MAD myself lol suck it arduino
#define GLM_FORCE_SIMD_AVX2
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// A bunch of defines
#define VIEWPORT_WIDTH 2048
#define VIEWPORT_HEIGHT 1024
#define SAMPLES 1024
#define THREADS 16

#define MQTT "[\e[0;36mMQTT\033[0m]\t\t"
#define RENDERER "[\e[0;32mRENDERER\033[0m]\t"

const float PI = 3.141592f;
const float TAU = 2.0f * PI;
const float HPI = PI * 0.5f;
const float rPI = 1.0f / PI;
const float rTAU = 1.0f / TAU;
const float PHI = std::sqrt(5.0f) * 0.5f + 0.5f;

const std::string BROKER_URL = "industrial.api.ubidots.com:1883";
const std::string CLIENT_ID = "ubidots_mqtt_cpp_client";
const std::string TOKEN = "BBUS-FJ0t8ffIFZZqVjAbWIfcBtFEhNoIJC";
const std::string DEVICE_LABEL = "mqttraytracer";
const std::string VARIABLE_LABEL = "rendertime";
const int QOS = 0;

class callback : public virtual mqtt::callback
{
    void connected(const std::string& cause) override {
        std::cout << MQTT << "Connection successful" << std::endl;
    }

    void connection_lost(const std::string& cause) override {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::cout << "\nMessage arrived" << std::endl;
        std::cout << "topic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "payload: '" << msg->to_string() << "'" << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}
};

class CameraTransforms {
public:
    glm::mat4 projectionMatrix;
    glm::mat4 projectionMatrixInverse;
    glm::mat4 viewMatrix;
    glm::mat4 viewMatrixInverse;

    CameraTransforms(const float& fov) {
        projectionMatrix = glm::perspective(glm::radians(fov), (float) VIEWPORT_WIDTH / (float) VIEWPORT_HEIGHT, 0.1f, 100.0f);
        projectionMatrixInverse = glm::inverse(projectionMatrix);
        viewMatrix = glm::rotate(glm::mat4(1.0f), 3.141592f / 10.0f, glm::vec3(1.0f, 0.0f, 0.0f));
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

    Material planeMaterial{};
    planeMaterial.albedo = glm::vec3(1.0f, 1.0f, 1.0f);
    planeMaterial.emissivity = 0.0f;
    planeMaterial.isEmissive = false;

    Material sphereMaterial{};
    sphereMaterial.albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    sphereMaterial.emissivity = 1.0f;
    sphereMaterial.isEmissive = true;

    HitData hitDataTemp;
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    IntersectPlane(hitDataTemp, planeMaterial, glm::vec3(0.0f, -0.5f, -4.5f), glm::vec3(0.0f, 0.0f, 1.0f), rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    sphereMaterial.albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    IntersectSphere(hitDataTemp, sphereMaterial, glm::vec3(-1.5f, 0.5f, -3.5f), 0.5f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    sphereMaterial.albedo = glm::vec3(0.0f, 1.0f, 0.0f);
    IntersectSphere(hitDataTemp, sphereMaterial, glm::vec3(0.0f, 0.5f, -3.5f), 0.5f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    hitDataTemp = HitData();
    sphereMaterial.albedo = glm::vec3(0.0f, 0.0f, 1.0f);
    IntersectSphere(hitDataTemp, sphereMaterial, glm::vec3(1.5f, 0.5f, -3.5f), 0.5f, rayOrigin, rayDirection);
    hitDataTotal.push_back(hitDataTemp);

    
    // Choose the closer intersection
    // using std::sort might not be the fastest solution. needs investigation
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

glm::vec3 Render(const std::size_t& x, const std::size_t& y, const CameraTransforms& cameraTransforms, const glm::vec2& rand) {
    glm::vec3 sceneColor{0.0f};

    glm::vec2 nCoord = glm::vec2{(float) x / VIEWPORT_WIDTH, (float) y / VIEWPORT_HEIGHT};
    glm::vec3 screenPos = glm::vec3(nCoord, -1.0f);
    glm::vec4 clipPos = glm::vec4(screenPos * glm::vec3(2.0f) - glm::vec3(1.0f), 1.0f);
    glm::vec4 viewPos = cameraTransforms.projectionMatrixInverse * clipPos;
              viewPos = glm::vec4(viewPos.x / viewPos.w, viewPos.y / viewPos.w, viewPos.z / viewPos.w, 1.0f);
    glm::vec4 worldPos = cameraTransforms.viewMatrixInverse * viewPos;

    glm::vec3 worldVector = glm::normalize(glm::vec3(worldPos.x, worldPos.y, worldPos.z));
    glm::vec3 rayOrigin = glm::vec3(0.0f, 2.0f, 1.0f);

    HitData hitData{};

    // Camera Ray
    RayTraceScene(hitData, rayOrigin, worldVector);
    sceneColor = hitData.material.emissivity * hitData.material.albedo;

    glm::vec3 randomDir = UniformSampleHemisphere(hitData.hitNormal, rand);

    HitData randomHit{};
    RayTraceScene(randomHit, hitData.hitPos + hitData.hitNormal * 0.001f, randomDir);

    if (hitData.rayHit && !hitData.material.isEmissive) {
        sceneColor += hitData.material.albedo * 2.0f * randomHit.material.emissivity * randomHit.material.albedo * glm::dot(hitData.hitNormal, randomDir); 
    }

    return sceneColor;
    //return hitData.;
}

void DispatchTile(std::array<glm::vec3, VIEWPORT_WIDTH * VIEWPORT_HEIGHT>& frameBuffer, std::size_t tileID, const CameraTransforms& cameraTransforms) {
    std::cout << RENDERER << "Dispatching tile with ID: " << tileID << '\n';
    std::size_t startY = tileID * VIEWPORT_HEIGHT / THREADS;
    std::size_t endY = (tileID + 1) * VIEWPORT_HEIGHT / THREADS;

    std::default_random_engine rng;
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};

    float rngX{};
    float rngY{};

    glm::vec3 sceneColor = glm::vec3(0.0);

    for (std::size_t x = 0; x < VIEWPORT_WIDTH; x++) {
        for (std::size_t y = startY; y < endY; y++) {
            // Invoke the meat of the implementation
            for (std::size_t sample = 0; sample < SAMPLES; sample++) {
                rngX = distribution(rng);
                rngY = distribution(rng);
                sceneColor += Render(x, y, cameraTransforms, glm::vec2(rngX, rngY));
            }

            sceneColor /= (float) SAMPLES;
            frameBuffer.at(y * VIEWPORT_WIDTH + x) = sceneColor;
        }
    }
}


int main() {
    // Init
    stbi_flip_vertically_on_write(1);

    mqtt::async_client client(BROKER_URL, CLIENT_ID);
    callback cb;
    client.set_callback(cb);

    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);
    connOpts.set_user_name(TOKEN);
    connOpts.set_password("");

    static std::array<unsigned char, 3 * VIEWPORT_WIDTH * VIEWPORT_HEIGHT> pngBuffer{};
    static std::array<glm::vec3, VIEWPORT_WIDTH * VIEWPORT_HEIGHT> frameBuffer{};
    CameraTransforms cameraTransforms(50.0f);

    auto start = std::chrono::high_resolution_clock::now();

    const std::size_t numTiles = THREADS;
    const std::size_t tileSize = VIEWPORT_HEIGHT / numTiles;

    std::cout << RENDERER << "Tiles: " << numTiles << " Tile Height: " << tileSize << '\n';

    std::vector<std::thread> threads;

    for (std::size_t tile = 0; tile < numTiles; tile++) {
        threads.emplace_back(
            DispatchTile, 
            std::ref(frameBuffer), 
            tile,
            std::cref(cameraTransforms)
        );
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (std::size_t i = 0; i < frameBuffer.size(); i++) {
        // Assuming the data is 8 bit normalized
        int r = frameBuffer.at(i).x * 255;
        int g = frameBuffer.at(i).y * 255;
        int b = frameBuffer.at(i).z * 255;

        r = r < 0 ? 0 : r;
        g = g < 0 ? 0 : g;
        b = b < 0 ? 0 : b;

        pngBuffer.at(i * 3) = r;
        pngBuffer.at(i * 3 + 1) = g;
        pngBuffer.at(i * 3 + 2) = b;
    }

    stbi_write_png("test.png", VIEWPORT_WIDTH, VIEWPORT_HEIGHT, 3, pngBuffer.data(), 0);
    std::cout << RENDERER << "Wrote " << pngBuffer.size() << " elements (hopefully)\n";

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop - start);

    std::cout << RENDERER << "Execution took " << duration.count() << "ms\n";

    // Need a better solution. Fuck if I know
    std::ostringstream oss;
    oss << duration.count();

    std::string pubTopic = "/v1.6/devices/" + DEVICE_LABEL;
    std::string payload = "{\"" + VARIABLE_LABEL + "\": " + oss.str() + "}";

    try {
        std::cout << MQTT << "Connecting to the broker" << std::endl;
        client.connect(connOpts)->wait();

        // Fuck off it doesnt work anyways leave it.
        mqtt::message_ptr pubmsg = mqtt::make_message(pubTopic, payload);
        pubmsg->set_qos(QOS);

        client.publish(pubmsg)->wait_for(std::chrono::seconds(10));
        std::this_thread::sleep_for(std::chrono::seconds(5));

        client.disconnect()->wait();
        std::cout << MQTT << "Disconnected" << std::endl;
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }


    return 0;
}