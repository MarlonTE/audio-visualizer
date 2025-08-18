#include "renderer.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <cmath>
#include <atomic>

// The main rendering thread function
void RenderThread(VisualizerData& sharedVisualizerData) {
    std::cout << "Rendering thread started." << std::endl;

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

        // Lock and wait for the processing thread to notify new data
        std::unique_lock<std::mutex> lock(sharedVisualizerData.mtx);
        sharedVisualizerData.cv.wait(lock);

        // Use glBegin and glEnd for immediate mode rendering.
        // It's not the most efficient, but it's simple for this example.
        glBegin(GL_QUADS);

        // Get the index of the buffer that the processing thread just wrote to
        int read_index = sharedVisualizerData.write_buffer_index.load();

        // Loop through the data and draw a bar for each frequency bin.
        for (int i = 0; i < NUM_BARS; ++i) {
            // Read the processed data from the shared buffer.
            double bar_height_normalized = sharedVisualizerData.out_data[read_index][i] / 50.0;
            if (bar_height_normalized > 1.0) bar_height_normalized = 1.0;
            if (bar_height_normalized < 0.0) bar_height_normalized = 0.0;

            // Bar dimensions and position.
            double bar_width = 2.0 / NUM_BARS;
            double x_position = -1.0 + i * bar_width;
            double y_height = bar_height_normalized * 1.5 - 1.0;

            // Set the color of the bar (e.g., a green gradient).
            glColor3f(0.1f + bar_height_normalized * 0.5f, 0.9f, 0.3f);

            // Draw a quad (rectangle) for the bar.
            glVertex2f(x_position, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, -1.0f);
            glVertex2f(x_position + bar_width - 0.01f, y_height);
            glVertex2f(x_position, y_height);
        }

        glEnd();

        // Unlock the mutex
        lock.unlock();

        // Swap the front and back buffers
        glfwSwapBuffers(window);
    }

    // Clean up resources
    glfwDestroyWindow(window);
    glfwTerminate();
}
