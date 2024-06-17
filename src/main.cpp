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

#ifdef MQTT_BENCHMARK_MODE
    #include <mqtt/client.h>
#endif

// STB for image writes
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// GLM since I don't want to implement MAD myself lol suck it arduino
#define GLM_FORCE_INTRINSICS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// MQTT parameters
const std::string BROKER_URL = "industrial.api.ubidots.com:1883";
const std::string CLIENT_ID = "ubidots_mqtt_cpp_client";
const std::string TOKEN = "BBUS-FJ0t8ffIFZZqVjAbWIfcBtFEhNoIJC";
const std::string DEVICE_LABEL = "mqttraytracer";
const std::string VARIABLE_LABEL = "rendertime";
const int QOS = 0;

#include "include/constants.h"
#include "include/utils.h"
#include "include/intersections.h"
#include "include/renderer.h"

#ifdef MQTT_BENCHMARK_MODE
    class callback : public virtual mqtt::callback {
        void connected(const std::string& cause) override {
            std::cout << MQTT_HINT << "Connection successful" << std::endl;
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
#endif

int main() {
    // Init
    stbi_flip_vertically_on_write(1);

    #ifdef MQTT_BENCHMARK_MODE
        mqtt::async_client client(BROKER_URL, CLIENT_ID);
        callback cb;
        client.set_callback(cb);

        mqtt::connect_options connOpts;
        connOpts.set_clean_session(true);
        connOpts.set_user_name(TOKEN);
        connOpts.set_password("");
    #endif

    static std::array<unsigned char, 3 * VIEWPORT_WIDTH * VIEWPORT_HEIGHT> pngBuffer{};
    static std::array<glm::vec3, VIEWPORT_WIDTH * VIEWPORT_HEIGHT> frameBuffer{};
    glm::mat4 cameraViewMatrix = glm::mat4(1.0f);

    auto start = std::chrono::high_resolution_clock::now();

    const std::size_t numTiles = THREADS;
    const std::size_t tileSize = VIEWPORT_HEIGHT / numTiles;

    std::cout << RENDERER_HINT << "Tiles: " << numTiles << " Tile Height: " << tileSize << '\n';

    std::vector<std::thread> threads;

    for (std::size_t tile = 0; tile < numTiles; tile++) {
        threads.emplace_back(
            DispatchTile, 
            std::ref(frameBuffer), 
            tile,
            std::cref(cameraViewMatrix)
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
    std::cout << RENDERER_HINT << "Wrote " << pngBuffer.size() << " elements (hopefully)\n";

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> (stop - start);

    std::cout << RENDERER_HINT << "Execution took " << duration.count() << "ms\n";

    #ifdef MQTT_BENCHMARK_MODE
        // Need a better solution. Fuck if I know
        std::ostringstream oss;
        oss << duration.count();

        std::string pubTopic = "/v1.6/devices/" + DEVICE_LABEL;
        std::string payload = "{\"" + VARIABLE_LABEL + "\": " + oss.str() + "}";

        try {
            std::cout << MQTT_HINT << "Connecting to the broker" << std::endl;
            client.connect(connOpts)->wait();

            // Fuck off it doesnt work anyways leave it.
            mqtt::message_ptr pubmsg = mqtt::make_message(pubTopic, payload);
            pubmsg->set_qos(QOS);

            client.publish(pubmsg)->wait_for(std::chrono::seconds(10));
            std::this_thread::sleep_for(std::chrono::seconds(5));

            client.disconnect()->wait();
            std::cout << MQTT_HINT << "Disconnected" << std::endl;
        } catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            return 1;
        }
    #endif

    return 0;
}