import http.server
import socketserver
import json
import sqlite3
import uuid
import base64
import time
import re
import os
import hashlib
from urllib.parse import urlparse, parse_qs

PORT = int(os.environ.get("PORT", 8080))
DB_FILE = "astra.db"
UPLOAD_DIR = "uploads"

# Retrieve Admin Password from environment variable (default for local testing)
ADMIN_PASSWORD = os.environ.get("ADMIN_PASSWORD", "Gamedev@1234").strip().strip('"').strip("'")

# In-memory IP rate limiter tracking: { ip: [timestamp1, timestamp2, ...] }
RATE_LIMIT_LIMIT = 60 # max requests per minute
RATE_LIMIT_WINDOW = 60 # 60 seconds
ip_request_log = {}

def is_rate_limited(ip):
    now = time.time()
    if ip not in ip_request_log:
        ip_request_log[ip] = []
    
    # Filter logs within window
    ip_request_log[ip] = [t for t in ip_request_log[ip] if now - t < RATE_LIMIT_WINDOW]
    
    if len(ip_request_log[ip]) >= RATE_LIMIT_LIMIT:
        return True
    
    ip_request_log[ip].append(now)
    return False

def init_db():
    if not os.path.exists(UPLOAD_DIR):
        os.makedirs(UPLOAD_DIR)
        
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS users (
            uuid TEXT PRIMARY KEY,
            username TEXT UNIQUE,
            email TEXT UNIQUE,
            password TEXT,
            points INTEGER DEFAULT 1250,
            badges TEXT DEFAULT '["Founder"]',
            cosmetics TEXT DEFAULT '[]',
            equipped_cape TEXT DEFAULT '',
            equipped_wings TEXT DEFAULT '',
            equipped_hat TEXT DEFAULT '',
            equipped_aura TEXT DEFAULT '',
            skin_url TEXT DEFAULT '',
            cape_url TEXT DEFAULT ''
        )
    """)
    c.execute("""
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            client_token TEXT,
            uuid TEXT,
            FOREIGN KEY(uuid) REFERENCES users(uuid)
        )
    """)
    
    # Launcher settings storage
    c.execute("""
        CREATE TABLE IF NOT EXISTS config (
            key TEXT PRIMARY KEY,
            value TEXT
        )
    """)
    
    # Seed default launcher version config
    c.execute("INSERT OR IGNORE INTO config (key, value) VALUES ('version', '2.0.4')")
    c.execute("INSERT OR IGNORE INTO config (key, value) VALUES ('required', 'true')")
    c.execute("INSERT OR IGNORE INTO config (key, value) VALUES ('download_url', 'https://cdn.astraclient.com/launcher/AstraLauncher_2.0.4.exe')")
    c.execute("INSERT OR IGNORE INTO config (key, value) VALUES ('changelog', 'Added 3D skins, dynamic space themes, and optimized P2P encrypted lobbies.')")
    
    # Seed default test user if not exists
    try:
        test_uuid = "a7891234-abcd-1234-abcd-1234567890ab"
        c.execute("INSERT OR IGNORE INTO users (uuid, username, email, password) VALUES (?, ?, ?, ?)", 
                  (test_uuid, "AstraPlayer", "player@astraclient.com", "password123"))
    except Exception as e:
        print("Init database seeding error:", e)
        
    conn.commit()
    conn.close()

# Manual multipart parser compatible with Python 3.13+ (no cgi dependency)
def parse_multipart(content_bytes, boundary):
    parts = {}
    boundary_bytes = b"--" + boundary.encode("utf-8")
    split_parts = content_bytes.split(boundary_bytes)
    
    for part in split_parts:
        if not part or part.strip() == b"--" or part.strip() == b"":
            continue
        
        # Split headers and content
        header_body_split = part.split(b"\r\n\r\n", 1)
        if len(header_body_split) < 2:
            continue
        
        header_raw = header_body_split[0].decode("utf-8", errors="ignore")
        body = header_body_split[1]
        
        # Remove trailing carriage returns
        if body.endswith(b"\r\n"):
            body = body[:-2]
        
        name_match = re.search(r'name="([^"]+)"', header_raw)
        if not name_match:
            continue
            
        field_name = name_match.group(1)
        
        filename_match = re.search(r'filename="([^"]+)"', header_raw)
        if filename_match:
            filename = filename_match.group(1)
            parts[field_name] = {
                "filename": filename,
                "content": body
            }
        else:
            parts[field_name] = body.decode("utf-8", errors="ignore").strip()
            
    return parts

class AstraHandler(http.server.BaseHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS, PUT, DELETE')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Requested-With')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode("utf-8"))

    def get_body(self):
        content_length = int(self.headers.get('Content-Length', 0))
        if content_length == 0:
            return {}
        body = self.rfile.read(content_length).decode('utf-8')
        try:
            return json.loads(body)
        except Exception:
            return {}

    def handle_static_upload(self, path):
        # Prevent directory traversal attacks
        filename = os.path.basename(path)
        filepath = os.path.join(UPLOAD_DIR, filename)
        
        if not os.path.exists(filepath):
            self.send_json({"error": "File not found"}, 404)
            return

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(os.path.getsize(filepath)))
        self.end_headers()
        
        with open(filepath, "rb") as f:
            self.wfile.write(f.read())

    def do_GET(self):
        client_ip = self.client_address[0]
        if is_rate_limited(client_ip):
            self.send_json({"error": "DDoS Warning: Rate limit exceeded. Max 60 requests/min."}, 429)
            return

        parsed_url = urlparse(self.path)
        path = parsed_url.path
        query = parse_qs(parsed_url.query)

        # Statically serve files in /uploads/
        if path.startswith("/uploads/"):
            self.handle_static_upload(path[9:])
            return

        # ----------------------------------------------------
        # ADMIN PANEL GUI DASHBOARD ROUTE
        # ----------------------------------------------------
        if path == "/admin":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(self.get_admin_dashboard_html().encode("utf-8"))
            return

        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # Welcome Route
            if path == "/":
                self.send_json({
                    "status": "Astra Client Backend is online",
                    "version": "2.0.4",
                    "github": "https://github.com/CrafterStudioIndia/AstraCilent-Backend"
                }, 200)
                return

            # Profile Sync API
            profile_match = re.match(r"^/api/profile/([^/]+)$", path)
            if profile_match:
                identifier = profile_match.group(1)
                if '-' in identifier or len(identifier) == 32:
                    c.execute("SELECT * FROM users WHERE uuid = ?", (identifier,))
                else:
                    c.execute("SELECT * FROM users WHERE email = ? OR username = ?", (identifier, identifier))
                row = c.fetchone()
                if row:
                    user_data = dict(row)
                    res = {
                        "uuid": user_data["uuid"],
                        "username": user_data["username"],
                        "email": user_data["email"],
                        "points": user_data["points"],
                        "badges": json.loads(user_data["badges"] or '["Founder"]'),
                        "cosmetics": json.loads(user_data["cosmetics"] or '[]'),
                        "equipped": {
                            "cape": user_data["equipped_cape"],
                            "wings": user_data["equipped_wings"],
                            "hat": user_data["equipped_hat"],
                            "aura": user_data["equipped_aura"]
                        },
                        "skin_url": user_data["skin_url"],
                        "cape_url": user_data["cape_url"]
                    }
                    self.send_json(res, 200)
                else:
                    self.send_json({"error": "User not found"}, 404)
                return

            # Mod Bundle List API
            if path == "/api/mods/bundle":
                mods = [
                    {"name": "AstraCore-1.21.11.jar", "size": "4.2 MB", "hash": "a78d89fbc0d2", "url": "https://cdn.astraclient.com/mods/AstraCore.jar"},
                    {"name": "Sodium-1.21.11.jar", "size": "1.8 MB", "hash": "b23c91a0ef88", "url": "https://cdn.astraclient.com/mods/Sodium.jar"},
                    {"name": "Lithium-1.21.11.jar", "size": "0.9 MB", "hash": "f89d311029ba", "url": "https://cdn.astraclient.com/mods/Lithium.jar"},
                    {"name": "AstraHUD-1.21.11.jar", "size": "2.1 MB", "hash": "c1290bb098ef", "url": "https://cdn.astraclient.com/mods/AstraHUD.jar"}
                ]
                self.send_json(mods, 200)
                return

            # Launcher Updates Check API
            if path == "/api/launcher/version":
                c.execute("SELECT key, value FROM config")
                configs = {r["key"]: r["value"] for r in c.fetchall()}
                ver = {
                    "latest_version": configs.get("version", "2.0.4"),
                    "required": configs.get("required", "true") == "true",
                    "download_url": configs.get("download_url", ""),
                    "changelog": configs.get("changelog", "")
                }
                self.send_json(ver, 200)
                return

            # Yggdrasil Meta API (authlib-injector)
            if path == "/api/yggdrasil":
                meta = {
                    "meta": {
                        "serverName": "Astra Authentication",
                        "implementationName": "Astra Python Backend",
                        "implementationVersion": "1.0.0"
                    },
                    "skinDomains": [
                        "localhost", "astraclient.com", "cdn.astraclient.com", "imgur.com", "i.imgur.com"
                    ],
                    "signaturePublickey": "-----BEGIN PUBLIC KEY-----\nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0k1... (mock key)\n-----END PUBLIC KEY-----"
                }
                self.send_json(meta, 200)
                return

            # Yggdrasil hasJoined (Server validation)
            if path == "/api/yggdrasil/sessionserver/session/minecraft/hasJoined":
                username = query.get("username", [None])[0]
                if username:
                    c.execute("SELECT * FROM users WHERE username = ?", (username,))
                    row = c.fetchone()
                    if row:
                        user_data = dict(row)
                        stripped_id = user_data["uuid"].replace("-", "")
                        has_joined = {
                            "id": stripped_id,
                            "name": user_data["username"],
                            "properties": []
                        }
                        self.send_json(has_joined, 200)
                        return
                self.send_response(204)
                self.end_headers()
                return

            # Yggdrasil Profile Endpoint (Authlib Skin/Cape loader)
            yggdrasil_profile_match = re.match(r"^/api/yggdrasil/sessionserver/session/minecraft/profile/([^/]+)$", path)
            if yggdrasil_profile_match:
                stripped_id = yggdrasil_profile_match.group(1)
                
                # Restore hyphens to query sqlite
                uuid_str = stripped_id
                if len(uuid_str) == 32:
                    uuid_str = f"{uuid_str[0:8]}-{uuid_str[8:12]}-{uuid_str[12:16]}-{uuid_str[16:20]}-{uuid_str[20:32]}"

                c.execute("SELECT * FROM users WHERE uuid = ?", (uuid_str,))
                row = c.fetchone()
                if row:
                    user_data = dict(row)
                    
                    skin = user_data["skin_url"] if user_data["skin_url"] else "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d"
                    textures = {"SKIN": {"url": skin}}
                    
                    if user_data["equipped_cape"]:
                        cape = user_data["cape_url"] if user_data["cape_url"] else "https://textures.minecraft.net/texture/c7d3db9f71c4c1a5b4f8d22791e8477ffdf7867be7614e040f7bdf753b1b62"
                        textures["CAPE"] = {"url": cape}
                    elif user_data["cape_url"]:
                        textures["CAPE"] = {"url": user_data["cape_url"]}

                    texture_json = {
                        "timestamp": int(time.time() * 1000),
                        "profileId": stripped_id,
                        "profileName": user_data["username"],
                        "textures": textures
                    }

                    encoded_val = base64.b64encode(json.dumps(texture_json).encode("utf-8")).decode("utf-8")

                    profile_res = {
                        "id": stripped_id,
                        "name": user_data["username"],
                        "properties": [
                            {
                                "name": "textures",
                                "value": encoded_val
                            }
                        ]
                    }
                    self.send_json(profile_res, 200)
                else:
                    self.send_response(204)
                    self.end_headers()
                return

            # Admin Retrieve Users list
            if path == "/admin/api/users":
                # Simple header check for admin authentication
                auth_header = self.headers.get("Authorization")
                print(f"[Admin Auth Debug] Received Auth: '{auth_header}' | Expected: 'Bearer {ADMIN_PASSWORD}'")
                if auth_header != f"Bearer {ADMIN_PASSWORD}":
                    self.send_json({"error": "Unauthorized"}, 401)
                    return
                c.execute("SELECT uuid, username, email, points, badges, skin_url, cape_url FROM users")
                users_list = [dict(r) for r in c.fetchall()]
                self.send_json(users_list, 200)
                return

            self.send_json({"error": "Endpoint not found"}, 404)
        
        except Exception as e:
            self.send_json({"error": str(e)}, 500)
        finally:
            conn.close()

    def do_POST(self):
        client_ip = self.client_address[0]
        if is_rate_limited(client_ip):
            self.send_json({"error": "DDoS Warning: Rate limit exceeded. Max 60 requests/min."}, 429)
            return

        path = self.path
        
        # Handle file upload multipart/form-data for Admin EXE uploads
        if path == "/admin/api/upload":
            content_type = self.headers.get("Content-Type", "")
            auth_header = self.headers.get("Authorization", "")
            
            if auth_header != f"Bearer {ADMIN_PASSWORD}":
                self.send_json({"error": "Unauthorized"}, 401)
                return
                
            if "multipart/form-data" in content_type:
                boundary = content_type.split("boundary=")[1]
                content_length = int(self.headers.get("Content-Length", 0))
                body_bytes = self.rfile.read(content_length)
                
                try:
                    parts = parse_multipart(body_bytes, boundary)
                    
                    if "file" not in parts:
                        self.send_json({"error": "No file uploaded"}, 400)
                        return
                        
                    file_info = parts["file"]
                    filename = file_info["filename"]
                    content = file_info["content"]
                    
                    # Sanitize filename
                    filename = os.path.basename(filename)
                    filepath = os.path.join(UPLOAD_DIR, filename)
                    
                    with open(filepath, "wb") as f:
                        f.write(content)
                        
                    # Auto generate direct hosted Render URL link
                    host = self.headers.get("Host", "localhost:8080")
                    protocol = "https" if "render.com" in host or "koyeb" in host else "http"
                    file_url = f"{protocol}://{host}/uploads/{filename}"
                    
                    self.send_json({
                        "message": "File uploaded successfully!",
                        "filename": filename,
                        "url": file_url,
                        "size": f"{round(len(content)/(1024*1024), 2)} MB"
                    }, 200)
                except Exception as e:
                    self.send_json({"error": f"Upload parse error: {str(e)}"}, 500)
            else:
                self.send_json({"error": "Invalid Content-Type"}, 400)
            return

        # Handle regular JSON posts
        body = self.get_body()
        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # Custom User Registration API
            if path == "/api/register":
                username = body.get("username")
                email = body.get("email")
                password = body.get("password")

                if not username or not email or not password:
                    self.send_json({"error": "Missing parameters"}, 400)
                    return

                c.execute("SELECT COUNT(*) as count FROM users WHERE email = ? OR username = ?", (email, username))
                if c.fetchone()["count"] > 0:
                    self.send_json({"error": "Username or email already exists"}, 400)
                    return

                user_uuid = str(uuid.uuid4())
                c.execute("INSERT INTO users (uuid, username, email, password) VALUES (?, ?, ?, ?)", 
                          (user_uuid, username, email, password))
                conn.commit()
                self.send_json({"message": "User registered successfully", "uuid": user_uuid}, 201)
                return

            # Admin Edit User details
            if path == "/admin/api/users/update":
                auth_header = self.headers.get("Authorization")
                if auth_header != f"Bearer {ADMIN_PASSWORD}":
                    self.send_json({"error": "Unauthorized"}, 401)
                    return
                
                user_uuid = body.get("uuid")
                points = body.get("points")
                badges_list = body.get("badges") # list
                skin_url = body.get("skin_url", "")
                cape_url = body.get("cape_url", "")

                c.execute("UPDATE users SET points = ?, badges = ?, skin_url = ?, cape_url = ? WHERE uuid = ?", 
                          (points, json.dumps(badges_list), skin_url, cape_url, user_uuid))
                conn.commit()
                self.send_json({"message": "User updated successfully!"}, 200)
                return

            # Admin Edit Version configurations
            if path == "/admin/api/version/update":
                auth_header = self.headers.get("Authorization")
                if auth_header != f"Bearer {ADMIN_PASSWORD}":
                    self.send_json({"error": "Unauthorized"}, 401)
                    return

                version = body.get("version")
                required = body.get("required", "true")
                download_url = body.get("download_url")
                changelog = body.get("changelog")

                c.execute("UPDATE config SET value = ? WHERE key = 'version'", (version,))
                c.execute("UPDATE config SET value = ? WHERE key = 'required'", (required,))
                c.execute("UPDATE config SET value = ? WHERE key = 'download_url'", (download_url,))
                c.execute("UPDATE config SET value = ? WHERE key = 'changelog'", (changelog,))
                conn.commit()
                self.send_json({"message": "Version config updated successfully!"}, 200)
                return

            # Equip Cosmetic API
            if path == "/api/cosmetics/equip":
                user_uuid = body.get("uuid")
                slot = body.get("slot")
                item = body.get("item")

                column_map = {
                    "cape": "equipped_cape",
                    "wings": "equipped_wings",
                    "hat": "equipped_hat",
                    "aura": "equipped_aura"
                }

                if slot not in column_map:
                    self.send_json({"error": "Invalid slot"}, 400)
                    return

                col = column_map[slot]
                c.execute(f"UPDATE users SET {col} = ? WHERE uuid = ?", (item, user_uuid))
                conn.commit()
                self.send_json({"message": "Cosmetic equipped successfully", "slot": slot, "item": item}, 200)
                return

            # Buy Cosmetic API
            if path == "/api/cosmetics/purchase":
                user_uuid = body.get("uuid")
                item = body.get("item")
                cost = body.get("cost", 0)

                c.execute("SELECT points, cosmetics FROM users WHERE uuid = ?", (user_uuid,))
                row = c.fetchone()
                if row:
                    user_data = dict(row)
                    if user_data["points"] < cost:
                        self.send_json({"error": "Insufficient points"}, 400)
                        return

                    inv = json.loads(user_data["cosmetics"] or '[]')
                    if item not in inv:
                        inv.append(item)

                    new_points = user_data["points"] - cost
                    c.execute("UPDATE users SET cosmetics = ?, points = ? WHERE uuid = ?", 
                              (json.dumps(inv), new_points, user_uuid))
                    conn.commit()
                    self.send_json({"message": "Purchase successful", "points": new_points, "cosmetics": inv}, 200)
                else:
                    self.send_json({"error": "User not found"}, 404)
                return

            # Update Textures (Skin/Cape URL)
            if path == "/api/profile/textures":
                user_uuid = body.get("uuid")
                skin_url = body.get("skin_url", "")
                cape_url = body.get("cape_url", "")

                c.execute("UPDATE users SET skin_url = ?, cape_url = ? WHERE uuid = ?", 
                          (skin_url, cape_url, user_uuid))
                conn.commit()
                self.send_json({"message": "Textures updated successfully"}, 200)
                return

            # Add Points
            if path == "/api/points/add":
                user_uuid = body.get("uuid")
                amount = body.get("amount", 0)

                c.execute("UPDATE users SET points = points + ? WHERE uuid = ?", (amount, user_uuid))
                conn.commit()
                c.execute("SELECT points FROM users WHERE uuid = ?", (user_uuid,))
                new_points = c.fetchone()["points"]
                self.send_json({"message": "Points updated successfully", "points": new_points}, 200)
                return

            # Yggdrasil Authenticate API (authlib-injector Login)
            if path == "/api/yggdrasil/authserver/authenticate":
                username = body.get("username")
                password = body.get("password")
                client_token = body.get("clientToken", str(uuid.uuid4()))

                c.execute("SELECT * FROM users WHERE email = ? OR username = ?", (username, username))
                row = c.fetchone()
                if row:
                    user_data = dict(row)
                    if user_data["password"] == password:
                        access_token = str(uuid.uuid4())
                        c.execute("DELETE FROM sessions WHERE uuid = ?", (user_data["uuid"],))
                        c.execute("INSERT INTO sessions (token, client_token, uuid) VALUES (?, ?, ?)", 
                                  (access_token, client_token, user_data["uuid"]))
                        conn.commit()

                        stripped_id = user_data["uuid"].replace("-", "")

                        res = {
                            "accessToken": access_token,
                            "clientToken": client_token,
                            "selectedProfile": {
                                "id": stripped_id,
                                "name": user_data["username"]
                            },
                            "availableProfiles": [
                                {"id": stripped_id, "name": user_data["username"]}
                            ],
                            "user": {
                                "id": stripped_id,
                                "properties": []
                            }
                        }
                        self.send_json(res, 200)
                        return
                self.send_json({"error": "ForbiddenOperationException", "errorMessage": "Invalid credentials"}, 403)
                return

            # Yggdrasil Validate API
            if path == "/api/yggdrasil/authserver/validate":
                token = body.get("accessToken")
                c.execute("SELECT uuid FROM sessions WHERE token = ?", (token,))
                row = c.fetchone()
                if row:
                    self.send_response(204)
                    self.end_headers()
                else:
                    self.send_json({"error": "ForbiddenOperationException", "errorMessage": "Invalid token"}, 403)
                return

            # Yggdrasil Refresh API
            if path == "/api/yggdrasil/authserver/refresh":
                token = body.get("accessToken")
                client_token = body.get("clientToken", "")
                
                c.execute("SELECT uuid FROM sessions WHERE token = ?", (token,))
                row = c.fetchone()
                if row:
                    user_uuid = row["uuid"]
                    new_token = str(uuid.uuid4())
                    
                    c.execute("DELETE FROM sessions WHERE token = ?", (token,))
                    c.execute("INSERT INTO sessions (token, client_token, uuid) VALUES (?, ?, ?)", 
                              (new_token, client_token, user_uuid))
                    conn.commit()

                    c.execute("SELECT username FROM users WHERE uuid = ?", (user_uuid,))
                    user_row = c.fetchone()
                    stripped_id = user_uuid.replace("-", "")

                    res = {
                        "accessToken": new_token,
                        "clientToken": client_token,
                        "selectedProfile": {
                            "id": stripped_id,
                            "name": user_row["username"]
                        },
                        "user": {
                            "id": stripped_id,
                            "properties": []
                        }
                    }
                    self.send_json(res, 200)
                else:
                    self.send_json({"error": "ForbiddenOperationException", "errorMessage": "Invalid token"}, 403)
                return

            # Yggdrasil Invalidate API
            if path == "/api/yggdrasil/authserver/invalidate":
                token = body.get("accessToken")
                c.execute("DELETE FROM sessions WHERE token = ?", (token,))
                conn.commit()
                self.send_response(204)
                self.end_headers()
                return

            # Yggdrasil Signout API
            if path == "/api/yggdrasil/authserver/signout":
                self.send_response(204)
                self.end_headers()
                return

            # Yggdrasil Join API
            if path == "/api/yggdrasil/sessionserver/session/minecraft/join":
                self.send_response(204)
                self.end_headers()
                return

            self.send_json({"error": "Endpoint not found"}, 404)

        except Exception as e:
            self.send_json({"error": str(e)}, 500)
        finally:
            conn.close()

    # Admin Control Panel HTML/CSS layout (consistently bundled in C++ & Python servers)
    def get_admin_dashboard_html(self):
        return """<!DOCTYPE html>
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
        
        /* Users Table Styling */
        table { width: 100%; border-collapse: collapse; margin-top: 15px; font-size: 13px; text-align: left; }
        th, td { padding: 12px; border-bottom: 1px solid rgba(255,255,255,0.05); }
        th { color: #bb86fc; font-weight: 700; }
        tr:hover { background: rgba(255,255,255,0.02); }
        .badge { padding: 3px 8px; border-radius: 10px; font-size: 10px; font-weight: 700; background: rgba(255,255,255,0.1); margin-right: 4px; }
        .badge-founder { background: rgba(255, 215, 0, 0.15); color: #ffd700; border: 1px solid #ffd700; }
        .badge-staff { background: rgba(233, 30, 99, 0.15); color: #e91e63; border: 1px solid #e91e63; }
        .badge-beta { background: rgba(0, 230, 118, 0.15); color: #00e676; border: 1px solid #00e676; }
        
        /* File Upload styling */
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

        /* User Edit Modal overlay */
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
            <!-- Left Side: Users list -->
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
                    <tbody>
                        <!-- Users go here -->
                    </tbody>
                </table>
            </div>

            <!-- Right Side: Executable Updates & Config -->
            <div class="flex-col">
                <div class="glass-card">
                    <h3>Launcher Update Panel</h3>
                    <p style="font-size:12px; color:#888; margin-bottom:15px;">Drag & drop your new updates (.exe or mods) here to upload them directly to the Render server.</p>
                    
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

    <!-- Edit User Modal -->
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

        if (adminToken) {
            showDashboard();
        }

        function authenticateAdmin() {
            const pwd = document.getElementById("adminPasswordInput").value;
            const err = document.getElementById("authError");
            
            // To verify token, query users using bearer
            fetch(`${hostUrl}/admin/api/users`, {
                headers: { "Authorization": `Bearer ${pwd}` }
            })
            .then(res => {
                if(res.ok) {
                    adminToken = pwd;
                    localStorage.setItem("astra_admin_token", adminToken);
                    showDashboard();
                } else {
                    err.innerText = "Access Denied: Invalid Admin Credentials.";
                }
            })
            .catch(e => err.innerText = "Error contacting API backend.");
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
            fetch(`${hostUrl}/admin/api/users`, {
                headers: { "Authorization": `Bearer ${adminToken}` }
            })
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
                headers: { 
                    "Content-Type": "application/json",
                    "Authorization": `Bearer ${adminToken}`
                },
                body: JSON.stringify({ version, download_url, changelog })
            })
            .then(res => {
                if (res.ok) {
                    alert("Launcher version config deployed successfully!");
                    loadVersionConfig();
                } else {
                    alert("Unauthorized session.");
                }
            });
        }

        // Trigger drag-and-drop file inputs
        function triggerFileInput() {
            document.getElementById("fileInput").click();
        }

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
                    text.innerText = "File Uploaded Successfully!";
                    
                    // Auto fill executable url if uploaded .exe
                    if(file.name.endsWith(".exe")) {
                        document.getElementById("configDownloadUrl").value = data.url;
                    }
                } else {
                    status.style.color = "#ff5252";
                    status.innerText = data.error || "Upload failed.";
                }
            })
            .catch(e => {
                status.style.color = "#ff5252";
                status.innerText = "Server upload connection error.";
            });
        }

        // Modal triggers
        function openEditModal(uuid, name, points, skin, cape, badgesJson) {
            document.getElementById("editUuid").value = uuid;
            document.getElementById("editModalTitle").innerText = `Edit: ${name}`;
            document.getElementById("editPoints").value = points;
            document.getElementById("editSkin").value = skin;
            document.getElementById("editCape").value = cape;
            
            // Clear checked
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

        function closeEditModal() {
            document.getElementById("editModal").style.display = "none";
        }

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
                headers: {
                    "Content-Type": "application/json",
                    "Authorization": `Bearer ${adminToken}`
                },
                body: JSON.stringify({ uuid, points, skin_url, cape_url, badges })
            })
            .then(res => {
                if(res.ok) {
                    alert("User data updated successfully!");
                    closeEditModal();
                    loadUsers();
                } else {
                    alert("Failed to update user.");
                }
            });
        }
    </script>
</body>
</html>"""

if __name__ == "__main__":
    init_db()
    print(f"Database initialized. Astra Client Backend running on port {PORT}")
    
    # Allow address reuse for quick redeploy cycles
    socketserver.TCPServer.allow_reuse_address = True
    
    with socketserver.TCPServer(("0.0.0.0", PORT), AstraHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping server...")
            httpd.server_close()
