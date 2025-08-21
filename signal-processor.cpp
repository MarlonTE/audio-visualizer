#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <complex> // Se necesita para std::complex
#include "fftw3.h"
#include "signal-processor.h"

// Definimos la constante M_PI manualmente para asegurar la portabilidad
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Función para procesar los datos de audio con la FFT
void ProcessAudioData(const std::vector<double>& audio_frames, std::vector<double>& fft_out_data) {
    // Verificar que el tamaño del buffer de entrada coincide con el tamaño de la FFT.
    if (audio_frames.size() != FFT_SIZE) {
        std::cerr << "Error: El tamaño del buffer de audio no coincide con el tamaño de la FFT." << std::endl;
        return;
    }

    // Crear un búfer local no constante para los datos de entrada de la FFT.
    std::vector<double> local_in_data(audio_frames.begin(), audio_frames.end());

    // Usar la biblioteca FFTW para realizar la FFT.
    fftw_complex* fftw_out = reinterpret_cast<fftw_complex*>(fft_out_data.data());
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, local_in_data.data(), fftw_out, FFTW_ESTIMATE);

    // Ejecutar la transformación.
    fftw_execute(plan);

    // Calcular la magnitud del resultado para cada bin de frecuencia.
    for (size_t i = 0; i < fft_out_data.size(); ++i) {
        // La magnitud se calcula a partir de los números complejos en el búfer de salida.
        std::complex<double> complex_val(fftw_out[i][0], fftw_out[i][1]);
        fft_out_data[i] = std::abs(complex_val);
    }

    // Liberar los recursos de la FFTW.
    fftw_destroy_plan(plan);
}

// Función principal del hilo de procesamiento de la señal.
void SignalProcessingThread(AudioData& audioData, VisualizerData& visualizerData) {
    // Crear un búfer local para el cálculo de la FFT para evitar
    // cualquier problema de sincronización con la escritura en los búferes de visualización.
    std::vector<double> local_fft_out_data(FFT_SIZE / 2 + 1);

    std::cout << "Hilo de procesamiento de señal iniciado." << std::endl;

    // Bucle principal que espera por los datos de audio.
    while (true) {
        // Bloquear el mutex del búfer de audio y esperar la notificación del hilo de captura.
        std::unique_lock<std::mutex> lock(audioData.mtx);
        audioData.cv.wait(lock);

        // Procesar los datos de audio y almacenarlos en el búfer local.
        ProcessAudioData(audioData.in_data, local_fft_out_data);
        lock.unlock();

        // Bloquear el mutex del búfer de visualización para escribir.
        std::unique_lock<std::mutex> vis_lock(visualizerData.mtx);

        // Determinar en qué búfer escribir.
        int write_index = visualizerData.write_buffer_index.load();

        // Copiar los datos procesados al búfer de visualización correcto.
        std::copy(local_fft_out_data.begin(), local_fft_out_data.end(), visualizerData.out_data[write_index].begin());

        // Alternar el índice del búfer para la próxima escritura.
        visualizerData.write_buffer_index.store(1 - write_index);

        vis_lock.unlock();

        // Notificar al hilo de renderizado que hay nuevos datos disponibles.
        visualizerData.cv.notify_one();
    }
}
