#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "database.hpp"
#include <random>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstdlib>

using json = nlohmann::json;

// Global rate limiting structures
std::map<std::string, std::vector<double>> ip_logs;
std::mutex rate_limit_mutex;

bool is_rate_limited(const std::string& ip) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    auto& logs = ip_logs[ip];
    
    // Clear timestamps older than 60 seconds
    logs.erase(std::remove_if(logs.begin(), logs.end(), [&](double t) {
        return now - t > 60;
    }), logs.end());
    
    if (logs.size() >= 60) {
        return true; // Exceeded 60 requests/min
    }
    
    logs.push_back(now);
    return false;
}

// Helper to generate UUIDs
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

std::string strip_uuid(const std::string& uuid) {
    std::string stripped = uuid;
    stripped.erase(std::remove(stripped.begin(), stripped.end(), '-'), stripped.end());
    return stripped;
}

// Embedded Admin Console HTML
const char* admin_dashboard_html = R"html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Astra Admin | Cloud Console</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        * { margin:0; padding:0; box-sizing:border-box; font-family:'Outfit', sans-serif; }
        body {
            background: linear-gradient(135deg, #090314 0%, #020106 100%);
            color: #e0e0e0;
            min-height: 100vh;
            padding: 30px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; border-bottom: 1px solid rgba(255,255,255,0.05); padding-bottom: 15px; }
        .header h1 { font-size: 26px; font-weight: 800; color: #fff; display: flex; align-items: center; gap: 10px; }
        .header h1 i { color: #bb86fc; }
        .glass-card {
            background: rgba(20, 8, 38, 0.45);
            backdrop-filter: blur(16px);
            border: 1px solid rgba(187, 134, 252, 0.15);
            border-radius: 16px;
            padding: 25px;
            margin-bottom: 25px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
        }
        .grid-2 { display: grid; grid-template-columns: 1.6fr 1fr; gap: 25px; }
        .login-box { max-width: 400px; margin: 100px auto; text-align: center; }
        .login-box h2 { font-size: 24px; font-weight: 800; margin-bottom: 20px; color: #fff; }
        .input-group { position: relative; margin-bottom: 15px; }
        .input-group i { position: absolute; left: 15px; top: 50%; transform: translateY(-50%); color: #777; }
        .input-group input, .input-group textarea, select {
            width: 100%;
            padding: 12px 15px 12px 42px;
            background: rgba(0,0,0,0.3);
            border: 1px solid rgba(187, 134, 252, 0.2);
            border-radius: 10px;
            color: #fff;
            outline: none;
            font-size: 14px;
        }
        .input-group input:focus, select:focus { border-color: #bb86fc; }
        .btn {
            background: #bb86fc;
            color: #0b0518;
            border: none;
            padding: 12px 20px;
            border-radius: 10px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        .btn:hover { background: #d7b4ff; transform: translateY(-2px); box-shadow: 0 0 15px rgba(187, 134, 252, 0.4); }
        .btn-sm { padding: 6px 12px; font-size: 12px; border-radius: 6px; }
        
        table { width: 100%; border-collapse: collapse; margin-top: 15px; font-size: 13px; text-align: left; }
        th, td { padding: 12px; border-bottom: 1px solid rgba(255,255,255,0.05); }
        th { color: #bb86fc; font-weight: 700; }
        tr:hover { background: rgba(255,255,255,0.02); }
        .badge { padding: 3px 8px; border-radius: 10px; font-size: 10px; font-weight: 700; background: rgba(255,255,255,0.1); margin-right: 4px; }
        .badge-founder { background: rgba(255, 215, 0, 0.15); color: #ffd700; border: 1px solid #ffd700; }
        .badge-staff { background: rgba(233, 30, 99, 0.15); color: #e91e63; border: 1px solid #e91e63; }
        .badge-beta { background: rgba(0, 230, 118, 0.15); color: #00e676; border: 1px solid #00e676; }
        
        .uploader-zone {
            border: 2px dashed rgba(187, 134, 252, 0.3);
            border-radius: 12px;
            padding: 30px;
            text-align: center;
            cursor: pointer;
            margin-bottom: 15px;
            transition: all 0.2s ease;
        }
        .uploader-zone:hover { border-color: #bb86fc; background: rgba(187, 134, 252, 0.03); }
        .uploader-zone i { font-size: 38px; color: #bb86fc; margin-bottom: 10px; }
        .upload-status { font-size: 12px; font-weight: 600; margin-top: 10px; }

        .modal { display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.7); z-index:100; justify-content:center; align-items:center; }
        .modal-content { width: 500px; background:#120a22; border: 1px solid #bb86fc; padding: 30px; border-radius: 16px; }
        .modal-content h3 { margin-bottom: 15px; color:#fff; }
        .checkbox-group { display: flex; flex-wrap: wrap; gap: 10px; margin: 10px 0; }
        .checkbox-item { display: flex; align-items: center; gap: 6px; font-size: 12px; }
        .checkbox-item input { accent-color:#bb86fc; }
    </style>
</head>
<body>
    <div class="container" id="adminAuth">
        <div class="glass-card login-box">
            <h2>Astra Admin Console</h2>
            <div class="input-group">
                <i class="fa-solid fa-lock"></i>
                <input type="password" id="adminPasswordInput" placeholder="Enter Admin Password">
            </div>
            <button class="btn" onclick="authenticateAdmin()" style="width: 100%;">Sign In</button>
            <p id="authError" style="color:#ff5252; margin-top:10px; font-size:13px; font-weight:600;"></p>
        </div>
    </div>

    <div class="container" id="adminDashboard" style="display: none;">
        <div class="header">
            <h1><i class="fa-solid fa-solar-system"></i> Astra Client Management Console</h1>
            <button class="btn btn-sm" onclick="logoutAdmin()" style="background:#ff5252; color:#fff;">Logout</button>
        </div>

        <div class="grid-2">
            <div class="glass-card">
                <h3>Registered Accounts</h3>
                <table id="usersTable">
                    <thead>
                        <tr>
                            <th>Username</th>
                            <th>Email</th>
                            <th>Points</th>
                            <th>Badges/Tags</th>
                            <th>Action</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
            </div>

            <div class="flex-col">
                <div class="glass-card">
                    <h3>Launcher Update Panel</h3>
                    <div class="uploader-zone" onclick="triggerFileInput()" id="dropZone">
                        <i class="fa-solid fa-cloud-arrow-up"></i>
                        <p id="uploadText">Drag files or Click to Upload (.exe or .jar)</p>
                        <input type="file" id="fileInput" style="display:none;" onchange="uploadFile()">
                    </div>
                    <div id="uploadStatus" class="upload-status"></div>
                </div>

                <div class="glass-card">
                    <h3>Launcher Configurations</h3>
                    <div class="input-group" style="margin-top: 10px;">
                        <input type="text" id="configVersion" placeholder="Version (e.g. 2.0.5)">
                    </div>
                    <div class="input-group">
                        <input type="text" id="configDownloadUrl" placeholder="Download Executable URL">
                    </div>
                    <div class="input-group">
                        <textarea id="configChangelog" style="height:80px; width:100%; border-radius:10px; background:rgba(0,0,0,0.3); border:1px solid rgba(187,134,252,0.2); color:#fff; padding:10px; outline:none;" placeholder="Changelog details..."></textarea>
                    </div>
                    <button class="btn" onclick="saveVersionConfig()" style="width: 100%;">Save & Deploy Config</button>
                </div>
            </div>
        </div>
    </div>

    <div class="modal" id="editModal">
        <div class="modal-content glass-card">
            <h3 id="editModalTitle">Edit User</h3>
            <input type="hidden" id="editUuid">
            <div class="input-group" style="margin-bottom:10px;">
                <input type="number" id="editPoints" placeholder="Points Balance">
            </div>
            <div class="input-group" style="margin-bottom:10px;">
                <input type="url" id="editSkin" placeholder="Skin PNG URL">
            </div>
            <div class="input-group" style="margin-bottom:10px;">
                <input type="url" id="editCape" placeholder="Cape PNG URL">
            </div>
            <p style="font-size:12px; color:#bb86fc; font-weight:600; margin-top:10px;">Badges & Admin Tags:</p>
            <div class="checkbox-group">
                <div class="checkbox-item"><input type="checkbox" id="badgeFounder"><label for="badgeFounder">Founder</label></div>
                <div class="checkbox-item"><input type="checkbox" id="badgeBeta"><label for="badgeBeta">Beta Tester</label></div>
                <div class="checkbox-item"><input type="checkbox" id="badgeStaff"><label for="badgeStaff">Staff</label></div>
                <div class="checkbox-item"><input type="checkbox" id="badgeSupporter"><label for="badgeSupporter">Supporter</label></div>
                <div class="checkbox-item"><input type="checkbox" id="badgeCreator"><label for="badgeCreator">Creator</label></div>
            </div>
            <div style="display:flex; gap:10px; margin-top:20px;">
                <button class="btn btn-primary" onclick="saveUserData()">Save Changes</button>
                <button class="btn" onclick="closeEditModal()" style="background:#444; color:#fff;">Cancel</button>
            </div>
        </div>
    </div>

    <script>
        let adminToken = localStorage.getItem("astra_admin_token") || "";
        let hostUrl = window.location.origin;

        if (adminToken) { showDashboard(); }

        function authenticateAdmin() {
            const pwd = document.getElementById("adminPasswordInput").value;
            const err = document.getElementById("authError");
            fetch(`${hostUrl}/admin/api/users`, { headers: { "Authorization": `Bearer ${pwd}` } })
            .then(res => {
                if(res.ok) {
                    adminToken = pwd;
                    localStorage.setItem("astra_admin_token", adminToken);
                    showDashboard();
                } else { err.innerText = "Access Denied: Invalid Admin Credentials."; }
            }).catch(e => err.innerText = "Error contacting API backend.");
        }

        function showDashboard() {
            document.getElementById("adminAuth").style.display = "none";
            document.getElementById("adminDashboard").style.display = "block";
            loadUsers();
            loadVersionConfig();
        }

        function logoutAdmin() {
            localStorage.removeItem("astra_admin_token");
            adminToken = "";
            document.getElementById("adminAuth").style.display = "block";
            document.getElementById("adminDashboard").style.display = "none";
        }

        function loadUsers() {
            fetch(`${hostUrl}/admin/api/users`, { headers: { "Authorization": `Bearer ${adminToken}` } })
            .then(res => res.json())
            .then(users => {
                const tbody = document.querySelector("#usersTable tbody");
                tbody.innerHTML = "";
                users.forEach(u => {
                    const tr = document.createElement("tr");
                    const badges = JSON.parse(u.badges || '[]');
                    let badgeHtml = "";
                    badges.forEach(b => {
                        const style = b.toLowerCase() === 'founder' ? 'badge-founder' : 
                                      b.toLowerCase() === 'staff' ? 'badge-staff' :
                                      b.toLowerCase() === 'beta tester' ? 'badge-beta' : '';
                        badgeHtml += `<span class="badge ${style}">${b}</span>`;
                    });
                    tr.innerHTML = `
                        <td><strong>${u.username}</strong></td>
                        <td>${u.email}</td>
                        <td style="color:#ffd700; font-weight:700;">${u.points}</td>
                        <td>${badgeHtml}</td>
                        <td><button class="btn btn-sm" onclick="openEditModal('${u.uuid}', '${u.username}', ${u.points}, '${u.skin_url}', '${u.cape_url}', '${u.badges.replace(/'/g, "\\'")}')">Edit</button></td>
                    `;
                    tbody.appendChild(tr);
                });
            });
        }

        function loadVersionConfig() {
            fetch(`${hostUrl}/api/launcher/version`)
            .then(res => res.json())
            .then(data => {
                document.getElementById("configVersion").value = data.latest_version;
                document.getElementById("configDownloadUrl").value = data.download_url;
                document.getElementById("configChangelog").value = data.changelog;
            });
        }

        function saveVersionConfig() {
            const version = document.getElementById("configVersion").value;
            const download_url = document.getElementById("configDownloadUrl").value;
            const changelog = document.getElementById("configChangelog").value;

            fetch(`${hostUrl}/admin/api/version/update`, {
                method: "POST",
                headers: { "Content-Type": "application/json", "Authorization": `Bearer ${adminToken}` },
                body: JSON.stringify({ version, download_url, changelog })
            }).then(res => {
                if (res.ok) { alert("Version configuration saved!"); loadVersionConfig(); }
                else { alert("Unauthorized."); }
            });
        }

        function triggerFileInput() { document.getElementById("fileInput").click(); }

        function uploadFile() {
            const fileInput = document.getElementById("fileInput");
            const file = fileInput.files[0];
            const status = document.getElementById("uploadStatus");
            const text = document.getElementById("uploadText");
            if(!file) return;

            status.style.color = "#bb86fc";
            status.innerText = `Uploading ${file.name}...`;

            const formData = new FormData();
            formData.append("file", file);

            fetch(`${hostUrl}/admin/api/upload`, {
                method: "POST",
                headers: { "Authorization": `Bearer ${adminToken}` },
                body: formData
            })
            .then(res => res.json())
            .then(data => {
                if (data.url) {
                    status.style.color = "#4caf50";
                    status.innerText = "Upload Complete!";
                    text.innerText = "File Uploaded!";
                    if(file.name.endsWith(".exe")) {
                        document.getElementById("configDownloadUrl").value = data.url;
                    }
                } else {
                    status.style.color = "#ff5252";
                    status.innerText = data.error || "Upload failed.";
                }
            }).catch(e => {
                status.style.color = "#ff5252";
                status.innerText = "Upload connection error.";
            });
        }

        function openEditModal(uuid, name, points, skin, cape, badgesJson) {
            document.getElementById("editUuid").value = uuid;
            document.getElementById("editModalTitle").innerText = `Edit: ${name}`;
            document.getElementById("editPoints").value = points;
            document.getElementById("editSkin").value = skin;
            document.getElementById("editCape").value = cape;
            
            document.querySelectorAll(".checkbox-item input").forEach(i => i.checked = false);
            const badges = JSON.parse(badgesJson || '[]');
            badges.forEach(b => {
                if (b === "Founder") document.getElementById("badgeFounder").checked = true;
                if (b === "Beta Tester") document.getElementById("badgeBeta").checked = true;
                if (b === "Staff") document.getElementById("badgeStaff").checked = true;
                if (b === "Supporter") document.getElementById("badgeSupporter").checked = true;
                if (b === "Creator") document.getElementById("badgeCreator").checked = true;
            });
            document.getElementById("editModal").style.display = "flex";
        }

        function closeEditModal() { document.getElementById("editModal").style.display = "none"; }

        function saveUserData() {
            const uuid = document.getElementById("editUuid").value;
            const points = parseInt(document.getElementById("editPoints").value);
            const skin_url = document.getElementById("editSkin").value;
            const cape_url = document.getElementById("editCape").value;
            const badges = [];
            if(document.getElementById("badgeFounder").checked) badges.push("Founder");
            if(document.getElementById("badgeBeta").checked) badges.push("Beta Tester");
            if(document.getElementById("badgeStaff").checked) badges.push("Staff");
            if(document.getElementById("badgeSupporter").checked) badges.push("Supporter");
            if(document.getElementById("badgeCreator").checked) badges.push("Creator");

            fetch(`${hostUrl}/admin/api/users/update`, {
                method: "POST",
                headers: { "Content-Type": "application/json", "Authorization": `Bearer ${adminToken}` },
                body: JSON.stringify({ uuid, points, skin_url, cape_url, badges })
            }).then(res => {
                if(res.ok) { alert("User data updated!"); closeEditModal(); loadUsers(); }
                else { alert("Failed to update user."); }
            });
        }
    </script>
</body>
</html>)html";

int main() {
    httplib::Server svr;
    AstraDatabase db("astra.db");
    
    // Ensure uploads directory exists
    std::system("mkdir uploads");

    // Load admin password from environment
    const char* env_pwd = std::getenv("ADMIN_PASSWORD");
    std::string admin_password = env_pwd ? env_pwd : "Gamedev@1234";

    std::cout << "Astra C++ Backend running on http://localhost:8080" << std::endl;

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
    // MIDDLEWARE: RATE LIMITER
    // ----------------------------------------------------
    svr.Post("/admin/api/upload", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) {
            res.status = 429;
            res.set_content(json{{"error", "DDoS Limit Exceeded"}}.dump(), "application/json");
            return;
        }

        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer " + admin_password) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }

        if (req.has_file("file")) {
            const auto& file = req.get_file_value("file");
            std::string filename = file.filename;
            std::string file_content = file.content;
            
            std::ofstream out("uploads/" + filename, std::ios::binary);
            out.write(file_content.data(), file_content.size());
            out.close();

            std::string host = req.get_header_value("Host");
            std::string protocol = (host.find("render.com") != std::string::npos || host.find("koyeb") != std::string::npos) ? "https" : "http";
            std::string url = protocol + "://" + host + "/uploads/" + filename;

            res.status = 200;
            res.set_content(json{
                {"message", "File uploaded successfully!"},
                {"filename", filename},
                {"url", url}
            }.dump(), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{{"error", "No file uploaded"}}.dump(), "application/json");
        }
    });

    // Root welcome
    svr.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
        res.status = 200;
        res.set_content(json{
            {"status", "Astra Client Backend is online"},
            {"version", "2.0.4"},
            {"github", "https://github.com/CrafterStudioIndia/AstraCilent-Backend"}
        }.dump(), "application/json");
    });

    // Serve Static Uploads
    svr.Get(R"(/uploads/(.+))", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string filename = req.matches[1];
        std::ifstream file("uploads/" + filename, std::ios::binary);
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.status = 200;
            res.set_content(buffer.str(), "application/octet-stream");
        } else {
            res.status = 404;
        }
    });

    // Serve Admin HTML Panel
    svr.Get("/admin", [&](const httplib::Request& req, httplib::Response& res) {
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
        res.status = 200;
        res.set_content(admin_dashboard_html, "text/html");
    });

    // Admin Query Users list
    svr.Get("/admin/api/users", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
        
        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer " + admin_password) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return;
        }
        
        // Mock query from DB for simplicity of single file setup
        sqlite3* sql_db;
        sqlite3_open("astra.db", &sql_db);
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sql_db, "SELECT uuid, username, email, points, badges, skin_url, cape_url FROM users", -1, &stmt, nullptr);
        
        json user_list = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            user_list.push_back({
                {"uuid", (const char*)sqlite3_column_text(stmt, 0)},
                {"username", (const char*)sqlite3_column_text(stmt, 1)},
                {"email", (const char*)sqlite3_column_text(stmt, 2)},
                {"points", sqlite3_column_int(stmt, 3)},
                {"badges", (const char*)sqlite3_column_text(stmt, 4)},
                {"skin_url", (const char*)sqlite3_column_text(stmt, 5)},
                {"cape_url", (const char*)sqlite3_column_text(stmt, 6)}
            });
        }
        sqlite3_finalize(stmt);
        sqlite3_close(sql_db);

        res.status = 200;
        res.set_content(user_list.dump(), "application/json");
    });

    // Admin Update User
    svr.Post("/admin/api/users/update", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
        
        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer " + admin_password) {
            res.status = 401;
            return;
        }

        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            int points = body["points"];
            std::string skin = body["skin_url"];
            std::string cape = body["cape_url"];
            std::string badges = body["badges"].dump();

            db.update_points(uuid, points - db.get_user_by_uuid(uuid, db.get_user_by_uuid(uuid, *new AstraDatabase::User()) ? *new AstraDatabase::User() : *new AstraDatabase::User())); // simple delta mock
            // Directly overwrite via SQL for exact admin commands
            sqlite3* sql_db;
            sqlite3_open("astra.db", &sql_db);
            std::string query = "UPDATE users SET points = " + std::to_string(points) + 
                                ", badges = '" + badges + 
                                "', skin_url = '" + skin + 
                                "', cape_url = '" + cape + "' WHERE uuid = '" + uuid + "';";
            char* err;
            sqlite3_exec(sql_db, query.c_str(), nullptr, nullptr, &err);
            sqlite3_close(sql_db);

            res.status = 200;
            res.set_content(json{{"message", "Success"}}.dump(), "application/json");
        } catch(...) {
            res.status = 400;
        }
    });

    // Admin Update Version
    svr.Post("/admin/api/version/update", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer " + admin_password) { res.status = 401; return; }

        try {
            auto body = json::parse(req.body);
            std::string version = body["version"];
            std::string url = body["download_url"];
            std::string log = body["changelog"];

            sqlite3* sql_db;
            sqlite3_open("astra.db", &sql_db);
            std::string q1 = "UPDATE config SET value = '" + version + "' WHERE key = 'version';";
            std::string q2 = "UPDATE config SET value = '" + url + "' WHERE key = 'download_url';";
            std::string q3 = "UPDATE config SET value = '" + log + "' WHERE key = 'changelog';";
            sqlite3_exec(sql_db, q1.c_str(), nullptr, nullptr, nullptr);
            sqlite3_exec(sql_db, q2.c_str(), nullptr, nullptr, nullptr);
            sqlite3_exec(sql_db, q3.c_str(), nullptr, nullptr, nullptr);
            sqlite3_close(sql_db);

            res.status = 200;
            res.set_content(json{{"message", "Success"}}.dump(), "application/json");
        } catch(...) {
            res.status = 400;
        }
    });

    // Register User
    svr.Post("/api/register", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
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
                res.set_content(json{{"message", "Success"}, {"uuid", uuid}}.dump(), "application/json");
            } else {
                res.status = 500;
            }
        } catch (...) { res.status = 400; }
    });

    // Sync Profile
    svr.Get(R"(/api/profile/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        if (is_rate_limited(req.remote_addr)) { res.status = 429; return; }
        std::string identifier = req.matches[1];
        AstraDatabase::User user;
        bool found = (identifier.find('-') != std::string::npos || identifier.length() == 32) ? db.get_user_by_uuid(identifier, user) : db.get_user(identifier, user);

        if (found) {
            json profile_json = {
                {"uuid", user.uuid},
                {"username", user.username},
                {"email", user.email},
                {"points", user.points},
                {"badges", json::parse(user.badges.empty() ? "[\"Founder\"]" : user.badges)},
                {"cosmetics", json::parse(user.cosmetics.empty() ? "[]" : user.cosmetics)},
                {"equipped", {{"cape", user.equipped_cape}, {"wings", user.equipped_wings}, {"hat", user.equipped_hat}, {"aura", user.equipped_aura}}},
                {"skin_url", user.skin_url},
                {"cape_url", user.cape_url}
            };
            res.status = 200;
            res.set_content(profile_json.dump(), "application/json");
        } else {
            res.status = 404;
        }
    });

    // Equip Cosmetic
    svr.Post("/api/cosmetics/equip", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string slot = body["slot"];
            std::string item = body["item"];

            if (db.update_equipped(uuid, slot, item)) {
                res.status = 200;
                res.set_content(json{{"message", "Success"}}.dump(), "application/json");
            } else { res.status = 500; }
        } catch (...) { res.status = 400; }
    });

    // Buy Cosmetic
    svr.Post("/api/cosmetics/purchase", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string item = body["item"];
            int cost = body["cost"];

            AstraDatabase::User user;
            if (db.get_user_by_uuid(uuid, user)) {
                if (user.points < cost) { res.status = 400; return; }
                json inv = json::parse(user.cosmetics.empty() ? "[]" : user.cosmetics);
                if (std::find(inv.begin(), inv.end(), item) == inv.end()) inv.push_back(item);

                if (db.update_cosmetics(uuid, inv.dump()) && db.update_points(uuid, -cost)) {
                    res.status = 200;
                    res.set_content(json{{"message", "Success"}, {"points", user.points - cost}, {"cosmetics", inv}}.dump(), "application/json");
                } else { res.status = 500; }
            } else { res.status = 404; }
        } catch (...) { res.status = 400; }
    });

    // Update Skin / Cape URLs
    svr.Post("/api/profile/textures", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string uuid = body["uuid"];
            std::string skin = body["skin_url"];
            std::string cape = body["cape_url"];
            if (db.update_textures(uuid, skin, cape)) { res.status = 200; }
            else { res.status = 500; }
        } catch (...) { res.status = 400; }
    });

    // Add Points
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
                res.set_content(json{{"points", user.points}}.dump(), "application/json");
            } else { res.status = 500; }
        } catch (...) { res.status = 400; }
    });

    svr.Get("/api/mods/bundle", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.set_content(json::array({
            {{"name", "AstraCore-1.21.11.jar"}, {"size", "4.2 MB"}, {"hash", "a78d89fbc0d2"}, {"url", "https://cdn.astraclient.com/mods/AstraCore.jar"}},
            {{"name", "Sodium-1.21.11.jar"}, {"size", "1.8 MB"}, {"hash", "b23c91a0ef88"}, {"url", "https://cdn.astraclient.com/mods/Sodium.jar"}},
            {{"name", "Lithium-1.21.11.jar"}, {"size", "0.9 MB"}, {"hash", "f89d311029ba"}, {"url", "https://cdn.astraclient.com/mods/Lithium.jar"}},
            {{"name", "AstraHUD-1.21.11.jar"}, {"size", "2.1 MB"}, {"hash", "c1290bb098ef"}, {"url", "https://cdn.astraclient.com/mods/AstraHUD.jar"}}
        }).dump(), "application/json");
    });

    svr.Get("/api/launcher/version", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        sqlite3* sql_db;
        sqlite3_open("astra.db", &sql_db);
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sql_db, "SELECT key, value FROM config", -1, &stmt, nullptr);
        std::string version = "2.0.4", url = "", changelog = "";
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            std::string key = (const char*)sqlite3_column_text(stmt, 0);
            std::string val = (const char*)sqlite3_column_text(stmt, 1);
            if (key == "version") version = val;
            if (key == "download_url") url = val;
            if (key == "changelog") changelog = val;
        }
        sqlite3_finalize(stmt);
        sqlite3_close(sql_db);

        res.set_content(json{
            {"latest_version", version},
            {"required", true},
            {"download_url", url},
            {"changelog", changelog}
        }.dump(), "application/json");
    });

    // ----------------------------------------------------
    // YGGDRASIL ENDPOINTS FOR AUTHLIB INJECTOR
    // ----------------------------------------------------
    svr.Get("/api/yggdrasil", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.set_content(json{
            {"meta", {{"serverName", "Astra Authentication"}, {"implementationName", "Astra C++ Backend"}}},
            {"skinDomains", {"localhost", "astraclient.com", "cdn.astraclient.com", "imgur.com"}}
        }.dump(), "application/json");
    });

    svr.Post("/api/yggdrasil/authserver/authenticate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];
            std::string client_token = body.value("clientToken", generate_uuid());

            AstraDatabase::User user;
            if (db.get_user(username, user) && user.password == password) {
                std::string access_token = generate_uuid();
                db.create_session(access_token, client_token, user.uuid);
                std::string stripped_id = strip_uuid(user.uuid);

                res.set_content(json{
                    {"accessToken", access_token},
                    {"clientToken", client_token},
                    {"selectedProfile", {{"id", stripped_id}, {"name", user.username}}},
                    {"availableProfiles", json::array({{{"id", stripped_id}, {"name", user.username}}})},
                    {"user", {{"id", stripped_id}, {"properties", json::array()}}}
                }.dump(), "application/json");
            } else { res.status = 403; }
        } catch(...) { res.status = 400; }
    });

    svr.Post("/api/yggdrasil/authserver/validate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            std::string token = body["accessToken"];
            std::string uuid;
            if (db.validate_session(token, uuid)) { res.status = 204; }
            else { res.status = 403; }
        } catch(...) { res.status = 400; }
    });

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

                res.set_content(json{
                    {"accessToken", new_token},
                    {"clientToken", client_token},
                    {"selectedProfile", {{"id", stripped_id}, {"name", user.username}}},
                    {"user", {{"id", stripped_id}, {"properties", json::array()}}}
                }.dump(), "application/json");
            } else { res.status = 403; }
        } catch(...) { res.status = 400; }
    });

    svr.Post("/api/yggdrasil/authserver/invalidate", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        try {
            auto body = json::parse(req.body);
            db.remove_session(body["accessToken"]);
            res.status = 204;
        } catch(...) { res.status = 400; }
    });

    svr.Post("/api/yggdrasil/authserver/signout", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 204;
    });

    svr.Post("/api/yggdrasil/sessionserver/session/minecraft/join", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        res.status = 204;
    });

    svr.Get("/api/yggdrasil/sessionserver/session/minecraft/hasJoined", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string username = req.get_param_value("username");
        AstraDatabase::User user;
        if (db.get_user(username, user)) {
            res.set_content(json{
                {"id", strip_uuid(user.uuid)},
                {"name", user.username},
                {"properties", json::array()}
            }.dump(), "application/json");
        } else { res.status = 204; }
    });

    svr.Get(R"(/api/yggdrasil/sessionserver/session/minecraft/profile/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        set_cors_headers(res);
        std::string stripped_id = req.matches[1];
        std::string uuid = stripped_id;
        if (uuid.length() == 32) {
            uuid.insert(8, "-"); uuid.insert(13, "-"); uuid.insert(18, "-"); uuid.insert(23, "-");
        }

        AstraDatabase::User user;
        if (db.get_user_by_uuid(uuid, user)) {
            json textures = json::object();
            std::string skin = user.skin_url.empty() ? "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d" : user.skin_url;
            textures["SKIN"] = {{"url", skin}};

            if (!user.equipped_cape.empty()) {
                std::string cape = user.cape_url.empty() ? "https://textures.minecraft.net/texture/c7d3db9f71c4c1a5b4f8d22791e8477ffdf7867be7614e040f7bdf753b1b62" : user.cape_url;
                textures["CAPE"] = {{"url", cape}};
            } else if (!user.cape_url.empty()) {
                textures["CAPE"] = {{"url", user.cape_url}};
            }

            json texture_json = {
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()},
                {"profileId", stripped_id},
                {"profileName", user.username},
                {"textures", textures}
            };

            res.set_content(json{
                {"id", stripped_id},
                {"name", user.username},
                {"properties", json::array({{
                    {"name", "textures"},
                    {"value", base64_encode(texture_json.dump())}
                }})}
            }.dump(), "application/json");
        } else { res.status = 204; }
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}
