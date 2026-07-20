#define CPPHTTPLIB_OPENSSL_SUPPORT // Define this if we ever want SSL, but standard is fine
#include "httplib.h"
#include "json.hpp"
#include "database.hpp"
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

// Helper to generate a random UUID-like string
std::string generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        ss << dis(gen);
    }
    return ss.str();
}

// Base64 helper for skin/cape textures
std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// Strips hyphens from UUID for Yggdrasil API
std::string strip_uuid(const std::string& uuid) {
    std::string stripped = uuid;
    stripped.erase(std::remove(stripped.begin(), stripped.end(), '-'), stripped.end());
    return stripped;
}

int main() {
    httplib::Server svr;
    AstraDatabase db("astra.db");

    std::cout << "Astra Client Backend running on http://localhost:8080" << std::endl;

    // Enable CORS for web portals and local launcher requests
    auto set_cors_headers = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    };

    svr.Options(R"(/.*)", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 204;
    });

    // ----------------------------------------------------
    // 1. ASTRA CUSTOM API ENDPOINTS
    // ----------------------------------------------------

    // Register User
    svr.Post("/api/register", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string email = body["email"];
            std::string password = body["password"];

            if (db.user_exists(email) || db.user_exists(username)) {
                res.status = 400;
                res.set_content(json{{"error", "Username or email already exists"}}.dump(), "application/json");
                return;
            }

            std::string uuid = generate_uuid();
            if (db.register_user(uuid, username, email, password)) {
                res.status = 201;
                res.set_content(json{{"message", "User registered successfully"}, {"uuid", uuid}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to register user"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON payload"}}.dump(), "application/json");
        }
    });

    // Sync Profile / User Data
    svr.Get(R"(/api/profile/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string identifier = req.matches[1];
        AstraDatabase::User user;
        
        bool found = false;
        if (identifier.find('-') != std::string::npos || identifier.length() == 32) {
            found = db.get_user_by_uuid(identifier, user);
        } else {
            found = db.get_user(identifier, user);
        }

        if (found) {
            json profile_json = {
                {"uuid", user.uuid},
                {"username", user.username},
                {"email", user.email},
                {"points", user.points},
                {"badges", json::parse(user.badges.empty() ? "[\"Founder\"]" : user.badges)},
                {"cosmetics", json::parse(user.cosmetics.empty() ? "[]" : user.cosmetics)},
                {"equipped", {
                    {"cape", user.equipped_cape},
                    {"wings", user.equipped_wings},
                    {"hat", user.equipped_hat},
                    {"aura", user.equipped_aura}
                }},
                {"skin_url", user.skin_url},
                {"cape_url", user.cape_url}
            };
            res.status = 200;
            res.set_content(profile_json.dump(), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{{"error", "User not found"}}.dump(), "application/json");
        }
    });

    // Equip Cosmetic
    svr.Post("/api/cosmetics/equip", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string slot = body["slot"]; // "cape", "wings", "hat", "aura"
            std::string item = body["item"]; // name of the cosmetic, or empty string to unequip

            if (db.update_equipped(uuid, slot, item)) {
                res.status = 200;
                res.set_content(json{{"message", "Cosmetic updated successfully"}, {"slot", slot}, {"item", item}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to update cosmetic"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON payload"}}.dump(), "application/json");
        }
    });

    // Buy Cosmetic / Add to Inventory
    svr.Post("/api/cosmetics/purchase", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string item = body["item"];
            int cost = body["cost"];

            AstraDatabase::User user;
            if (db.get_user_by_uuid(uuid, user)) {
                if (user.points < cost) {
                    res.status = 400;
                    res.set_content(json{{"error", "Insufficient points"}}.dump(), "application/json");
                    return;
                }

                // Parse inventory, add item
                json inv = json::parse(user.cosmetics.empty() ? "[]" : user.cosmetics);
                bool already_owned = false;
                for (auto& owned : inv) {
                    if (owned == item) {
                        already_owned = true;
                        break;
                    }
                }

                if (!already_owned) {
                    inv.push_back(item);
                }

                if (db.update_cosmetics(uuid, inv.dump()) && db.update_points(uuid, -cost)) {
                    res.status = 200;
                    res.set_content(json{{"message", "Purchase successful"}, {"points", user.points - cost}, {"cosmetics", inv}}.dump(), "application/json");
                } else {
                    res.status = 500;
                    res.set_content(json{{"error", "Purchase transaction failed"}}.dump(), "application/json");
                }
            } else {
                res.status = 404;
                res.set_content(json{{"error", "User not found"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON payload"}}.dump(), "application/json");
        }
    });

    // Update Skin and Cape Textures
    svr.Post("/api/profile/textures", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string skin_url = body["skin_url"];
            std::string cape_url = body["cape_url"];

            if (db.update_textures(uuid, skin_url, cape_url)) {
                res.status = 200;
                res.set_content(json{{"message", "Textures updated successfully"}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to update textures"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON payload"}}.dump(), "application/json");
        }
    });

    // Add/Claim Points (Daily reward, Referrals, etc.)
    svr.Post("/api/points/add", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            int amount = body["amount"];

            if (db.update_points(uuid, amount)) {
                AstraDatabase::User user;
                db.get_user_by_uuid(uuid, user);
                res.status = 200;
                res.set_content(json{{"message", "Points updated successfully"}, {"points", user.points}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to update points"}}.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON payload"}}.dump(), "application/json");
        }
    });

    // Mod Bundles list (Launcher checks this for client updates)
    svr.Get("/api/mods/bundle", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json mods = json::array({
            {{"name", "AstraCore-1.21.11.jar"}, {"size", "4.2 MB"}, {"hash", "a78d89fbc0d2"}, {"url", "https://cdn.astraclient.com/mods/AstraCore.jar"}},
            {{"name", "Sodium-1.21.11.jar"}, {"size", "1.8 MB"}, {"hash", "b23c91a0ef88"}, {"url", "https://cdn.astraclient.com/mods/Sodium.jar"}},
            {{"name", "Lithium-1.21.11.jar"}, {"size", "0.9 MB"}, {"hash", "f89d311029ba"}, {"url", "https://cdn.astraclient.com/mods/Lithium.jar"}},
            {{"name", "AstraHUD-1.21.11.jar"}, {"size", "2.1 MB"}, {"hash", "c1290bb098ef"}, {"url", "https://cdn.astraclient.com/mods/AstraHUD.jar"}}
        });
        res.status = 200;
        res.set_content(mods.dump(), "application/json");
    });

    // Launcher updates check
    svr.Get("/api/launcher/version", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json ver = {
            {"latest_version", "2.0.4"},
            {"required", true},
            {"download_url", "https://cdn.astraclient.com/launcher/AstraLauncher_2.0.4.exe"},
            {"changelog", "Added 3D skins, dynamic space themes, and optimized P2P encrypted lobbies."}
        };
        res.status = 200;
        res.set_content(ver.dump(), "application/json");
    });

    // ----------------------------------------------------
    // 2. YGGDRASIL / AUTHLIB-INJECTOR ENDPOINTS
    // ----------------------------------------------------

    // Auth Metadata (Root)
    svr.Get("/api/yggdrasil", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        json meta = {
            {"meta", {
                {"serverName", "Astra Authentication"},
                {"implementationName", "Astra C++ Backend"},
                {"implementationVersion", "1.0.0"}
            }},
            {"skinDomains", {
                "localhost", "astraclient.com", "cdn.astraclient.com", "imgur.com", "i.imgur.com"
            }},
            {"signaturePublickey", "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0k1... (mock key)\n-----END PUBLIC KEY-----"}
        };
        res.status = 200;
        res.set_content(meta.dump(), "application/json");
    });

    // Authenticate (Login via Launcher/Authlib)
    svr.Post("/api/yggdrasil/authserver/authenticate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"]; // can be email or username
            std::string password = body["password"];
            std::string client_token = body.value("clientToken", generate_uuid());

            AstraDatabase::User user;
            if (db.get_user(username, user)) {
                // Simple password check (plaintext for local mock)
                if (user.password == password) {
                    std::string access_token = generate_uuid();
                    db.create_session(access_token, client_token, user.uuid);

                    std::string stripped_id = strip_uuid(user.uuid);

                    json auth_res = {
                        {"accessToken", access_token},
                        {"clientToken", client_token},
                        {"selectedProfile", {
                            {"id", stripped_id},
                            {"name", user.username}
                        }},
                        {"availableProfiles", json::array({
                            {{"id", stripped_id}, {"name", user.username}}
                        })},
                        {"user", {
                            {"id", stripped_id},
                            {"properties", json::array()}
                        }}
                    };

                    res.status = 200;
                    res.set_content(auth_res.dump(), "application/json");
                } else {
                    res.status = 403;
                    res.set_content(json{
                        {"error", "ForbiddenOperationException"},
                        {"errorMessage", "Invalid credentials"}
                    }.dump(), "application/json");
                }
            } else {
                res.status = 403;
                res.set_content(json{
                    {"error", "ForbiddenOperationException"},
                    {"errorMessage", "Invalid credentials"}
                }.dump(), "application/json");
            }
        } catch (std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        }
    });

    // Validate Session
    svr.Post("/api/yggdrasil/authserver/validate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string token = body["accessToken"];
            std::string uuid;
            if (db.validate_session(token, uuid)) {
                res.status = 204; // No content, valid
            } else {
                res.status = 403;
                res.set_content(json{{"error", "ForbiddenOperationException"}, {"errorMessage", "Invalid token"}}.dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
        }
    });

    // Refresh Token
    svr.Post("/api/yggdrasil/authserver/refresh", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string token = body["accessToken"];
            std::string client_token = body.value("clientToken", "");
            std::string uuid;
            
            if (db.validate_session(token, uuid)) {
                std::string new_token = generate_uuid();
                db.create_session(new_token, client_token, uuid);

                AstraDatabase::User user;
                db.get_user_by_uuid(uuid, user);

                std::string stripped_id = strip_uuid(user.uuid);

                json refresh_res = {
                    {"accessToken", new_token},
                    {"clientToken", client_token},
                    {"selectedProfile", {
                        {"id", stripped_id},
                        {"name", user.username}
                    }},
                    {"user", {
                        {"id", stripped_id},
                        {"properties", json::array()}
                    }}
                };
                res.status = 200;
                res.set_content(refresh_res.dump(), "application/json");
            } else {
                res.status = 403;
                res.set_content(json{{"error", "ForbiddenOperationException"}, {"errorMessage", "Invalid token"}}.dump(), "application/json");
            }
        } catch (...) {
            res.status = 400;
        }
    });

    // Invalidate Session
    svr.Post("/api/yggdrasil/authserver/invalidate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string token = body["accessToken"];
            db.remove_session(token);
            res.status = 204;
        } catch (...) {
            res.status = 400;
        }
    });

    // Sign out (Remove all sessions)
    svr.Post("/api/yggdrasil/authserver/signout", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 204;
    });

    // ----------------------------------------------------
    // YGGDRASIL SESSION SERVER (Minecraft In-Game Checks)
    // ----------------------------------------------------

    // Client joins multiplayer server
    svr.Post("/api/yggdrasil/sessionserver/session/minecraft/join", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        // In local mock, we just say OK.
        // In real servers, this binds serverId + token for authentication check.
        res.status = 204;
    });

    // Server queries if client joined (hasJoined)
    svr.Get("/api/yggdrasil/sessionserver/session/minecraft/hasJoined", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string username = req.get_param_value("username");
        AstraDatabase::User user;
        if (db.get_user(username, user)) {
            std::string stripped_id = strip_uuid(user.uuid);
            json has_joined = {
                {"id", stripped_id},
                {"name", user.username},
                {"properties", json::array()}
            };
            res.status = 200;
            res.set_content(has_joined.dump(), "application/json");
        } else {
            res.status = 204; // Not joined/not found
        }
    });

    // Get User Profile (Includes skins, capes textures for Authlib Injector!)
    svr.Get(R"(/api/yggdrasil/sessionserver/session/minecraft/profile/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string stripped_id = req.matches[1];
        
        // Find user by reconstructed UUID
        std::string uuid = stripped_id;
        if (uuid.length() == 32) {
            uuid.insert(8, "-");
            uuid.insert(13, "-");
            uuid.insert(18, "-");
            uuid.insert(23, "-");
        }

        AstraDatabase::User user;
        if (db.get_user_by_uuid(uuid, user)) {
            // Build Textures JSON
            json texture_json = {
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
                {"profileId", stripped_id},
                {"profileName", user.username},
                {"textures", json::object()}
            };

            // Set skin if exists, otherwise default Steve/Alex skin URL
            std::string skin = user.skin_url.empty() ? "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d" : user.skin_url;
            texture_json["textures"]["SKIN"] = {{"url", skin}};

            // Set cape if equipped
            if (!user.equipped_cape.empty()) {
                // Get URL from equipped cape, or fallback to user.cape_url if it's set
                std::string cape = user.cape_url.empty() ? "https://textures.minecraft.net/texture/c7d3db9f71c4c1a5b4f8d22791e8477ffdf7867be7614e040f7bdf753b1b62" : user.cape_url;
                texture_json["textures"]["CAPE"] = {{"url", cape}};
            } else if (!user.cape_url.empty()) {
                texture_json["textures"]["CAPE"] = {{"url", user.cape_url}};
            }

            // Encode textures object to Base64
            std::string encoded_textures = base64_encode(texture_json.dump());

            json profile_res = {
                {"id", stripped_id},
                {"name", user.username},
                {"properties", json::array({
                    {
                        {"name", "textures"},
                        {"value", encoded_textures}
                    }
                })}
            };

            res.status = 200;
            res.set_content(profile_res.dump(), "application/json");
        } else {
            res.status = 204; // User not found
        }
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}
