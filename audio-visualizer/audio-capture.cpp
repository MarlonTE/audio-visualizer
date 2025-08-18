#include "audio-capture.h"
#include <iostream>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "avrt.lib")

// Constantes globales de audio
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

// Función principal del hilo de captura de audio (WASAPI)
void AudioCaptureThread(AudioData& sharedData) {
    HRESULT hr = S_OK;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WAVEFORMATEX* pwfx = NULL;

    // Búfer de acumulación para asegurar que solo enviamos datos completos de 1024 frames.
    std::vector<double> accumulated_data;

    // 1. Inicializar la biblioteca COM para el hilo.
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Error: CoInitializeEx falló. HRESULT: " << hr << std::endl;
        return;
    }

    // 2. Obtener el enumerador de dispositivos de audio por defecto.
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&pEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Error: CoCreateInstance falló. HRESULT: " << hr << std::endl;
        CoUninitialize();
        return;
    }

    // 3. Obtener el dispositivo de audio de renderizado por defecto.
    hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Error: GetDefaultAudioEndpoint falló. HRESULT: " << hr << std::endl;
        pEnumerator->Release();
        CoUninitialize();
        return;
    }

    // 4. Activar la interfaz de IAudioClient para el dispositivo.
    hr = pDevice->Activate(
        IID_IAudioClient, CLSCTX_ALL, NULL,
        (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Error: Activate IAudioClient falló. HRESULT: " << hr << std::endl;
        pEnumerator->Release();
        pDevice->Release();
        CoUninitialize();
        return;
    }

    // 5. Obtener el formato de onda del dispositivo.
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Error: GetMixFormat falló. HRESULT: " << hr << std::endl;
        pEnumerator->Release();
        pDevice->Release();
        pAudioClient->Release();
        CoUninitialize();
        return;
    }

    // 6. Inicializar la sesión de audio.
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        pwfx,
        NULL);
    if (FAILED(hr)) {
        std::cerr << "Error: Initialize falló. HRESULT: " << hr << std::endl;
        CoTaskMemFree(pwfx);
        pEnumerator->Release();
        pDevice->Release();
        pAudioClient->Release();
        CoUninitialize();
        return;
    }

    // 7. Activar la interfaz de IAudioCaptureClient.
    hr = pAudioClient->GetService(
        IID_IAudioCaptureClient,
        (void**)&pCaptureClient);
    if (FAILED(hr)) {
        std::cerr << "Error: GetService falló. HRESULT: " << hr << std::endl;
        CoTaskMemFree(pwfx);
        pEnumerator->Release();
        pDevice->Release();
        pAudioClient->Release();
        CoUninitialize();
        return;
    }

    // 8. Iniciar la captura.
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Error: Start falló. HRESULT: " << hr << std::endl;
        CoTaskMemFree(pwfx);
        pEnumerator->Release();
        pDevice->Release();
        pAudioClient->Release();
        pCaptureClient->Release();
        CoUninitialize();
        return;
    }

    std::cout << "Captura de audio iniciada en un hilo separado. Presiona Ctrl+C para detener." << std::endl;

    // Bucle principal de captura.
    while (true) {
        UINT32 numFramesInPacket = 0;
        BYTE* pData = NULL;
        DWORD flags = 0;

        hr = pCaptureClient->GetNextPacketSize(&numFramesInPacket);

        if (numFramesInPacket == 0) {
            Sleep(5);
            continue;
        }

        std::cout << "Se recibieron " << numFramesInPacket << " frames de audio." << std::endl;

        hr = pCaptureClient->GetBuffer(
            &pData,
            &numFramesInPacket,
            &flags,
            NULL,
            NULL);

        if (FAILED(hr)) {
            std::cerr << "Error: GetBuffer falló. HRESULT: " << hr << std::endl;
            // No se debe liberar un búfer que no se pudo obtener
            continue;
        }

        if (SUCCEEDED(hr) && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
            if (pwfx->wBitsPerSample == 32) {
                float* pFloatData = reinterpret_cast<float*>(pData);
                for (UINT32 i = 0; i < numFramesInPacket; ++i) {
                    accumulated_data.push_back(static_cast<double>(pFloatData[i]));
                }
            }
        }

        hr = pCaptureClient->ReleaseBuffer(numFramesInPacket);
        if (FAILED(hr)) {
            std::cerr << "Error: ReleaseBuffer falló. HRESULT: " << hr << std::endl;
            // Manejar este error si es posible, aunque es raro que falle
            continue;
        }

        std::cout << "Tamaño de accumulated_data antes del chequeo: " << accumulated_data.size() << std::endl;

        if (accumulated_data.size() >= FFT_SIZE) {
            std::cout << "Tamaño de accumulated_data suficiente. Copiando datos..." << std::endl;
            std::unique_lock<std::mutex> lock(sharedData.mtx);

            // Verificamos que el destino tenga el tamaño correcto antes de copiar.
            if (sharedData.in_data.size() != FFT_SIZE) {
                std::cerr << "Error: El tamaño del vector de destino no es correcto." << std::endl;
                // Manejo de error o redimensionar
                sharedData.in_data.resize(FFT_SIZE);
            }

            std::copy(accumulated_data.begin(), accumulated_data.begin() + FFT_SIZE, sharedData.in_data.begin());
            std::cout << "Datos copiados." << std::endl;

            std::cout << "Eliminando datos del búfer de acumulación..." << std::endl;
            accumulated_data.erase(accumulated_data.begin(), accumulated_data.begin() + FFT_SIZE);
            std::cout << "Datos eliminados. Nuevo tamaño: " << accumulated_data.size() << std::endl;

            lock.unlock();
            sharedData.cv.notify_one();
        }
    }

    // Liberar los objetos COM.
    CoTaskMemFree(pwfx);
    pEnumerator->Release();
    pDevice->Release();
    pAudioClient->Release();
    pCaptureClient->Release();
    CoUninitialize();
}
