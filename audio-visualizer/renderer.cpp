#include "renderer.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <cmath>
#include <atomic>

// The main rendering thread function.
void RenderThread(VisualizerData& sharedVisualizerData) {
    std::cout << "Hilo de renderizado iniciado." << std::endl;

    // Initialize GLFW.
    if (!glfwInit()) {
        std::cerr << "Error: No se pudo inicializar GLFW." << std::endl;
        return;
    }

    // Create a windowed mode window and its OpenGL context.
    GLFWwindow* window = glfwCreateWindow(1024, 600, "Visualizador de Audio", NULL, NULL);
    if (!window) {
        std::cerr << "Error: No se pudo crear la ventana de GLFW." << std::endl;
        glfwTerminate();
        return;
    }

    // Make the window's context current.
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync.

    // Get the window size for positioning the bars.
    int window_width, window_height;
    glfwGetWindowSize(window, &window_width, &window_height);

    // Main rendering loop.
    while (!glfwWindowShouldClose(window)) {
        // Poll for and process events.
        glfwPollEvents();

        // Clear the screen to a gray color.
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- Start of drawing logic ---
        // Get the index of the buffer to read from (the one that was just written to).
        int read_index = 1 - sharedVisualizerData.write_buffer_index.load();

        // Lock the mutex to ensure we have a consistent view of the data.
        std::unique_lock<std::mutex> lock(sharedVisualizerData.mtx);

        // Wait for the audio processing thread to signal new data.
        sharedVisualizerData.cv.wait(lock);

        // Use glBegin and glEnd for immediate mode rendering.
        // It's not the most efficient, but it's simple for this example.
        glBegin(GL_QUADS);

        // Loop through the data and draw a bar for each frequency bin.
        for (int i = 0; i < NUM_BARS; ++i) {
            // Read the processed data from the shared buffer.
            double bar_height_normalized = sharedVisualizerData.out_data[read_index][i] / 50.0;
            if (bar_height_normalized > 1.0) bar_height_normalized = 1.0;
            if (bar_height_normalized < 0.0) bar_height_normalized = 0.0;

            // Bar dimensions and position.
            double bar_width = 2.0 / NUM_BARS;
            double x_position = -1.0 + i * bar_width;
            double y_height = bar_height_normalized * 1.5;

            // Set the color of the bar (e.g., a green gradient).
            glColor3f(0.1f + bar_height_normalized * 0.9f, 0.9f, 0.3f);

            // Draw a quad (rectangle) for the bar.
            glVertex2f(x_position, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, -1.0f + y_height);
            glVertex2f(x_position, -1.0f + y_height);
        }

        glEnd();

        // --- End of drawing logic ---
        lock.unlock();

        // Swap front and back buffers.
        glfwSwapBuffers(window);
    }

    glfwTerminate();
}
