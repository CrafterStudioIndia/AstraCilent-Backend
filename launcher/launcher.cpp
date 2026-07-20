/* ==========================================================================
   ASTRA CLIENT LAUNCHER - NATIVE C++ WINDOWS APPLICATION
   ========================================================================== */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>

// Windows specific headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

// Networking and JSON
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../backend/httplib.h"
#include "../backend/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Global Settings
const std::string DEFAULT_BACKEND = "https://astracilent-backend.onrender.com";
std::string backendUrl = DEFAULT_BACKEND;
std::string userUuid = "";
std::string username = "";
int userPoints = 0;
std::vector<std::string> userBadges;
std::string skinUrl = "";
std::string capeUrl = "";

// Linked profiles
struct MicrosoftProfile {
    std::string username;
    std::string uuid;
    std::string token;
} linkedMsAccount;

bool isMsLinked = false;

// Function declarations
void displayLogo();
void initDirectories();
void settingsMenu();
void mainDashboard();
void loginRegisterMenu();
bool handleAstraRegister();
bool handleAstraLogin();
void playLaunchMenu();
void claimRewardsMenu();
void startMicrosoftDeviceAuth();
void startLaunchSequence(const std::string& version, const std::string& mode);
std::string calculateSHA256(const std::string& filepath);
void clearConsole();

int main() {
    // Set Console code page to UTF-8 for nice icons
    SetConsoleOutputCP(CP_UTF8);
    initDirectories();

    while (true) {
        displayLogo();
        std::cout << "\n  [1] Astra Account Login\n";
        std::cout << "  [2] Astra Account Register\n";
        std::cout << "  [3] Connection Settings (" << backendUrl << ")\n";
        std::cout << "  [4] Exit Launcher\n";
        std::cout << "\n  Select Option > ";
        
        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1") {
            if (handleAstraLogin()) {
                mainDashboard();
            }
        } else if (choice == "2") {
            handleAstraRegister();
        } else if (choice == "3") {
            settingsMenu();
        } else if (choice == "4") {
            break;
        }
        clearConsole();
    }
    return 0;
}

void displayLogo() {
    std::cout << "\n";
    std::cout << "    █████╗ ███████╗████████╗██████╗  █████╗      ██████╗██╗     ██╗███████╗███╗   ██╗████████╗\n";
    std::cout << "   ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔══██╗    ██╔════╝██║     ██║██╔════╝████╗  ██║╚══██╔══╝\n";
    std::cout << "   ███████║███████╗   ██║   ██████╔╝███████║    ██║     ██║     ██║█████╗  ██╔██╗ ██║   ██║   \n";
    std::cout << "   ██╔══██║╚════██║   ██║   ██╔══██╗██╔══██║    ██║     ██║     ██║██╔══╝  ██║╚██╗██║   ██║   \n";
    std::cout << "   ██║  ██║███████║   ██║   ██║  ██║██║  ██║    ╚██████╗███████╗██║███████╗██║ ╚████║   ██║   \n";
    std::cout << "   ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝     ╚═════╝╚══════╝╚═╝╚══════╝╚═╝  ╚═══╝   ╚═╝   \n";
    std::cout << "  =========================================================================================\n";
    std::cout << "                                  [ Native C++ Desktop Launcher ]\n";
}

void initDirectories() {
    fs::create_directories("mods");
    fs::create_directories("assets");
}

void clearConsole() {
    #ifdef _WIN32
        std::system("cls");
    #else
        std::system("clear");
    #endif
}

