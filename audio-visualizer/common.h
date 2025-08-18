#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Tamaño de la ventana de la FFT
const int FFT_SIZE = 1024;
// Número de barras a visualizar, que es la mitad del tamaño de la FFT
const int NUM_BARS = FFT_SIZE / 2;

// Estructura de datos compartida entre el hilo de captura y el de procesamiento
struct AudioData {
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<double> in_data; // Vector para almacenar la entrada de audio
};

// Estructura de datos compartida entre el hilo de procesamiento y el de renderizado
struct VisualizerData {
    std::mutex mtx;
    std::condition_variable cv;
    // Dos búferes para la técnica de doble amortiguación
    std::vector<double> out_data[2];
    // Índice atómico para indicar qué búfer es el que se está escribiendo actualmente
    std::atomic<int> write_buffer_index;
};
