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

    // Buffer temporal para evitar la reasignación de memoria.
    std::vector<double> local_buffer(FFT_SIZE);
    int buffer_position = 0;

    // 1. Inicializar la biblioteca COM para el hilo.
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Error: CoInitializeEx fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 2. Crear el enumerador de dispositivos.
    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        (void**)&pEnumerator
    );
    if (FAILED(hr)) {
        std::cerr << "Error: CoCreateInstance fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 3. Obtener el dispositivo de captura de audio por defecto.
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Error: GetDefaultAudioEndpoint fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 4. Activar la interfaz de cliente de audio.
    hr = pDevice->Activate(
        IID_IAudioClient,
        CLSCTX_ALL,
        NULL,
        (void**)&pAudioClient
    );
    if (FAILED(hr)) {
        std::cerr << "Error: Activar IAudioClient fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 5. Obtener el formato de audio soportado.
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Error: GetMixFormat fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 6. Inicializar el cliente de audio con el formato de mezcla.
    // Usamos AUDCLNT_STREAMFLAGS_LOOPBACK para capturar el audio que se está reproduciendo.
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        pwfx,
        NULL
    );
    if (FAILED(hr)) {
        std::cerr << "Error: Inicializar IAudioClient fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 7. Obtener la interfaz de captura de audio.
    hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
    if (FAILED(hr)) {
        std::cerr << "Error: Obtener servicio IAudioCaptureClient fallido, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // 8. Iniciar el motor de audio.
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Error: Fallo al iniciar el motor de audio, hr = 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    std::cout << "Hilo de captura de audio iniciado." << std::endl;

    // Bucle principal de captura.
    while (true) {
        UINT32 numFramesInPacket = 0;
        hr = pCaptureClient->GetNextPacketSize(&numFramesInPacket);

        if (numFramesInPacket == 0) {
            continue;
        }

        BYTE* pData;
        UINT32 numFramesToRead;
        DWORD dwFlags;

        hr = pCaptureClient->GetBuffer(&pData, &numFramesToRead, &dwFlags, NULL, NULL);
        if (FAILED(hr)) {
            continue;
        }

        if (!(dwFlags & AUDCLNT_BUFFERFLAGS_SILENT)) {
            const float* pFloatData = reinterpret_cast<const float*>(pData);
            for (UINT32 i = 0; i < numFramesToRead; ++i) {
                if (buffer_position < FFT_SIZE) {
                    local_buffer[buffer_position++] = static_cast<double>(pFloatData[i]);
                }
                else {
                    break;
                }
            }
        }

        pCaptureClient->ReleaseBuffer(numFramesToRead);

        if (buffer_position >= FFT_SIZE) {
            std::unique_lock<std::mutex> lock(sharedData.mtx);
            std::copy(local_buffer.begin(), local_buffer.begin() + FFT_SIZE, sharedData.in_data.begin());
            lock.unlock();

            sharedData.cv.notify_one();
            buffer_position = 0;
        }
    }

cleanup:
    // Sección de limpieza centralizada.
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    CoTaskMemFree(pwfx);
    CoUninitialize();
}