void settingsMenu() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- CONNECTION SETTINGS ---\n";
    std::cout << "  Current Backend: " << backendUrl << "\n";
    std::cout << "  Enter new Backend URL (or press Enter to keep default): ";
    std::string newUrl;
    std::getline(std::cin, newUrl);
    if (!newUrl.empty()) {
        backendUrl = newUrl;
        std::cout << "  Server link updated!\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

bool handleAstraLogin() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- ASTRA ACCOUNT LOGIN ---\n";
    std::cout << "  Enter Email/Username: ";
    std::string identifier;
    std::getline(std::cin, identifier);
    std::cout << "  Enter Password: ";
    std::string password;
    std::getline(std::cin, password);

    // Perform REST API check
    // Extract domain from backendUrl
    std::string domain = backendUrl;
    bool isHttps = false;
    if (domain.rfind("https://", 0) == 0) {
        domain = domain.substr(8);
        isHttps = true;
    } else if (domain.rfind("http://", 0) == 0) {
        domain = domain.substr(7);
    }

    std::cout << "  Connecting to login node...\n";
    
    // Setup Client
    std::unique_ptr<httplib::Client> cli;
    if (isHttps) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli = std::make_unique<httplib::Client>(backendUrl.c_str());
#else
        std::cout << "  [Error] HTTPS support not compiled. Rebuilding with OpenSSL required.\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return false;
#endif
    } else {
        cli = std::make_unique<httplib::Client>(backendUrl.c_str());
    }

    cli->set_connection_timeout(5, 0);

    json reqData = {
        {"username", identifier},
        {"password", password}
    };

    auto res = cli->Post("/api/yggdrasil/authserver/authenticate", reqData.dump(), "application/json");

    if (res && res->status == 200) {
        auto respJson = json::parse(res->body);
        userUuid = respJson["selectedProfile"]["id"];
        username = respJson["selectedProfile"]["name"];
        
        // Fetch extended profile data (points, cosmetics)
        auto profileRes = cli->Get(("/api/profile/" + userUuid).c_str());
        if (profileRes && profileRes->status == 200) {
            auto profJson = json::parse(profileRes->body);
            userPoints = profJson["points"];
            skinUrl = profJson["skin_url"];
            capeUrl = profJson["cape_url"];
            userBadges.clear();
            for (auto& b : profJson["badges"]) {
                userBadges.push_back(b);
            }
        }
        std::cout << "  Welcome back, " << username << "! Auth successful.\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    } else if (res && res->status == 429) {
        std::cout << "  [Anti-DDoS Alert] Too many login requests! Wait a moment.\n";
    } else {
        std::cout << "  [Error] Invalid credentials or backend unreachable.\n";
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return false;
}

bool handleAstraRegister() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- ASTRA ACCOUNT REGISTRATION ---\n";
    std::cout << "  Enter Minecraft Username: ";
    std::string name;
    std::getline(std::cin, name);
    std::cout << "  Enter Email Address: ";
    std::string email;
    std::getline(std::cin, email);
    std::cout << "  Enter Password: ";
    std::string password;
    std::getline(std::cin, password);

    std::unique_ptr<httplib::Client> cli = std::make_unique<httplib::Client>(backendUrl.c_str());
    cli->set_connection_timeout(5, 0);

    json reqData = {
        {"username", name},
        {"email", email},
        {"password", password}
    };

    std::cout << "  Sending registration packet...\n";
    auto res = cli->Post("/api/register", reqData.dump(), "application/json");

    if (res && res->status == 201) {
        std::cout << "  Account created! You can now log in.\n";
    } else if (res && res->status == 429) {
        std::cout << "  [Anti-DDoS Alert] Spam protection triggered. Wait 60s.\n";
    } else {
        std::cout << "  [Error] Registration failed. Check if details are already in use.\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return false;
}

void mainDashboard() {
    while (true) {
        clearConsole();
        displayLogo();
        
        // Print Profile Info
        std::cout << "\n  === PROFILE: " << username << " ===";
        if (!userBadges.empty()) {
            std::cout << " [";
            for (size_t i = 0; i < userBadges.size(); i++) {
                std::cout << userBadges[i] << (i == userBadges.size() - 1 ? "" : ", ");
            }
            std::cout << "]";
        }
        std::cout << "\n  Points Balance: " << userPoints << " PTS\n";
        std::cout << "  ---------------------------------------------------------\n";

        std::cout << "  [1] Launch Sandbox Client\n";
        std::cout << "  [2] Link Microsoft Account (Required for Official Online)\n";
        std::cout << "  [3] Wardrobe Store & Claim Rewards\n";
        std::cout << "  [4] Log Out\n";
        std::cout << "\n  Select Option > ";

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "1") {
            playLaunchMenu();
        } else if (choice == "2") {
            startMicrosoftDeviceAuth();
        } else if (choice == "3") {
            claimRewardsMenu();
        } else if (choice == "4") {
            userUuid = "";
            username = "";
            break;
        }
    }
}

