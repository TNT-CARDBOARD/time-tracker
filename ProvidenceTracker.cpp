#pragma comment(lib, "Psapi.lib")
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

using namespace std;
const char CYPHER_KEY = 'X';
const string SAVE_FILE = "system_data.dat";

// черный список
set<string> BLACKLIST = { "SearchApp", "svchost", "csrss", "System", "Idle", "Registry", "smss" };

// структура данных
struct AppStats {
    long long totalSeconds = 0;
    bool isVisible = true;
};

map<string, AppStats> trackerData;
bool isTracking = true;

// шифрование/расшифровка строк (XOR)
string Cipher(string data) {
    string output = data;
    for (int i = 0; i < data.size(); i++)
        output[i] = data[i] ^ CYPHER_KEY;
    return output;
}

// добавление в реестр
void SetAutoStart(bool enable) {
    HKEY hKey;
    const char* czExePath = "ProvidenceTracker.exe";
    char pPath[MAX_PATH];
    GetModuleFileNameA(NULL, pPath, MAX_PATH);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExA(hKey, "ProvidenceTracker", 0, REG_SZ, (const BYTE*)pPath, strlen(pPath) + 1);
            cout << "[SYSTEM]: Added to AutoStart successfully." << endl;
        }
        else {
            RegDeleteValueA(hKey, "ProvidenceTracker");
            cout << "[SYSTEM]: Removed from AutoStart." << endl;
        }
        RegCloseKey(hKey);
    }
}

// сохранение данных (зашифрованное)
void SaveData() {
    ofstream file(SAVE_FILE, ios::binary);
    if (file.is_open()) {
        for (auto const& [name, stats] : trackerData) {
            string line = name + ":" + to_string(stats.totalSeconds) + ":" + to_string(stats.isVisible) + "\n";
            string encrypted = Cipher(line);
            file << encrypted;
        }
        file.close();
    }
}

// загрузка данных
void LoadData() {
    ifstream file(SAVE_FILE, ios::binary);
    if (file.is_open()) {
        stringstream buffer;
        buffer << file.rdbuf();
        string content = Cipher(buffer.str()); // расшифровка

        stringstream ss(content);
        string line;
        while (getline(ss, line)) {
            size_t p1 = line.find(':');
            size_t p2 = line.find_last_of(':');
            if (p1 != string::npos && p2 != string::npos) {
                string name = line.substr(0, p1);
                long long seconds = stoll(line.substr(p1 + 1, p2 - p1 - 1));
                bool visible = stoi(line.substr(p2 + 1));
                trackerData[name] = { seconds, visible };
            }
        }
        file.close();
    }
}

string GetActiveWindowName() {
    HWND hwnd = GetForegroundWindow();
    if (hwnd == NULL) return "";

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    if (hProcess) {
        char buffer[MAX_PATH];
        if (GetModuleBaseNameA(hProcess, NULL, buffer, MAX_PATH)) {
            CloseHandle(hProcess);
            string name = string(buffer);
            size_t lastindex = name.find_last_of(".");
            return name.substr(0, lastindex);
        }
        CloseHandle(hProcess);
    }
    return "Unknown";
}

void TrackerLoop() {
    while (isTracking) {
        string currentApp = GetActiveWindowName();

        // фильтр говна и пустых имен
        if (currentApp.length() > 0 && BLACKLIST.find(currentApp) == BLACKLIST.end()) {
            trackerData[currentApp].totalSeconds++;
        }

        // автосохрнение каждые 30 сек
        static int saveTimer = 0;
        if (++saveTimer > 30) {
            SaveData();
            saveTimer = 0;
        }

        this_thread::sleep_for(chrono::seconds(1));
    }
}

// ИНТЕРФЕЙС

void ClearScreen() {
    system("cls");
}

void PrintHeader() {
    cout << "=========================================" << endl;
    cout << "          PROVIDENCE TRACKER v1          " << endl;
    cout << "=========================================" << endl;
}

void ShowStats() {
    ClearScreen();
    PrintHeader();
    cout << "APP NAME\t\tTIME (Min)\tSTATUS" << endl;
    cout << "------------------------------------------" << endl;

    for (auto const& [name, stats] : trackerData) {
        if (stats.totalSeconds > 60) {
            string visibility = stats.isVisible ? "[VISIBLE]" : "[HIDDEN]";
            cout << name << "\t\t" << (stats.totalSeconds / 60) << " m\t" << visibility << endl;
        }
    }
    cout << "\n[PRESS ENTER TO RETURN]";
    cin.ignore(); cin.get();
}

void ToggleVisibility() {
    ClearScreen();
    PrintHeader();
    cout << "Enter App Name to Hide/Unhide: ";
    string target;
    cin >> target;
    if (trackerData.find(target) != trackerData.end()) {
        trackerData[target].isVisible = !trackerData[target].isVisible;
        cout << "Visibility changed for " << target << endl;
        SaveData();
    }
    else {
        cout << "App not found in history." << endl;
    }
    this_thread::sleep_for(chrono::seconds(1));
}

int main() {
    SetConsoleTitleA("Providence Core");
    system("color 0A"); // цвет

    LoadData();
    thread trackerThread(TrackerLoop);
    trackerThread.detach();

    int choice;
    while (true) {
        ClearScreen();
        PrintHeader();
        cout << "1. Show My Stats" << endl;
        cout << "2. Enable AutoStart" << endl;
        cout << "3. Disable AutoStart" << endl;
        cout << "4. Exit (Tracker keeps running)" << endl;
        cout << ">> ";
        cin >> choice;

        switch (choice) {
        case 1: ShowStats(); break;
        case 2: SetAutoStart(true); break;
        case 3: SetAutoStart(false); break;
        case 4: return 0;
        default: break;
        }
    }

    return 0;
}