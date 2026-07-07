#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>

class Logger {
public:
    static std::string GetLogPath() {
        char path[MAX_PATH];
        HMODULE hModule = NULL;
        
        // 1. Try to get the path of the current DLL
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&GetLogPath, &hModule)) {
            if (GetModuleFileNameA(hModule, path, MAX_PATH)) {
                std::string p(path);
                size_t pos = p.find_last_of("\\/");
                std::string logPath = p.substr(0, pos) + "\\speedometer.log";
                
                // Check if we can write here
                std::ofstream test(logPath, std::ios_base::app);
                if (test.is_open()) {
                    test.close();
                    return logPath;
                }
            }
        }

        // 2. Fallback to Game Directory
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            std::string p(path);
            size_t pos = p.find_last_of("\\/");
            std::string logPath = p.substr(0, pos) + "\\speedometer.log";
            
            std::ofstream test(logPath, std::ios_base::app);
            if (test.is_open()) {
                test.close();
                return logPath;
            }
        }

        // 3. Last resort: TEMP folder (Guaranteed writable)
        char tempPath[MAX_PATH];
        if (GetTempPathA(MAX_PATH, tempPath)) {
            return std::string(tempPath) + "speedometer_mod.log";
        }

        return "speedometer.log";
    }

    static void Log(const std::string& message) {
        std::ofstream logFile(GetLogPath(), std::ios_base::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            struct tm window_time;
            localtime_s(&window_time, &time);

            logFile << "[" << std::put_time(&window_time, "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
        }
    }

    static void Clear() {
        std::ofstream logFile(GetLogPath(), std::ios_base::trunc);
    }
};