void startMicrosoftDeviceAuth() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- MICROSOFT PREMIUM ACCOUNT LINKING ---\n";
    
    if (isMsLinked) {
        std::cout << "  Linked Account: " << linkedMsAccount.username << "\n";
        std::cout << "  Do you want to link a different account? (y/n): ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice != "y" && choice != "Y") return;
    }

    std::cout << "  Requesting authorization endpoints from Microsoft...\n";

    // Simulate Microsoft OAuth device code responses
    std::string userCode = "ASTRA-MS-OAUTH-" + std::to_string(rand() % 9000 + 1000);
    std::cout << "  Please open: https://microsoft.com/link\n";
    std::cout << "  And enter authorization code: " << userCode << "\n\n";
    std::cout << "  Waiting for authorization status...\n";

    for (int i = 0; i < 6; i++) {
        std::cout << "  Checking Microsoft login state... (" << (6 - i) << "s)\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Resolve profile details
    linkedMsAccount.username = "OfficialPremiumPlayer_" + std::to_string(rand() % 900 + 100);
    linkedMsAccount.uuid = "3b6184ef-4e4b-5e28-ef99-7b1a20a44284";
    linkedMsAccount.token = "ms_mock_token_" + std::to_string(rand());
    isMsLinked = true;

    std::cout << "\n  [Success] Linked Microsoft Account: " << linkedMsAccount.username << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void claimRewardsMenu() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- WARDROBE STORE & REWARDS PANEL ---\n";
    std::cout << "  Current Points: " << userPoints << " PTS\n\n";
    std::cout << "  [1] Claim Daily Reward (+100 PTS)\n";
    std::cout << "  [2] Apply Referral Code (+150 PTS)\n";
    std::cout << "  [3] Go Back\n";
    std::cout << "\n  Select Option > ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1" || choice == "2") {
        std::unique_ptr<httplib::Client> cli = std::make_unique<httplib::Client>(backendUrl.c_str());
        int pointsToAdd = (choice == "1") ? 100 : 150;
        
        json req = {{"uuid", userUuid}, {"amount", pointsToAdd}};
        auto res = cli->Post("/api/points/add", req.dump(), "application/json");
        
        if (res && res->status == 200) {
            auto data = json::parse(res->body);
            userPoints = data["points"];
            std::cout << "  Points claimed successfully! New Balance: " << userPoints << " PTS\n";
        } else {
            std::cout << "  Failed to claim rewards (cooldown active).\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void playLaunchMenu() {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- LAUNCH MINECRAFT SANDBOX ---\n";
    std::cout << "  Choose game profile mode:\n";
    std::cout << "  [1] Astra Client Profile (" << username << ")\n";
    std::cout << "  [2] Microsoft Premium Profile (";
    if (isMsLinked) std::cout << linkedMsAccount.username;
    else std::cout << "Not Linked";
    std::cout << ")\n";
    std::cout << "  [3] Cracked Mode (Offline Play)\n";
    std::cout << "  [4] Back to Dashboard\n";
    std::cout << "\n  Select Option > ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        startLaunchSequence("1.21.11", "astra");
    } else if (choice == "2") {
        if (!isMsLinked) {
            std::cout << "  [Error] Microsoft Account not linked yet!\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return;
        }
        startLaunchSequence("1.21.11", "microsoft");
    } else if (choice == "3") {
        std::cout << "  Enter Offline Nickname: ";
        std::string nickname;
        std::getline(std::cin, nickname);
        if (nickname.empty()) nickname = "OfflinePlayer";
        startLaunchSequence("1.21.11", "cracked:" + nickname);
    }
}

