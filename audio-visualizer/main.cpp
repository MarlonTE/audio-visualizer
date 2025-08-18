#include <iostream>
#include <thread>
#include "common.h"
#include "audio-capture.h"
#include "audio-processing.h"
#include "renderer.h" // Incluimos el nuevo archivo de cabecera

int main() {
    // Create an instance of the shared data structure for audio capture.
    AudioData sharedAudioData;
    sharedAudioData.in_data.resize(FFT_SIZE);

    // Create an instance of the shared data structure for visualization.
    VisualizerData sharedVisualizerData;
    sharedVisualizerData.out_data[0].resize(NUM_BARS);
    sharedVisualizerData.out_data[1].resize(NUM_BARS);
    sharedVisualizerData.write_buffer_index.store(0);

    // Create a thread for audio capture.
    std::thread audioCaptureThread(AudioCaptureThread, std::ref(sharedAudioData));

    // Create a thread for signal processing.
    std::thread signalProcessingThread(AudioProcessingThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData));

    // Create a thread for rendering the visualization.
    std::thread renderThread(RenderThread, std::ref(sharedVisualizerData));

    // NOTE: For a real application, you would not join the threads here.
    // Instead, the main thread would run the message loop and terminate when the
    // rendering window is closed. For now, we'll join for simplicity.

    // Wait for the threads to finish.
    audioCaptureThread.join();
    signalProcessingThread.join();
    renderThread.join();

    std::cout << "Presiona Enter para salir..." << std::endl;
    std::cin.get();

    return 0;
}
