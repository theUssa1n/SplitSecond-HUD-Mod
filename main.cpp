// ---------------------------------------------------------
// MAIN ENTRY POINT
// ---------------------------------------------------------
#include <windows.h>
#include "d3d9_hook.h"
#include "logger.h"

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        
        Logger::Clear();
        Logger::Log("--- Speedometer Mod Attached ---");
        Logger::Log("Log File Path: " + Logger::GetLogPath());
        
        char modulePath[MAX_PATH];
        GetModuleFileNameA(GetModuleHandleA(nullptr), modulePath, MAX_PATH);
        Logger::Log("Process Path: " + std::string(modulePath));
        Logger::Log("Process Base Address: " + std::to_string((uintptr_t)GetModuleHandleA(nullptr)));
        
        char dllPath[MAX_PATH];
        GetModuleFileNameA(hModule, dllPath, MAX_PATH);
        Logger::Log("Mod DLL Path: " + std::string(dllPath));
        Logger::Log("Mod Base Address: " + std::to_string((uintptr_t)hModule));
        
        // Check if we are in the right process
        std::string procName = std::string(modulePath);
        if (procName.find("SplitSecond.exe") == std::string::npos) {
            Logger::Log("Warning: Mod attached to unexpected process: " + procName);
        }

        // Create a thread to initialize our hook safely
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)D3D9Hook::Initialize, hModule, 0, nullptr);
        if (hThread) {
            Logger::Log("Initialization thread created successfully.");
            CloseHandle(hThread);
        } else {
            Logger::Log("Failed to create initialization thread! Error: " + std::to_string(GetLastError()));
        }
        break;
    }
        
    case DLL_PROCESS_DETACH:
        D3D9Hook::Shutdown();
        break;
    }
    return TRUE;
}
