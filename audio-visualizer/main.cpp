#include <iostream>
#include <thread>
#include "common.h"
#include "audio-capture.h"
#include "audio-processing.h"
#include "renderer.h"
#include "config.h"

int main() {
    // Crear una instancia de la estructura de datos compartida para la captura de audio.
    AudioData sharedAudioData;
    sharedAudioData.in_data.resize(FFT_SIZE);

    // Crear una instancia de la estructura de datos compartida para la visualización.
    VisualizerData sharedVisualizerData;
    sharedVisualizerData.out_data[0].resize(NUM_BARS);
    sharedVisualizerData.out_data[1].resize(NUM_BARS);
    sharedVisualizerData.write_buffer_index.store(0);

    // Crear una instancia de la estructura de configuración compartida.
    SharedConfigData sharedConfigData;
    // Cargar la configuración desde el archivo.
    LoadConfig(sharedConfigData, "config.json");

    // Crear un hilo para la captura de audio.
    std::thread audioCaptureThread(AudioCaptureThread, std::ref(sharedAudioData));

    // Crear un hilo para el procesamiento de la señal.
    std::thread signalProcessingThread(AudioProcessingThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData));

    // Crear un hilo para el renderizado de la visualización, pasándole los datos de visualización y de configuración.
    std::thread renderThread(RenderThread, std::ref(sharedVisualizerData), std::ref(sharedConfigData));

    // Esperar a que los hilos terminen.
    // En una aplicación real, el hilo principal ejecutaría el bucle de mensajes
    // de la ventana y los hilos terminarían cuando la ventana se cierre.
    audioCaptureThread.join();
    signalProcessingThread.join();
    renderThread.join();

    return 0;
}
