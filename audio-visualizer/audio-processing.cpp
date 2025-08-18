#include "audio-processing.h"
#include <iostream>
#include <fftw3.h> // Se asume que has instalado la biblioteca FFTW3
#include <cmath> // Para log10 y sqrt

// Función principal del hilo de procesamiento de audio
void AudioProcessingThread(AudioData& sharedData, VisualizerData& sharedVisualizerData) {
    std::cout << "Hilo de procesamiento de señal iniciado." << std::endl;

    // Crear un plan para la FFT.
    // FFTW_ESTIMATE: no realiza ninguna medición, por lo que el tiempo de planificación es insignificante.
    // La desventaja es que el rendimiento puede no ser óptimo.
    // Aunque para nuestra aplicación, esto es suficiente.
    fftw_plan plan_forward;

    // In-place FFT: la entrada se sobrescribe con la salida.
    fftw_complex* fft_out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (FFT_SIZE / 2 + 1));
    if (fft_out == NULL) {
        std::cerr << "Error: No se pudo asignar memoria para fft_out." << std::endl;
        return;
    }

    // Bucle principal de procesamiento
    while (true) {
        std::unique_lock<std::mutex> lock(sharedData.mtx);

        // Espera a que el hilo de captura de audio notifique que hay nuevos datos disponibles.
        sharedData.cv.wait(lock);

        // La FFT se realiza aquí.
        // La biblioteca FFTW necesita que la entrada sea de tipo double.
        // `in_data` ya es un vector de double, así que podemos usarlo directamente.
        plan_forward = fftw_plan_dft_r2c_1d(FFT_SIZE, sharedData.in_data.data(), fft_out, FFTW_ESTIMATE);

        fftw_execute(plan_forward);

        // Libera la memoria utilizada por el plan de FFT.
        fftw_destroy_plan(plan_forward);

        // Copia los datos de la FFT al vector de datos de salida compartido.
        // Solo nos interesan los valores de magnitud.
        // El espectro es simétrico, así que solo usamos la primera mitad (FFT_SIZE / 2).
        int write_index = sharedVisualizerData.write_buffer_index.load();

        for (int i = 0; i < NUM_BARS; ++i) {
            // Calcula la magnitud del número complejo (real^2 + imag^2)^0.5
            double magnitude = sqrt(fft_out[i][0] * fft_out[i][0] + fft_out[i][1] * fft_out[i][1]);
            // Escala la magnitud a un rango útil, logarítmicamente.
            // NOTA: out_data es miembro de sharedVisualizerData, no de sharedData.
            sharedVisualizerData.out_data[write_index][i] = 10.0 * log10(1 + magnitude);
        }

        // Alternar el índice del búfer para que el hilo de renderizado lea del búfer que acaba de ser escrito.
        sharedVisualizerData.write_buffer_index.store(1 - write_index);

        // Notifica al hilo de renderizado que hay nuevos datos de visualización disponibles.
        lock.unlock();
        sharedVisualizerData.cv.notify_one();
    }

    // Liberar memoria
    fftw_free(fft_out);
}
