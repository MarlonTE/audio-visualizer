#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Tamaño del buffer de audio para el procesamiento.
// Este valor debe coincidir con el tamaño de los datos de audio capturados.
const int FFT_SIZE = 1024;

// Estructura de datos compartida para los datos de audio crudo
// que se transfieren del hilo de captura al hilo de procesamiento.
struct AudioData {
    std::vector<double> in_data;
    std::mutex mtx;
    std::condition_variable cv;
};

// Estructura de datos compartida para los datos procesados de la FFT
// que se transfieren del hilo de procesamiento al hilo de renderizado.
struct VisualizerData {
    // Búferes para el doble búfer. El hilo de procesamiento escribe en uno
    // mientras el hilo de renderizado lee del otro.
    std::vector<double> out_data[2];
    std::mutex mtx;
    std::condition_variable cv;
    // Un índice para seguir el rastro del búfer que se está escribiendo actualmente.
    // Usamos std::atomic para un acceso seguro sin necesidad de un mutex adicional
    // para esta operación atómica.
    std::atomic<int> write_buffer_index;
};

// Declaración de las funciones del hilo de captura y de procesamiento.
extern void AudioCaptureThread(AudioData& sharedData);
extern void SignalProcessingThread(AudioData& audioData, VisualizerData& visualizerData);
