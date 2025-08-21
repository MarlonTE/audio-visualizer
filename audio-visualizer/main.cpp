#include <iostream>
#include <thread>
#include <Windows.h> // Necesario para la función FreeConsole()
#include "common.h"
#include "audio-capture.h"
#include "audio-processing.h"
#include "renderer.h"
#include "config.h"

int main() {
    // Ocultar la ventana de la consola.
    // Esto es específico de Windows. En otros sistemas operativos, se maneja de forma diferente.
    // Para depurar, es posible que quieras comentar esta línea.
    FreeConsole();

    // Crear una instancia de la estructura de datos compartida para la captura de audio.
    AudioData sharedAudioData;
    sharedAudioData.in_data.resize(FFT_SIZE);

    // Crear una instancia de la estructura de datos compartida para la visualización.
    VisualizerData sharedVisualizerData;
    // Inicializamos con un valor por defecto.
    const int DEFAULT_WINDOW_WIDTH = 1024;
    sharedVisualizerData.out_data[0].resize(DEFAULT_WINDOW_WIDTH);
    sharedVisualizerData.out_data[1].resize(DEFAULT_WINDOW_WIDTH);
    sharedVisualizerData.write_buffer_index.store(0);
    sharedVisualizerData.atomic_num_bars.store(DEFAULT_WINDOW_WIDTH);
    sharedVisualizerData.should_terminate.store(false);

    // Crear una instancia de la estructura de configuración compartida.
    SharedConfigData sharedConfigData;
    // Cargar la configuración desde el archivo.
    LoadConfig(sharedConfigData, "config.json");

    // Crear un hilo para la captura de audio.
    std::thread audioCaptureThread(AudioCaptureThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData));

    // Crear un hilo para el procesamiento de la señal.
    std::thread signalProcessingThread(AudioProcessingThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData), std::ref(sharedConfigData));

    // Crear un hilo para el renderizado de la visualización, pasándole los datos de visualización y de configuración.
    std::thread renderThread(RenderThread, std::ref(sharedVisualizerData), std::ref(sharedConfigData));

    // Esperar a que el hilo de renderizado termine (cuando la ventana se cierra).
    renderThread.join();

    // Notificar a los otros hilos que deben terminar.
    sharedVisualizerData.should_terminate.store(true);
    sharedAudioData.cv.notify_one();
    sharedVisualizerData.cv.notify_one();

    // Esperar a que los hilos restantes terminen.
    audioCaptureThread.join();
    signalProcessingThread.join();

    return 0;
}
