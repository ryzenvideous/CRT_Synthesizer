/*Personal Use License  

Copyright (c) [2025] [Mr. Matthew Falzon]  

Permission is hereby granted, **only to the original recipient**, to use this  
software and associated documentation files (the "Software"), specifically the executable originally named "[crtnoise.exe/CRT_Synthesizer.exe]", **for personal or internal business purposes only**, subject to the following conditions:

1. **No Redistribution** – The Software **may not be copied, published, distributed, shared, or sublicensed** in any form, either for free or for payment, without express written permission from the copyright holder. This applies regardless of whether the Software is renamed or modified in any way.

2. **No Modification** – The Software **may not be modified, merged, decompiled, reverse-engineered, or used as a base for derivative works**. This applies regardless of the file name or any changes made to the Software.

3. **No Resale** – The Software **may not be sold, resold, rented, or otherwise monetized** in any form, as a standalone product or as part of another product or service, without express written permission from the copyright holder. This applies regardless of the file name or any changes made to the Software.

4. **Retention of Copyright Notice** – This copyright notice and license terms **must remain intact** in all authorized copies of the Software.

5. **No Warranty** – THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Windows.h>
#include <portaudio.h>
#include <cmath>
#include <vector>
#include <atomic>
#include <algorithm>
#include <commctrl.h>
#include <random>
#include <thread>
#include <shlwapi.h>
#include <windowsx.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")


const int SAMPLE_RATE = 44100;
const double NYQUIST_FREQ = SAMPLE_RATE / 2.0;
const int SCOPE_WIDTH = 300;
const int SCOPE_HEIGHT = 200;
const int SCOPE_SAMPLES = 500;


#define IDC_RADIO_15K        101
#define IDC_RADIO_PAL        102
#define IDC_RADIO_NTSC       103
#define IDC_YOKE_ONOFF       104
#define IDC_YOKE_SLIDER      105
#define IDC_MAINS_ONOFF      106
#define IDC_MAINS_SLIDER     107
#define IDC_RESONANCE_ONOFF  110
#define IDC_RESONANCE_SLIDER 111
#define IDC_NOISE_ONOFF      112
#define IDC_NOISE_SLIDER     113
#define IDC_THERMAL_ONOFF    114
#define IDC_THERMAL_SLIDER   115
#define IDC_PHASEDRIFT_ONOFF 116
#define IDC_PHASEDRIFT_SLIDER 117
#define IDC_SAVE_BUTTON      118
#define IDC_PRESET_TEXTBOX   119
#define IDC_LOAD_BUTTON      120
#define IDC_PRESET_DROPDOWN  121
#define IDC_VOLUME_SLIDER    122
#define IDC_HV_ARCING_ONOFF  123
#define IDC_HV_ARCING_SLIDER 124
#define IDC_HV_CORONA_ONOFF  125
#define IDC_HV_CORONA_SLIDER 126
#define IDC_HV_STATIC_ONOFF  127
#define IDC_HV_STATIC_SLIDER 128
#define IDC_AGING_ONOFF      129
#define IDC_AGING_SLIDER     130
#define IDC_VSYNC_HUM_ONOFF  131
#define IDC_VSYNC_HUM_SLIDER 132
#define IDC_HV_ARCING_FREQ_SLIDER 133
#define IDC_MUTE_BUTTON      134 


const wchar_t* REGISTRY_PATH = L"Software\\CRT_Synthesizer";


std::atomic<double> currentBaseFreq{ 15734.0 };
std::atomic<double> brightnessMod{ 0.0 };
std::atomic<double> baseModulation{ 0.5 };
std::atomic<double> transientModulation{ 0.0 };


std::atomic<double> yokeGain{ 0.183 };
std::atomic<bool> yokeEnabled{ true };
std::atomic<double> mainsGain{ 0.01 };
std::atomic<bool> mainsEnabled{ true };
std::atomic<double> resonanceGain{ 0.0544 };
std::atomic<bool> resonanceEnabled{ true };
std::atomic<double> noiseGain{ 0.0 };
std::atomic<bool> noiseEnabled{ true };
std::atomic<double> thermalGain{ 0.0025 };
std::atomic<bool> thermalEnabled{ false };
std::atomic<double> phaseDriftGain{ 0.2 };
std::atomic<bool> phaseDriftEnabled{ false };
std::atomic<double> hvArcingGain{ 1.2 };
std::atomic<bool> hvArcingEnabled{ true };
std::atomic<double> hvCoronaGain{ 0.06 };
std::atomic<bool> hvCoronaEnabled{ true };
std::atomic<double> hvStaticGain{ 0.5 };
std::atomic<bool> hvStaticEnabled{ true };
std::atomic<double> agingIntensity{ 2.0 };
std::atomic<bool> agingEnabled{ false };
std::atomic<double> vsyncHumGain{ 0.0857 };
std::atomic<bool> vsyncHumEnabled{ true };
std::atomic<double> globalVolume{ 2.0 };
std::atomic<double> hvArcingFreq{ 0.38 };
std::atomic<bool> isMuted{ false }; 


std::atomic<double> resonancePhaseDrift{ 0.0 };
std::atomic<double> thermalDrift{ 1.0 };
std::atomic<double> ageFactor{ 1.0 };


std::atomic<float> waveformBuffer[SCOPE_SAMPLES];
std::atomic<int> waveformIndex{ 0 };


HDC hScreenDC = GetDC(NULL);
HDC hMemDC = CreateCompatibleDC(hScreenDC);
HBITMAP hBitmap = NULL;
BITMAPINFOHEADER bmi = { 0 };


std::mt19937 gen(GetTickCount());
std::uniform_real_distribution<> dis(-1.0, 1.0);


static HWND hTooltip = NULL;


void SavePreset(HWND hWnd, const wchar_t* presetName);
void LoadPreset(HWND hWnd, const wchar_t* presetName);
void PopulatePresetDropdown(HWND hWnd);
void SaveSettings(HWND hWnd);
void LoadSettings(HWND hWnd);
void InitScreenCapture();
double CaptureBrightness();
void InitEffects();
void AddTooltip(HWND hWnd, HWND hControl, const wchar_t* text);
void DrawWaveform(HWND hWnd, HDC hdc, RECT& scopeRect);
static int AudioCallback(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);


void SavePreset(HWND hWnd, const wchar_t* presetName) {
    wchar_t filePath[MAX_PATH];
    wsprintfW(filePath, L"Presets\\%s.ini", presetName);
    CreateDirectoryW(L"Presets", NULL);

    WritePrivateProfileStringW(L"Settings", L"Frequency", std::to_wstring(currentBaseFreq.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"YokeEnabled", std::to_wstring(yokeEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"MainsEnabled", std::to_wstring(mainsEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"ResonanceEnabled", std::to_wstring(resonanceEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"NoiseEnabled", std::to_wstring(noiseEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"ThermalEnabled", std::to_wstring(thermalEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"PhaseDriftEnabled", std::to_wstring(phaseDriftEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVArcingEnabled", std::to_wstring(hvArcingEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVCoronaEnabled", std::to_wstring(hvCoronaEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVStaticEnabled", std::to_wstring(hvStaticEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"AgingEnabled", std::to_wstring(agingEnabled.load()).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"VSyncHumEnabled", std::to_wstring(vsyncHumEnabled.load()).c_str(), filePath);

    WritePrivateProfileStringW(L"Settings", L"YokeSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_YOKE_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"MainsSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_MAINS_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"ResonanceSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_RESONANCE_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"NoiseSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_NOISE_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"ThermalSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_THERMAL_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"PhaseDriftSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_PHASEDRIFT_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"VolumeSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_VOLUME_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVArcingSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVCoronaSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_HV_CORONA_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVStaticSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_HV_STATIC_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"AgingSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_AGING_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"VSyncHumSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_VSYNC_HUM_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
    WritePrivateProfileStringW(L"Settings", L"HVArcingFreqSliderPos", std::to_wstring(SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_FREQ_SLIDER), TBM_GETPOS, 0, 0)).c_str(), filePath);
}


void LoadPreset(HWND hWnd, const wchar_t* presetName) {
    wchar_t filePath[MAX_PATH];
    wsprintfW(filePath, L"Presets\\%s", presetName);

    currentBaseFreq.store(GetPrivateProfileIntW(L"Settings", L"Frequency", 15000, filePath));
    yokeEnabled.store(GetPrivateProfileIntW(L"Settings", L"YokeEnabled", 1, filePath) != 0);
    mainsEnabled.store(GetPrivateProfileIntW(L"Settings", L"MainsEnabled", 1, filePath) != 0);
    resonanceEnabled.store(GetPrivateProfileIntW(L"Settings", L"ResonanceEnabled", 1, filePath) != 0);
    noiseEnabled.store(GetPrivateProfileIntW(L"Settings", L"NoiseEnabled", 1, filePath) != 0);
    thermalEnabled.store(GetPrivateProfileIntW(L"Settings", L"ThermalEnabled", 1, filePath) != 0);
    phaseDriftEnabled.store(GetPrivateProfileIntW(L"Settings", L"PhaseDriftEnabled", 1, filePath) != 0);
    hvArcingEnabled.store(GetPrivateProfileIntW(L"Settings", L"HVArcingEnabled", 0, filePath) != 0);
    hvCoronaEnabled.store(GetPrivateProfileIntW(L"Settings", L"HVCoronaEnabled", 0, filePath) != 0);
    hvStaticEnabled.store(GetPrivateProfileIntW(L"Settings", L"HVStaticEnabled", 0, filePath) != 0);
    agingEnabled.store(GetPrivateProfileIntW(L"Settings", L"AgingEnabled", 0, filePath) != 0);
    vsyncHumEnabled.store(GetPrivateProfileIntW(L"Settings", L"VSyncHumEnabled", 0, filePath) != 0);

    auto updateSlider = [&](int sliderId, const wchar_t* key, int defaultPos, std::atomic<double>& param, double maxVal, bool isFreq = false) {
        HWND hSlider = GetDlgItem(hWnd, sliderId);
        int pos = GetPrivateProfileIntW(L"Settings", key, defaultPos, filePath);
        SendMessageW(hSlider, TBM_SETPOS, TRUE, pos);
        param.store((pos / 100.0) * (isFreq ? 1.0 : maxVal));
        SendMessageW(hWnd, WM_HSCROLL, MAKELONG(TB_THUMBPOSITION, pos), (LPARAM)hSlider);
        };

    updateSlider(IDC_YOKE_SLIDER, L"YokeSliderPos", 50, yokeGain, 0.3);
    updateSlider(IDC_MAINS_SLIDER, L"MainsSliderPos", 50, mainsGain, 0.2);
    updateSlider(IDC_RESONANCE_SLIDER, L"ResonanceSliderPos", 50, resonanceGain, 0.16);
    updateSlider(IDC_NOISE_SLIDER, L"NoiseSliderPos", 50, noiseGain, 0.2);
    updateSlider(IDC_THERMAL_SLIDER, L"ThermalSliderPos", 25, thermalGain, 0.01);
    updateSlider(IDC_PHASEDRIFT_SLIDER, L"PhaseDriftSliderPos", 50, phaseDriftGain, 0.4);
    updateSlider(IDC_VOLUME_SLIDER, L"VolumeSliderPos", 100, globalVolume, 1.0);
    updateSlider(IDC_HV_ARCING_SLIDER, L"HVArcingSliderPos", 50, hvArcingGain, 1.2);
    updateSlider(IDC_HV_ARCING_FREQ_SLIDER, L"HVArcingFreqSliderPos", 100, hvArcingFreq, 1.0, true);
    updateSlider(IDC_HV_CORONA_SLIDER, L"HVCoronaSliderPos", 50, hvCoronaGain, 0.3);
    updateSlider(IDC_HV_STATIC_SLIDER, L"HVStaticSliderPos", 50, hvStaticGain, 0.5);
    updateSlider(IDC_AGING_SLIDER, L"AgingSliderPos", 0, agingIntensity, 100.0);
    updateSlider(IDC_VSYNC_HUM_SLIDER, L"VSyncHumSliderPos", 50, vsyncHumGain, 0.0857);

    CheckDlgButton(hWnd, IDC_YOKE_ONOFF, yokeEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_MAINS_ONOFF, mainsEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_RESONANCE_ONOFF, resonanceEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_NOISE_ONOFF, noiseEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_THERMAL_ONOFF, thermalEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_PHASEDRIFT_ONOFF, phaseDriftEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_HV_ARCING_ONOFF, hvArcingEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_HV_CORONA_ONOFF, hvCoronaEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_HV_STATIC_ONOFF, hvStaticEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_AGING_ONOFF, agingEnabled.load() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hWnd, IDC_VSYNC_HUM_ONOFF, vsyncHumEnabled.load() ? BST_CHECKED : BST_UNCHECKED);

    switch (static_cast<int>(currentBaseFreq.load())) {
    case 15625: CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_PAL); break;
    case 15734: CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_NTSC); break;
    default:    CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_15K); break;
    }

    InvalidateRect(hWnd, NULL, TRUE);
    UpdateWindow(hWnd);
}


void PopulatePresetDropdown(HWND hWnd) {
    HWND hDropdown = GetDlgItem(hWnd, IDC_PRESET_DROPDOWN);
    SendMessageW(hDropdown, CB_RESETCONTENT, 0, 0);

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW(L"Presets\\*.ini", &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            SendMessageW(hDropdown, CB_ADDSTRING, 0, (LPARAM)findFileData.cFileName);
        } while (FindNextFileW(hFind, &findFileData) != 0);
        FindClose(hFind);
    }
}


void SaveSettings(HWND hWnd) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_PATH, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD freq = static_cast<DWORD>(currentBaseFreq.load());
        RegSetValueExW(hKey, L"Frequency", 0, REG_DWORD, (const BYTE*)&freq, sizeof(freq));

        DWORD yokeEnabledState = yokeEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"YokeEnabled", 0, REG_DWORD, (const BYTE*)&yokeEnabledState, sizeof(yokeEnabledState));
        DWORD mainsEnabledState = mainsEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"MainsEnabled", 0, REG_DWORD, (const BYTE*)&mainsEnabledState, sizeof(mainsEnabledState));
        DWORD resonanceEnabledState = resonanceEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"ResonanceEnabled", 0, REG_DWORD, (const BYTE*)&resonanceEnabledState, sizeof(resonanceEnabledState));
        DWORD noiseEnabledState = noiseEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"NoiseEnabled", 0, REG_DWORD, (const BYTE*)&noiseEnabledState, sizeof(noiseEnabledState));
        DWORD thermalEnabledState = thermalEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"ThermalEnabled", 0, REG_DWORD, (const BYTE*)&thermalEnabledState, sizeof(thermalEnabledState));
        DWORD phaseDriftEnabledState = phaseDriftEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"PhaseDriftEnabled", 0, REG_DWORD, (const BYTE*)&phaseDriftEnabledState, sizeof(phaseDriftEnabledState));
        DWORD hvArcingEnabledState = hvArcingEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"HVArcingEnabled", 0, REG_DWORD, (const BYTE*)&hvArcingEnabledState, sizeof(hvArcingEnabledState));
        DWORD hvCoronaEnabledState = hvCoronaEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"HVCoronaEnabled", 0, REG_DWORD, (const BYTE*)&hvCoronaEnabledState, sizeof(hvCoronaEnabledState));
        DWORD hvStaticEnabledState = hvStaticEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"HVStaticEnabled", 0, REG_DWORD, (const BYTE*)&hvStaticEnabledState, sizeof(hvStaticEnabledState));
        DWORD agingEnabledState = agingEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"AgingEnabled", 0, REG_DWORD, (const BYTE*)&agingEnabledState, sizeof(agingEnabledState));
        DWORD vsyncHumEnabledState = vsyncHumEnabled.load() ? 1 : 0;
        RegSetValueExW(hKey, L"VSyncHumEnabled", 0, REG_DWORD, (const BYTE*)&vsyncHumEnabledState, sizeof(vsyncHumEnabledState));

        DWORD yokeSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_YOKE_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"YokeSliderPos", 0, REG_DWORD, (const BYTE*)&yokeSliderPos, sizeof(yokeSliderPos));
        DWORD mainsSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_MAINS_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"MainsSliderPos", 0, REG_DWORD, (const BYTE*)&mainsSliderPos, sizeof(mainsSliderPos));
        DWORD resonanceSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_RESONANCE_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"ResonanceSliderPos", 0, REG_DWORD, (const BYTE*)&resonanceSliderPos, sizeof(resonanceSliderPos));
        DWORD noiseSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_NOISE_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"NoiseSliderPos", 0, REG_DWORD, (const BYTE*)&noiseSliderPos, sizeof(noiseSliderPos));
        DWORD thermalSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_THERMAL_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"ThermalSliderPos", 0, REG_DWORD, (const BYTE*)&thermalSliderPos, sizeof(thermalSliderPos));
        DWORD phaseDriftSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_PHASEDRIFT_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"PhaseDriftSliderPos", 0, REG_DWORD, (const BYTE*)&phaseDriftSliderPos, sizeof(phaseDriftSliderPos));
        DWORD volumeSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_VOLUME_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"VolumeSliderPos", 0, REG_DWORD, (const BYTE*)&volumeSliderPos, sizeof(volumeSliderPos));
        DWORD hvArcingSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"HVArcingSliderPos", 0, REG_DWORD, (const BYTE*)&hvArcingSliderPos, sizeof(hvArcingSliderPos));
        DWORD hvCoronaSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_HV_CORONA_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"HVCoronaSliderPos", 0, REG_DWORD, (const BYTE*)&hvCoronaSliderPos, sizeof(hvCoronaSliderPos));
        DWORD hvStaticSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_HV_STATIC_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"HVStaticSliderPos", 0, REG_DWORD, (const BYTE*)&hvStaticSliderPos, sizeof(hvStaticSliderPos));
        DWORD agingSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_AGING_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"AgingSliderPos", 0, REG_DWORD, (const BYTE*)&agingSliderPos, sizeof(agingSliderPos));
        DWORD vsyncHumSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_VSYNC_HUM_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"VSyncHumSliderPos", 0, REG_DWORD, (const BYTE*)&vsyncHumSliderPos, sizeof(vsyncHumSliderPos));
        DWORD hvArcingFreqSliderPos = SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_FREQ_SLIDER), TBM_GETPOS, 0, 0);
        RegSetValueExW(hKey, L"HVArcingFreqSliderPos", 0, REG_DWORD, (const BYTE*)&hvArcingFreqSliderPos, sizeof(hvArcingFreqSliderPos));

        RegCloseKey(hKey);
    }
}


void LoadSettings(HWND hWnd) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_PATH, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD freq = 15000;
        DWORD size = sizeof(freq);
        RegQueryValueExW(hKey, L"Frequency", NULL, NULL, (LPBYTE)&freq, &size);
        currentBaseFreq.store(static_cast<double>(freq));

        auto loadCheckBox = [&](const wchar_t* key, std::atomic<bool>& enabled, int ctrlId, bool defaultState) {
            DWORD state = defaultState ? 1 : 0;
            size = sizeof(state);
            if (RegQueryValueExW(hKey, key, NULL, NULL, (LPBYTE)&state, &size) == ERROR_SUCCESS) {
                enabled.store(state != 0);
                CheckDlgButton(hWnd, ctrlId, state ? BST_CHECKED : BST_UNCHECKED);
            }
            };

        loadCheckBox(L"YokeEnabled", yokeEnabled, IDC_YOKE_ONOFF, true);
        loadCheckBox(L"MainsEnabled", mainsEnabled, IDC_MAINS_ONOFF, true);
        loadCheckBox(L"ResonanceEnabled", resonanceEnabled, IDC_RESONANCE_ONOFF, true);
        loadCheckBox(L"NoiseEnabled", noiseEnabled, IDC_NOISE_ONOFF, true);
        loadCheckBox(L"ThermalEnabled", thermalEnabled, IDC_THERMAL_ONOFF, true);
        loadCheckBox(L"PhaseDriftEnabled", phaseDriftEnabled, IDC_PHASEDRIFT_ONOFF, true);
        loadCheckBox(L"HVArcingEnabled", hvArcingEnabled, IDC_HV_ARCING_ONOFF, false);
        loadCheckBox(L"HVCoronaEnabled", hvCoronaEnabled, IDC_HV_CORONA_ONOFF, false);
        loadCheckBox(L"HVStaticEnabled", hvStaticEnabled, IDC_HV_STATIC_ONOFF, false);
        loadCheckBox(L"AgingEnabled", agingEnabled, IDC_AGING_ONOFF, false);
        loadCheckBox(L"VSyncHumEnabled", vsyncHumEnabled, IDC_VSYNC_HUM_ONOFF, false);

        auto loadSlider = [&](const wchar_t* key, int ctrlId, std::atomic<double>& param, int defaultPos, double maxVal) {
            DWORD pos = defaultPos;
            size = sizeof(pos);
            if (RegQueryValueExW(hKey, key, NULL, NULL, (LPBYTE)&pos, &size) == ERROR_SUCCESS) {
                SendMessageW(GetDlgItem(hWnd, ctrlId), TBM_SETPOS, TRUE, pos);
                param.store((pos / 100.0) * maxVal);
            }
            };

        loadSlider(L"YokeSliderPos", IDC_YOKE_SLIDER, yokeGain, 50, 0.3);
        loadSlider(L"MainsSliderPos", IDC_MAINS_SLIDER, mainsGain, 50, 0.2);
        loadSlider(L"ResonanceSliderPos", IDC_RESONANCE_SLIDER, resonanceGain, 50, 0.16);
        loadSlider(L"NoiseSliderPos", IDC_NOISE_SLIDER, noiseGain, 50, 0.2);
        loadSlider(L"ThermalSliderPos", IDC_THERMAL_SLIDER, thermalGain, 25, 0.01);
        loadSlider(L"PhaseDriftSliderPos", IDC_PHASEDRIFT_SLIDER, phaseDriftGain, 50, 0.4);
        loadSlider(L"VolumeSliderPos", IDC_VOLUME_SLIDER, globalVolume, 100, 1.0);
        loadSlider(L"HVArcingSliderPos", IDC_HV_ARCING_SLIDER, hvArcingGain, 50, 1.2);
        loadSlider(L"HVCoronaSliderPos", IDC_HV_CORONA_SLIDER, hvCoronaGain, 50, 0.3);
        loadSlider(L"HVStaticSliderPos", IDC_HV_STATIC_SLIDER, hvStaticGain, 50, 0.5);
        loadSlider(L"AgingSliderPos", IDC_AGING_SLIDER, agingIntensity, 0, 100.0);
        loadSlider(L"VSyncHumSliderPos", IDC_VSYNC_HUM_SLIDER, vsyncHumGain, 50, 0.0857);
        loadSlider(L"HVArcingFreqSliderPos", IDC_HV_ARCING_FREQ_SLIDER, hvArcingFreq, 100, 1.0);

        RegCloseKey(hKey);
    }
}


void InitScreenCapture() {
    bmi.biSize = sizeof(BITMAPINFOHEADER);
    bmi.biWidth = GetSystemMetrics(SM_CXSCREEN) / 4;
    bmi.biHeight = -GetSystemMetrics(SM_CYSCREEN) / 4;
    bmi.biPlanes = 1;
    bmi.biBitCount = 32;
    bmi.biCompression = BI_RGB;

    hBitmap = CreateDIBSection(hMemDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, NULL, NULL, 0);
    SelectObject(hMemDC, hBitmap);
}


double CaptureBrightness() {
    BitBlt(hMemDC, 0, 0, bmi.biWidth, -bmi.biHeight, hScreenDC, 0, 0, SRCCOPY);

    DWORD* pixels;
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    pixels = (DWORD*)bmp.bmBits;

    double sum = 0.0, sumSquares = 0.0;
    const int sampleStep = 2;
    int count = 0;

    for (int y = 0; y < -bmi.biHeight; y += sampleStep) {
        for (int x = 0; x < bmi.biWidth; x += sampleStep) {
            DWORD pixel = pixels[y * bmi.biWidth + x];
            BYTE r = GetRValue(pixel);
            BYTE g = GetGValue(pixel);
            BYTE b = GetBValue(pixel);
            double luma = 0.299 * r + 0.587 * g + 0.114 * b;
            sum += luma;
            sumSquares += luma * luma;
            count++;
        }
    }

    double mean = sum / count;
    double variance = (sumSquares / count) - (mean * mean);
    double stdDev = sqrt(std::max(variance, 0.0));
    return (mean / 255.0 * 0.4) + (stdDev / 255.0 * 0.6);
}


void InitEffects() {
    std::thread([] {
        while (true) {
            if (thermalEnabled.load()) {
                thermalDrift.store(1.0 + thermalGain.load() * dis(gen) + 0.005 * sin(GetTickCount64() * 0.001));
            }
            Sleep(100);
        }
        }).detach();
}


void AddTooltip(HWND hWnd, HWND hControl, const wchar_t* text) {
    if (!hTooltip) {
        hTooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            hWnd, NULL, GetModuleHandle(NULL), NULL);
        if (!hTooltip) {
            MessageBoxW(hWnd, L"Failed to create tooltip control!", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        SendMessageW(hTooltip, TTM_ACTIVATE, TRUE, 0);
        SendMessageW(hTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELONG(500, 0));
    }

    TOOLINFOW ti = { 0 };
    ti.cbSize = sizeof(TOOLINFOW);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = hWnd;
    ti.uId = (UINT_PTR)hControl;
    ti.lpszText = (LPWSTR)text;
    SendMessageW(hTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}


void DrawWaveform(HWND hWnd, HDC hdc, RECT& scopeRect) {
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, SCOPE_WIDTH, SCOPE_HEIGHT);
    SelectObject(hMemDC, hBmp);

    RECT fillRect = { 0, 0, SCOPE_WIDTH, SCOPE_HEIGHT };
    FillRect(hMemDC, &fillRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    SelectObject(hMemDC, gridPen);
    for (int x = 0; x < SCOPE_WIDTH; x += 50) {
        MoveToEx(hMemDC, x, 0, NULL);
        LineTo(hMemDC, x, SCOPE_HEIGHT);
    }
    for (int y = 0; y < SCOPE_HEIGHT; y += 50) {
        MoveToEx(hMemDC, 0, y, NULL);
        LineTo(hMemDC, SCOPE_WIDTH, y);
    }

    HPEN wavePen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
    SelectObject(hMemDC, wavePen);

    int currentIndex = waveformIndex.load();
    int startPos = (currentIndex + 1) % SCOPE_SAMPLES;

    for (int i = 0; i < SCOPE_SAMPLES; i++) {
        int idx = (startPos + i) % SCOPE_SAMPLES;
        float sample = waveformBuffer[idx].load();

        int x = static_cast<int>((static_cast<float>(i) / SCOPE_SAMPLES) * SCOPE_WIDTH);
        int y = static_cast<int>((1.0f - (sample * 0.5f + 0.5f)) * SCOPE_HEIGHT);

        if (i == 0) MoveToEx(hMemDC, x, y, NULL);
        else LineTo(hMemDC, x, y);
    }

    BitBlt(hdc, scopeRect.left, scopeRect.top, SCOPE_WIDTH, SCOPE_HEIGHT, hMemDC, 0, 0, SRCCOPY);

    DeleteObject(gridPen);
    DeleteObject(wavePen);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
}


static int AudioCallback(const void*, void* outputBuffer, unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*) {

    float* out = static_cast<float*>(outputBuffer);
    static double phase = 0.0;
    static double smoothedFreq = currentBaseFreq.load();
    static double thermalMass = 1.0;
    static double ageFactorStatic = 1.0;

    struct ResonanceBank {
        double phase = 0.0;
        double amplitude = 0.0;
        double freqMultiplier = 1.0;
    };

    static ResonanceBank resonanceBanks[3] = {
        {0.0, 0.8, 7.0},
        {0.0, 0.3, 23.0},
        {0.0, 0.1, 3.83}
    };

    const double yokeFreq = (currentBaseFreq > 15700) ? 59.94 : 50.0;
    static double yokePhase = 0.0;
    static double mainsPhase = 0.0;
    static double arcingTimer = 0.0;
    static double arcingDecay = 0.0;
    static bool arcingTriggered = false;
    static double coronaHPF = 0.0;
    static double coronaPhase = 0.0;
    static double vsyncPhase = 0.0;
    double vsyncFreq = (currentBaseFreq > 15700) ? 59.94 : 50.0;
    static double noiseHPF = 0.0;
    static double staticTimer = 0.0;
    static double staticBurst = 0.0;
    static double staticLPF = 0.0;

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        if (!agingEnabled.load()) {
            ageFactorStatic = 1.0;
            thermalDrift.store(1.0);
            ageFactor.store(1.0);
        }
        else {
            ageFactorStatic *= 1.0 - (dis(gen) * 5e-8 * agingIntensity.load());
            ageFactor.store(ageFactorStatic);
        }

        double beamCurrent = brightnessMod.load();
        double saturation = 1.0 / (1.0 + exp(-10 * (beamCurrent - 0.5)));
        double transientMod = transientModulation.load();
        double mod = (baseModulation.load() * 0.25 + transientMod * 0.75) * (1.0 - 0.1 * saturation);
        if (fabs(transientMod) > 0.1) mod += 0.001 * dis(gen);

        yokePhase += yokeFreq / SAMPLE_RATE;
        if (yokePhase >= 1.0) yokePhase -= 1.0;
        double yokeDamping = exp(-yokePhase * 0.1);
        double yokeMod = yokeEnabled.load() ? yokeGain.load() * yokeDamping * (
            sin(yokePhase * 2 * M_PI) + 0.07 * sin(3 * yokePhase * 2 * M_PI) + 0.03 * sin(yokePhase * currentBaseFreq * 0.0001)
            ) : 0.0;

        mainsPhase += (currentBaseFreq > 15700) ? 60.0 / 44100 : 50.0 / 44100;
        double mainsHum = mainsEnabled.load() ? mainsGain.load() * (
            sin(mainsPhase * 2 * M_PI * 100) + 0.3 * fmod(mainsPhase * 200, 1.0)
            ) * (1.0 + 0.1 * currentBaseFreq.load() / 15000) : 0.0;

        double resonance = 0.0;
        for (auto& bank : resonanceBanks) {
            bank.phase += (currentBaseFreq * bank.freqMultiplier * thermalMass) / SAMPLE_RATE;
            bank.amplitude *= 0.999;
            resonance += bank.amplitude * sin(bank.phase * 2 * M_PI);
            if (bank.amplitude < 0.01) {
                bank.amplitude = 0.8 + 0.2 * dis(gen);
                bank.phase = 0.0;
            }
        }
        if (phaseDriftEnabled.load()) {
            double currentDrift = resonancePhaseDrift.load();
            currentDrift += phaseDriftGain.load() * dis(gen) * 0.05;
            resonancePhaseDrift.store(currentDrift);
            resonance += sin(resonancePhaseDrift.load() * 2 * M_PI) * 0.6;
        }
        resonance *= resonanceEnabled.load() ? resonanceGain.load() : 0.0;

        double hvEffects = 0.0;
        if (hvArcingEnabled.load()) {
            int freqThreshold = static_cast<int>(5000 + (45000 * (1.0 - hvArcingFreq.load())));
            if ((rand() % freqThreshold) < 1) {
                arcingTimer = 0.02 + dis(gen) * 0.04;
                arcingDecay = 1.0;
                arcingTriggered = true;
            }
            if (arcingTimer > 0) {
                double arcingNoise = hvArcingGain.load() * arcingDecay * dis(gen);
                if (arcingTriggered) {
                    hvEffects += hvArcingGain.load() * 0.8;
                    arcingTriggered = false;
                }
                else {
                    hvEffects += arcingNoise;
                }
                arcingDecay *= 0.92;
                arcingTimer -= 1.0 / SAMPLE_RATE;
            }
        }
        if (hvCoronaEnabled.load()) {
            double rawNoise = dis(gen);
            coronaHPF = 0.7 * coronaHPF + 0.3 * rawNoise;
            coronaPhase += 8000.0 / SAMPLE_RATE;
            if (coronaPhase >= 1.0) coronaPhase -= 1.0;
            double buzz = sin(coronaPhase * 2 * M_PI) * 0.2;
            hvEffects += hvCoronaGain.load() * (0.6 * coronaHPF + 0.4 * buzz);
        }
        if (hvStaticEnabled.load()) {
            staticTimer -= 1.0 / SAMPLE_RATE;
            if (staticTimer <= 0) {
                staticBurst = dis(gen) * (0.3 + 0.7 * brightnessMod.load());
                staticTimer = 0.01 + dis(gen) * 0.02;
            }
            double rawStatic = hvStaticGain.load() * staticBurst;
            staticLPF = 0.8 * staticLPF + 0.2 * rawStatic;
            hvEffects += rawStatic - (0.5 * staticLPF);
            staticBurst *= 0.98;
        }

        double noise = noiseEnabled.load() ? noiseGain.load() * dis(gen) : 0.0;
        noiseHPF = 0.9 * noiseHPF + 0.1 * noise;

        double vsyncHum = 0.0;
        if (vsyncHumEnabled.load()) {
            vsyncHum = vsyncHumGain.load() * (
                0.7 * sin(vsyncPhase * 2 * M_PI) +
                0.2 * sin(2 * vsyncPhase * 2 * M_PI) +
                0.1 * sin(4 * vsyncPhase * 2 * M_PI)
                );
            vsyncPhase += vsyncFreq / SAMPLE_RATE;
            if (vsyncPhase >= 1.0) vsyncPhase -= 1.0;
        }

        double agingNoise = 0.0;
        static double thermalStressCounter = 0.0;
        if (agingEnabled.load()) {
            double ageLinear = agingIntensity.load() / 100.0;
            double age = pow(ageLinear, 1.5);
            agingNoise = age * 0.002 * dis(gen);
            thermalStressCounter += age * 0.02;
            if (thermalStressCounter > 1.0) {
                thermalStressCounter = 0.0;
                thermalDrift.store(0.98 + 0.02 * dis(gen));
            }
        }

        double totalMod = mod + yokeMod + mainsHum + resonance + noiseHPF + hvEffects + vsyncHum + agingNoise;

        double targetFreq = currentBaseFreq.load() * thermalMass * (1.0 + totalMod * 0.45) * ageFactor.load() * thermalDrift.load();
        smoothedFreq = smoothedFreq * 0.95 + targetFreq * 0.05;
        double freq = std::clamp(smoothedFreq, 1000.0, NYQUIST_FREQ);

        float sample = static_cast<float>(
            sin(phase * 2.0 * M_PI) * (1.0 - vsyncHumGain.load() * 0.2) +
            vsyncHum * 0.5
            );
        phase += freq / SAMPLE_RATE;
        if (phase >= 1.0) phase -= 1.0;

        sample *= static_cast<float>(globalVolume.load());
        if (isMuted.load()) sample = 0.0f; 

        int idx = waveformIndex.fetch_add(1) % SCOPE_SAMPLES;
        waveformBuffer[idx].store(sample);

        *out++ = sample * 0.9f;
        *out++ = sample * 0.9f;
    }
    return paContinue;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hdcMem = NULL;
    static HBITMAP hbmMem = NULL;
    static RECT scopeRect;

    switch (msg) {
    case WM_CREATE: {
        HDC hdc = GetDC(hWnd);
        hdcMem = CreateCompatibleDC(hdc);
        RECT rc;
        GetClientRect(hWnd, &rc);
        hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        SelectObject(hdcMem, hbmMem);
        ReleaseDC(hWnd, hdc);

        scopeRect.left = rc.right - SCOPE_WIDTH - 20;
        scopeRect.top = 20;
        scopeRect.right = scopeRect.left + SCOPE_WIDTH;
        scopeRect.bottom = scopeRect.top + SCOPE_HEIGHT;

        HINSTANCE hInst = GetModuleHandle(NULL);

        CreateWindowW(L"BUTTON", L"Save", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 250, 10, 80, 25, hWnd, (HMENU)IDC_SAVE_BUTTON, hInst, NULL);
        CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 340, 10, 150, 25, hWnd, (HMENU)IDC_PRESET_TEXTBOX, hInst, NULL);
        CreateWindowW(L"BUTTON", L"Load", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 250, 40, 80, 25, hWnd, (HMENU)IDC_LOAD_BUTTON, hInst, NULL);
        CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 340, 40, 150, 100, hWnd, (HMENU)IDC_PRESET_DROPDOWN, hInst, NULL);

        PopulatePresetDropdown(hWnd);

        CreateWindowW(L"BUTTON", L"15,000 Hz", WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON | WS_GROUP, 10, 10, 150, 25, hWnd, (HMENU)IDC_RADIO_15K, hInst, NULL);
        CreateWindowW(L"BUTTON", L"15,625 Hz (PAL)", WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON, 10, 40, 150, 25, hWnd, (HMENU)IDC_RADIO_PAL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"15,734 Hz (NTSC)", WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON, 10, 70, 150, 25, hWnd, (HMENU)IDC_RADIO_NTSC, hInst, NULL);

        int ypos = 110;
        const int row_height = 30;
        const int static_width = 100;
        const int checkbox_x = 120;
        const int slider_x = 150;
        const int slider_right_edge = 490;
        const int primary_slider_width = 170;
        const int slider2_x = slider_x + primary_slider_width + 10;
        const int unified_slider_width = slider_right_edge - slider_x;

        CreateWindowW(L"STATIC", L"Yoke:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_YOKE_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_YOKE_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Mains:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_MAINS_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_MAINS_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Resonance:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_RESONANCE_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_RESONANCE_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Noise:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_NOISE_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_NOISE_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Thermal:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_THERMAL_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_THERMAL_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Phase Drift:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_PHASEDRIFT_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_PHASEDRIFT_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"HV Arcing:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_HV_ARCING_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, primary_slider_width, 20, hWnd, (HMENU)IDC_HV_ARCING_SLIDER, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider2_x, ypos, unified_slider_width - primary_slider_width - 10, 20, hWnd, (HMENU)IDC_HV_ARCING_FREQ_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"HV Corona:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_HV_CORONA_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_HV_CORONA_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"HV Static:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_HV_STATIC_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_HV_STATIC_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"Aging:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_AGING_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_AGING_SLIDER, hInst, NULL);
        ypos += row_height;

        CreateWindowW(L"STATIC", L"VSync Hum:", WS_VISIBLE | WS_CHILD, 10, ypos, static_width, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkbox_x, ypos, 20, 20, hWnd, (HMENU)IDC_VSYNC_HUM_ONOFF, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, slider_x, ypos, unified_slider_width, 20, hWnd, (HMENU)IDC_VSYNC_HUM_SLIDER, hInst, NULL);
        ypos += row_height;

        int windowWidth = 900;
        int volumeSliderWidth = unified_slider_width;
        int volumeSliderX = (windowWidth - SCOPE_WIDTH - 20 - volumeSliderWidth) / 2;
        int muteButtonSize = 20;
        int muteButtonX = volumeSliderX - 50 - muteButtonSize - 5; 
        CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, muteButtonX, ypos, muteButtonSize, muteButtonSize, hWnd, (HMENU)IDC_MUTE_BUTTON, hInst, NULL);
        CreateWindowW(L"STATIC", L"Volume:", WS_VISIBLE | WS_CHILD, volumeSliderX - 50, ypos, 100, 20, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"msctls_trackbar32", L"", WS_VISIBLE | WS_CHILD | TBS_HORZ, volumeSliderX, ypos, volumeSliderWidth, 20, hWnd, (HMENU)IDC_VOLUME_SLIDER, hInst, NULL);

        CreateWindowW(L"STATIC", L"\u00A9 Mr. Matthew Falzon 2025", WS_VISIBLE | WS_CHILD, windowWidth - 200, 450, 180, 20, hWnd, NULL, hInst, NULL);

        SendMessageW(GetDlgItem(hWnd, IDC_YOKE_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_MAINS_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_RESONANCE_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_NOISE_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_THERMAL_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_PHASEDRIFT_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_HV_ARCING_FREQ_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_HV_CORONA_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_HV_STATIC_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_AGING_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_VSYNC_HUM_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendMessageW(GetDlgItem(hWnd, IDC_VOLUME_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));

        LoadSettings(hWnd);

        switch (static_cast<int>(currentBaseFreq.load())) {
        case 15625: CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_PAL); break;
        case 15734: CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_NTSC); break;
        default:    CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, IDC_RADIO_15K); break;
        }

        SetTimer(hWnd, 2, 100, NULL);
        return 0;
    }
    case WM_TIMER:
        if (wParam == 1) {
            InvalidateRect(hWnd, &scopeRect, FALSE);
            UpdateWindow(hWnd);
        }
        else if (wParam == 2) {
            KillTimer(hWnd, 2);
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_YOKE_SLIDER), L"Controls yoke coil hum intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_MAINS_SLIDER), L"Controls mains power hum intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RESONANCE_SLIDER), L"Controls resonance harmonic intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_NOISE_SLIDER), L"Controls background noise intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_THERMAL_SLIDER), L"Controls thermal drift intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_PHASEDRIFT_SLIDER), L"Controls phase drift intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_HV_ARCING_SLIDER), L"Controls high-voltage arcing intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_HV_ARCING_FREQ_SLIDER), L"Controls arcing frequency (1/50s to 1/5s)");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_HV_CORONA_SLIDER), L"Controls corona discharge buzz intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_HV_STATIC_SLIDER), L"Controls static burst intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_AGING_SLIDER), L"Controls CRT aging noise and drift");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_VSYNC_HUM_SLIDER), L"Controls vertical sync hum intensity");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_VOLUME_SLIDER), L"Controls overall audio volume");
            AddTooltip(hWnd, GetDlgItem(hWnd, IDC_MUTE_BUTTON), L"Toggles audio mute");
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdcMem, &rc, (HBRUSH)(COLOR_WINDOW + 1));
        DrawWaveform(hWnd, hdcMem, scopeRect);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == IDC_MUTE_BUTTON) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;

            
            FillRect(hdc, &rc, (HBRUSH)(COLOR_BTNFACE + 1));

           
            DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT);

            
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            SelectObject(hdc, hPen);
            SelectObject(hdc, hBrush);

            POINT speaker[5] = {
                {rc.left + 4, rc.top + rc.bottom / 2},         
                {rc.left + 8, rc.top + 4},                     
                {rc.left + 12, rc.top + 4},                    
                {rc.left + 12, rc.bottom - 4},                 
                {rc.left + 8, rc.bottom - 4}                   
            };
            Polygon(hdc, speaker, 5);

            
            if (!isMuted.load()) {
                MoveToEx(hdc, rc.left + 14, rc.top + 6, NULL);
                LineTo(hdc, rc.left + 16, rc.top + 6);
                MoveToEx(hdc, rc.left + 14, rc.top + rc.bottom / 2, NULL);
                LineTo(hdc, rc.left + 16, rc.top + rc.bottom / 2);
                MoveToEx(hdc, rc.left + 14, rc.bottom - 6, NULL);
                LineTo(hdc, rc.left + 16, rc.bottom - 6);
            }
            else {
                
                HPEN hRedPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                SelectObject(hdc, hRedPen);
                MoveToEx(hdc, rc.left + 4, rc.bottom - 4, NULL);
                LineTo(hdc, rc.right - 4, rc.top + 4);
                DeleteObject(hRedPen);
            }

            DeleteObject(hPen);
            DeleteObject(hBrush);
            return TRUE;
        }
        return FALSE;
    }
    case WM_MOUSEMOVE:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    case WM_COMMAND: {
        if (HIWORD(wParam) == BN_CLICKED) {
            const int ctrlId = LOWORD(wParam);
            if (ctrlId == IDC_MUTE_BUTTON) {
                isMuted.store(!isMuted.load());
                InvalidateRect(hWnd, NULL, TRUE);
                return 0;
            }
            if (ctrlId == IDC_SAVE_BUTTON) {
                wchar_t presetName[256];
                GetWindowTextW(GetDlgItem(hWnd, IDC_PRESET_TEXTBOX), presetName, 256);
                if (wcslen(presetName) > 0) {
                    SavePreset(hWnd, presetName);
                    PopulatePresetDropdown(hWnd);
                    SetWindowTextW(GetDlgItem(hWnd, IDC_PRESET_TEXTBOX), L"");
                }
            }
            if (ctrlId == IDC_LOAD_BUTTON) {
                HWND hDropdown = GetDlgItem(hWnd, IDC_PRESET_DROPDOWN);
                int index = SendMessageW(hDropdown, CB_GETCURSEL, 0, 0);
                if (index != CB_ERR) {
                    wchar_t presetName[256];
                    SendMessageW(hDropdown, CB_GETLBTEXT, index, (LPARAM)presetName);
                    LoadPreset(hWnd, presetName);
                }
            }
            switch (ctrlId) {
            case IDC_RADIO_15K:
            case IDC_RADIO_PAL:
            case IDC_RADIO_NTSC:
                CheckRadioButton(hWnd, IDC_RADIO_15K, IDC_RADIO_NTSC, ctrlId);
                currentBaseFreq.store(
                    ctrlId == IDC_RADIO_15K ? 15000.0 :
                    ctrlId == IDC_RADIO_PAL ? 15625.0 : 15734.0
                );
                break;
            case IDC_YOKE_ONOFF:
                yokeEnabled.store(IsDlgButtonChecked(hWnd, IDC_YOKE_ONOFF) == BST_CHECKED);
                break;
            case IDC_MAINS_ONOFF:
                mainsEnabled.store(IsDlgButtonChecked(hWnd, IDC_MAINS_ONOFF) == BST_CHECKED);
                break;
            case IDC_RESONANCE_ONOFF:
                resonanceEnabled.store(IsDlgButtonChecked(hWnd, IDC_RESONANCE_ONOFF) == BST_CHECKED);
                break;
            case IDC_NOISE_ONOFF:
                noiseEnabled.store(IsDlgButtonChecked(hWnd, IDC_NOISE_ONOFF) == BST_CHECKED);
                break;
            case IDC_THERMAL_ONOFF:
                thermalEnabled.store(IsDlgButtonChecked(hWnd, IDC_THERMAL_ONOFF) == BST_CHECKED);
                break;
            case IDC_PHASEDRIFT_ONOFF:
                phaseDriftEnabled.store(IsDlgButtonChecked(hWnd, IDC_PHASEDRIFT_ONOFF) == BST_CHECKED);
                break;
            case IDC_HV_ARCING_ONOFF:
                hvArcingEnabled.store(IsDlgButtonChecked(hWnd, IDC_HV_ARCING_ONOFF) == BST_CHECKED);
                break;
            case IDC_HV_CORONA_ONOFF:
                hvCoronaEnabled.store(IsDlgButtonChecked(hWnd, IDC_HV_CORONA_ONOFF) == BST_CHECKED);
                break;
            case IDC_HV_STATIC_ONOFF:
                hvStaticEnabled.store(IsDlgButtonChecked(hWnd, IDC_HV_STATIC_ONOFF) == BST_CHECKED);
                break;
            case IDC_AGING_ONOFF:
                agingEnabled.store(IsDlgButtonChecked(hWnd, IDC_AGING_ONOFF) == BST_CHECKED);
                break;
            case IDC_VSYNC_HUM_ONOFF:
                vsyncHumEnabled.store(IsDlgButtonChecked(hWnd, IDC_VSYNC_HUM_ONOFF) == BST_CHECKED);
                break;
            }
        }
        return 0;
    }
    case WM_HSCROLL: {
        HWND hSlider = (HWND)lParam;
        int pos = SendMessageW(hSlider, TBM_GETPOS, 0, 0);

        if (hSlider == GetDlgItem(hWnd, IDC_YOKE_SLIDER)) {
            yokeGain.store((pos / 100.0) * 0.3);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_MAINS_SLIDER)) {
            mainsGain.store((pos / 100.0) * 0.2);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_RESONANCE_SLIDER)) {
            resonanceGain.store((pos / 100.0) * 0.16);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_NOISE_SLIDER)) {
            noiseGain.store((pos / 100.0) * 0.2);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_THERMAL_SLIDER)) {
            thermalGain.store((pos / 100.0) * 0.01);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_PHASEDRIFT_SLIDER)) {
            phaseDriftGain.store((pos / 100.0) * 0.4);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_HV_ARCING_SLIDER)) {
            hvArcingGain.store((pos / 100.0) * 1.2);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_HV_ARCING_FREQ_SLIDER)) {
            hvArcingFreq.store(pos / 100.0);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_HV_CORONA_SLIDER)) {
            hvCoronaGain.store((pos / 100.0) * 0.3);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_HV_STATIC_SLIDER)) {
            hvStaticGain.store((pos / 100.0) * 0.5);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_AGING_SLIDER)) {
            agingIntensity.store(pos);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_VSYNC_HUM_SLIDER)) {
            vsyncHumGain.store((pos / 100.0) * 0.0857);
        }
        else if (hSlider == GetDlgItem(hWnd, IDC_VOLUME_SLIDER)) {
            globalVolume.store(pos / 100.0);
        }
        return 0;
    }
    case WM_CLOSE:
        SaveSettings(hWnd);
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        DeleteDC(hdcMem);
        DeleteObject(hbmMem);
        if (hTooltip) DestroyWindow(hTooltip);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}


int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CRT_SynthesizerWindowClass";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"CRT_SynthesizerWindowClass", L"CRT_Synthesizer",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 500, NULL, NULL, hInst, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    InitScreenCapture();
    InitEffects();

    Pa_Initialize();
    PaStream* stream;
    Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, AudioCallback, nullptr);
    Pa_StartStream(stream);

    SetTimer(hWnd, 1, 33, NULL);

    MSG msg = { 0 };
    double baseAvg = 0.5;
    double prevBrightness = 0.5;

    while (true) {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else {
            double current = CaptureBrightness();
            baseAvg = baseAvg * 0.92 + current * 0.08;
            baseModulation.store(baseAvg);

            double delta = current - prevBrightness;
            prevBrightness = current;
            static double smoothedDelta = 0.0;
            smoothedDelta = smoothedDelta * 0.6 + delta * 0.4;
            transientModulation.store(smoothedDelta * 2.5);

            Sleep(1);
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
    DeleteObject(hBitmap);

    return (int)msg.wParam;
}
