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
#include <mutex>

using namespace std;
const char CYPHER_KEY = 'X';
const string SAVE_FILE = "system_data.dat";

// Black list
set<string> BLACKLIST = { "SearchApp", "svchost", "csrss", "System", "Idle", "Registry", "smss" };

//data structure
struct AppStats {
    long long totalSeconds = 0;
    bool isVisible = true;
};

map<string, AppStats> trackerData;
mutex dataMutex;
bool isTracking = true;

//string encryption/decryption (XOR)
string Cipher(string data) {
    string output = data;
    for (int i = 0; i < (int)data.size(); i++)
        output[i] = data[i] ^ CYPHER_KEY;
    return output;
}

//adding to the reg
void SetAutoStart(bool enable) {
    HKEY hKey;
    char pPath[MAX_PATH];
    GetModuleFileNameA(NULL, pPath, MAX_PATH);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExA(hKey, "ProvidenceTracker", 0, REG_SZ, (const BYTE*)pPath, (DWORD)(strlen(pPath) + 1));
            cout << "[SYSTEM]: Added to AutoStart successfully." << endl;
        }
        else {
            RegDeleteValueA(hKey, "ProvidenceTracker");
            cout << "[SYSTEM]: Removed from AutoStart." << endl;
        }
        RegCloseKey(hKey);
    }
}

//save data (encrypted)
void SaveData() {
    lock_guard<mutex> lock(dataMutex);
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

//LoadData
void LoadData() {
    lock_guard<mutex> lock(dataMutex);
    ifstream file(SAVE_FILE, ios::binary);
    if (file.is_open()) {
        stringstream buffer;
        buffer << file.rdbuf();
        string content = Cipher(buffer.str());

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
            if (lastindex == string::npos) return name;
            return name.substr(0, lastindex);
        }
        CloseHandle(hProcess);
    }
    return "Unknown";
}

void TrackerLoop() {
    int saveTimer = 0;
    while (isTracking) {
        string currentApp = GetActiveWindowName();

        if (currentApp.length() > 0 && BLACKLIST.find(currentApp) == BLACKLIST.end()) {
            lock_guard<mutex> lock(dataMutex);
            trackerData[currentApp].totalSeconds++;
        }

        //autosave
        if (++saveTimer > 30) {
            SaveData();
            saveTimer = 0;
        }

        this_thread::sleep_for(chrono::seconds(1));
    }
}

//INTERFACE

void ClearScreen() {
    system("cls");
}

void PrintHeader() {
    cout << "=========================================" << endl;
    cout << "          PROVIDENCE TRACKER v1.2        " << endl;
    cout << "=========================================" << endl;
}

void ShowStats() {
    ClearScreen();
    PrintHeader();
    cout << "APP NAME\t\tTIME (Min)\tSTATUS" << endl;
    cout << "------------------------------------------" << endl;

    {
        lock_guard<mutex> lock(dataMutex);
        for (auto const& [name, stats] : trackerData) {
            if (stats.totalSeconds > 60) {
                string visibility = stats.isVisible ? "[VISIBLE]" : "[HIDDEN]";
                cout << name << "\t\t" << (stats.totalSeconds / 60) << " m\t" << visibility << endl;
            }
        }
    }

    cout << "\n[PRESS ENTER TO RETURN]";
    cin.ignore();
    cin.get();
}

int main() {
    SetConsoleTitleA("Providence Core");
    system("color 0A");

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
        cout << "4. Go to Background" << endl;
        cout << ">> ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(10000, '\n');
            continue;
        }

        switch (choice) {
        case 1: ShowStats(); break;
        case 2: SetAutoStart(true); break;
        case 3: SetAutoStart(false); break;
        case 4:
            cout << "[SYSTEM]: Process detaching from console..." << endl;
            this_thread::sleep_for(chrono::seconds(1));

            FreeConsole();

            while (true) {
                this_thread::sleep_for(chrono::hours(24));
            }
            return 0;
        default: break;
        }
    }

    return 0;
}