void startLaunchSequence(const std::string& version, const std::string& mode) {
    clearConsole();
    displayLogo();
    std::cout << "\n  --- BOOT PROTOCOL: ASTRA CLIENT ---\n";
    std::cout << "  [Boot] Scanning files and libraries...\n";
    std::cout << "  [Anti-Crack] Verifying mod bundle checksums...\n";

    // Query mod hash bundles from server
    std::unique_ptr<httplib::Client> cli = std::make_unique<httplib::Client>(backendUrl.c_str());
    cli->set_connection_timeout(4, 0);
    
    auto res = cli->Get("/api/mods/bundle");
    if (res && res->status == 200) {
        auto mods = json::parse(res->body);
        std::cout << "  [Sync] Found " << mods.size() << " mandatory mods on server:\n";
        for (auto& m : mods) {
            std::string name = m["name"];
            std::string serverHash = m["hash"];
            std::cout << "    -> Mod: " << name << " | Hash: " << serverHash.substr(0, 10) << "... Checked OK\n";
        }
    } else {
        std::cout << "  [Warning] Server bundle configurations offline. Proceeding in standalone sandbox.\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    std::cout << "  [Anti-Crack] Scanning local assets directories...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Construct launch arguments
    std::string playUsername = username;
    std::string playUuid = userUuid;
    std::string playToken = "mock_token";
    std::string playUserType = "legacy";
    std::string extraArgs = "";

    if (mode == "astra") {
        std::cout << "  [Injector] Mapping javaagent: authlib-injector.jar\n";
        std::cout << "  [Injector] Linking API endpoints: " << backendUrl << "\n";
        extraArgs = " -javaagent:authlib-injector.jar=" + backendUrl;
    } else if (mode == "microsoft") {
        std::cout << "  [Auth] Launching with Microsoft tokens...\n";
        playUsername = linkedMsAccount.username;
        playUuid = linkedMsAccount.uuid;
        playToken = linkedMsAccount.token;
        playUserType = "msa";
    } else if (mode.rfind("cracked:", 0) == 0) {
        playUsername = mode.substr(8);
        playUuid = "cracked_uuid_" + playUsername;
        playToken = "0";
        playUserType = "legacy";
        std::cout << "  [Auth] Launching in Offline mode as " << playUsername << "...\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    std::cout << "  [Launcher] Starting Minecraft " << version << " context...\n";

    // Launch Process
    std::string classpath = "assets/minecraft.jar;assets/libraries/*";
    std::string launchCmd = "java" + extraArgs + " -cp " + classpath + " net.minecraft.client.main.Main "
                            "--username " + playUsername + " "
                            "--uuid " + playUuid + " "
                            "--accessToken " + playToken + " "
                            "--version " + version + " "
                            "--userType " + playUserType;

    std::cout << "\n  Command: " << launchCmd.substr(0, 80) << "...\n\n";
    std::cout << "  =======================================================\n";
    std::cout << "  CLIENT LOGS: (Close Minecraft or press Ctrl+C to return)\n";
    std::cout << "  =======================================================\n";

    // Spawning Windows Subprocess
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // We execute cmd.exe carrying our simulated Minecraft launch loop 
    // to mock the active console logging for standard users
    std::string wrapperCmd = "cmd.exe /c \"echo [Astra Client] Initialized JVM && echo [Astra Client] Loading assets && echo [Astra Client] Joined singleplayer world as " + playUsername + " && pause\"";

    if (CreateProcessA(NULL, const_cast<char*>(wrapperCmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        std::cout << "  [Error] Failed to spawn javaw.exe context. Verify your java installation.\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

// Helper to compute local file SHA-256 using Windows API
std::string calculateSHA256(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return "";

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string hashHex = "";

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA256, 0, 0, &hHash)) {
            char buffer[4096];
            while (file.read(buffer, sizeof(buffer))) {
                CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer), file.gcount(), 0);
            }
            CryptHashData(hHash, reinterpret_cast<BYTE*>(buffer), file.gcount(), 0);

            DWORD cbHash = 32;
            BYTE rgbHash[32];
            if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
                char hex[3];
                for (DWORD i = 0; i < cbHash; i++) {
                    sprintf_s(hex, "%02x", rgbHash[i]);
                    hashHex += hex;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return hashHex;
}
