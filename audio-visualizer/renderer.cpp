#include "renderer.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <cmath>
#include <atomic>
#include "config.h"

// Almacenamos las alturas actuales de las barras para implementar el decaimiento.
static std::vector<double> current_heights(NUM_BARS, 0.0);
// Almacenamos las alturas suavizadas para el efecto "ola".
static std::vector<double> smoothed_heights(NUM_BARS, 0.0);

// The main rendering thread function
void RenderThread(VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData) {
    std::cout << "Rendering thread started." << std::endl;

    // Obtener los factores de configuración de forma segura.
    std::unique_lock<std::mutex> config_lock(sharedConfigData.mtx);
    const float decay_factor = sharedConfigData.config.decay_factor;
    const float smoothing_factor = sharedConfigData.config.smoothing_factor;
    const float amplitude_factor = sharedConfigData.config.amplitude_factor;
    const std::vector<float> base_color = sharedConfigData.config.base_color_rgb;
    config_lock.unlock();

    // Initialize GLFW.
    if (!glfwInit()) {
        std::cerr << "Error: Failed to initialize GLFW." << std::endl;
        return;
    }

    // Create a windowed mode window and its OpenGL context.
    GLFWwindow* window = glfwCreateWindow(1024, 600, "Audio Visualizer", NULL, NULL);
    if (!window) {
        std::cerr << "Error: Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return;
    }

    // Make the window's context current.
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync to synchronize with the monitor's refresh rate.

    // Main rendering loop
    while (!glfwWindowShouldClose(window)) {
        // Poll for and process events.
        glfwPollEvents();

        // Clear the screen to a dark gray color.
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Begin drawing quads (rectangles) for the bars.
        glBegin(GL_QUADS);

        // Get the index of the buffer that the processing thread just wrote to
        int read_index = sharedVisualizerData.write_buffer_index.load();

        // Loop through the data and draw a bar for each frequency bin.
        for (int i = 0; i < NUM_BARS; ++i) {
            // Leer los datos procesados del búfer compartido.
            double raw_value = sharedVisualizerData.out_data[read_index][i];

            // Implementar decaimiento: las barras caen gradualmente.
            if (raw_value > current_heights[i]) {
                current_heights[i] = raw_value;
            }
            else {
                current_heights[i] *= decay_factor;
            }

            // Implementar suavizado: las barras se mueven en un efecto "ola".
            smoothed_heights[i] = (current_heights[i] * smoothing_factor) + (smoothed_heights[i] * (1.0 - smoothing_factor));

            // Aplicar el factor de amplitud.
            double bar_height_normalized = smoothed_heights[i] * amplitude_factor;

            // Asegurarse de que el valor está en el rango [0, 1].
            if (bar_height_normalized > 1.0) bar_height_normalized = 1.0;
            if (bar_height_normalized < 0.0) bar_height_normalized = 0.0;

            // Bar dimensions and position.
            double bar_width = 2.0 / NUM_BARS;
            double x_position = -1.0 + i * bar_width;
            double y_height = bar_height_normalized * 1.5 - 1.0;

            // Set the color of the bar using the base color from config and a gradient.
            glColor3f(base_color[0] + bar_height_normalized * 0.5f,
                base_color[1] - bar_height_normalized * 0.5f,
                base_color[2] + bar_height_normalized * 0.5f);

            // Draw a quad (rectangle) for the bar.
            glVertex2f(x_position, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, y_height);
            glVertex2f(x_position, y_height);
        }

        // End drawing.
        glEnd();

        // Swap front and back buffers.
        glfwSwapBuffers(window);
    }

    // Clean up.
    glfwTerminate();
}

