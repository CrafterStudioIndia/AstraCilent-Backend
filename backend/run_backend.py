import http.server
import socketserver
import json
import sqlite3
import uuid
import base64
import time
import re
from urllib.parse import urlparse, parse_qs

PORT = 8080
DB_FILE = "astra.db"

def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    # Users table
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
    # Sessions table
    c.execute("""
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            client_token TEXT,
            uuid TEXT,
            FOREIGN KEY(uuid) REFERENCES users(uuid)
        )
    """)
    # Insert a test user if not exists
    try:
        test_uuid = "a7891234-abcd-1234-abcd-1234567890ab"
        c.execute("INSERT OR IGNORE INTO users (uuid, username, email, password) VALUES (?, ?, ?, ?)", 
                  (test_uuid, "AstraPlayer", "player@astraclient.com", "password123"))
    except Exception as e:
        print("Init test user error:", e)
        
    conn.commit()
    conn.close()

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

    def do_GET(self):
        parsed_url = urlparse(self.path)
        path = parsed_url.path
        query = parse_qs(parsed_url.query)

        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # 0. Root Welcome API
            if path == "/":
                self.send_json({
                    "status": "Astra Client Backend is online",
                    "version": "2.0.4",
                    "github": "https://github.com/CrafterStudioIndia/AstraCilent-Backend"
                }, 200)
                return

            # 1. Profile Sync API
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

            # 2. Mod Bundle List API
            if path == "/api/mods/bundle":
                mods = [
                    {"name": "AstraCore-1.21.11.jar", "size": "4.2 MB", "hash": "a78d89fbc0d2", "url": "https://cdn.astraclient.com/mods/AstraCore.jar"},
                    {"name": "Sodium-1.21.11.jar", "size": "1.8 MB", "hash": "b23c91a0ef88", "url": "https://cdn.astraclient.com/mods/Sodium.jar"},
                    {"name": "Lithium-1.21.11.jar", "size": "0.9 MB", "hash": "f89d311029ba", "url": "https://cdn.astraclient.com/mods/Lithium.jar"},
                    {"name": "AstraHUD-1.21.11.jar", "size": "2.1 MB", "hash": "c1290bb098ef", "url": "https://cdn.astraclient.com/mods/AstraHUD.jar"}
                ]
                self.send_json(mods, 200)
                return

            # 3. Launcher Updates Check API
            if path == "/api/launcher/version":
                ver = {
                    "latest_version": "2.0.4",
                    "required": True,
                    "download_url": "https://cdn.astraclient.com/launcher/AstraLauncher_2.0.4.exe",
                    "changelog": "Added 3D skins, dynamic space themes, and optimized P2P encrypted lobbies."
                }
                self.send_json(ver, 200)
                return

            # 4. Yggdrasil Meta API (authlib-injector)
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

            # 5. Yggdrasil hasJoined (Server validation)
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

            # 6. Yggdrasil Profile Endpoint (Authlib Skin/Cape loader)
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
                    
                    # Default Skin fallback if empty
                    skin = user_data["skin_url"] if user_data["skin_url"] else "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d"
                    
                    textures = {
                        "SKIN": {"url": skin}
                    }
                    
                    # Set cape if equipped
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

                    # Base64 encode textures JSON
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
                    self.send_response(204) # Not found
                    self.end_headers()
                return

            # Default fallback
            self.send_json({"error": "Endpoint not found"}, 404)
        
        except Exception as e:
            self.send_json({"error": str(e)}, 500)
        finally:
            conn.close()

    def do_POST(self):
        path = self.path
        body = self.get_body()

        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        c = conn.cursor()

        try:
            # 1. Custom User Registration API
            if path == "/api/register":
                username = body.get("username")
                email = body.get("email")
                password = body.get("password")

                if not username or not email or not password:
                    self.send_json({"error": "Missing parameters"}, 400)
                    return

                # Check existence
                c.execute("SELECT COUNT(*) as count FROM users WHERE email = ? OR username = ?", (email, username))
                if c.fetchone()["count"] > 0:
                    self.send_json({"error": "Username or email already exists"}, 400)
                    return

                user_uuid = str(uuid.uuid4())
                c.execute("INSERT INTO users (uuid, username, email, password) VALUES (?, ?, ?, ?)", 
                          (user_uuid, username, email, password))
                conn.commit()
                self.send_json({"message": "User registered successfully", "uuid": user_uuid}, 21)
                return

            # 2. Equip Cosmetic API
            if path == "/api/cosmetics/equip":
                user_uuid = body.get("uuid")
                slot = body.get("slot") # cape, wings, hat, aura
                item = body.get("item") # item name or empty to unequip

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

            # 3. Buy Cosmetic API
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

            # 4. Update Textures (Skin/Cape URL)
            if path == "/api/profile/textures":
                user_uuid = body.get("uuid")
                skin_url = body.get("skin_url", "")
                cape_url = body.get("cape_url", "")

                c.execute("UPDATE users SET skin_url = ?, cape_url = ? WHERE uuid = ?", 
                          (skin_url, cape_url, user_uuid))
                conn.commit()
                self.send_json({"message": "Textures updated successfully"}, 200)
                return

            # 5. Add Points (Daily Reward/Referral simulator)
            if path == "/api/points/add":
                user_uuid = body.get("uuid")
                amount = body.get("amount", 0)

                c.execute("UPDATE users SET points = points + ? WHERE uuid = ?", (amount, user_uuid))
                conn.commit()
                c.execute("SELECT points FROM users WHERE uuid = ?", (user_uuid,))
                new_points = c.fetchone()["points"]
                self.send_json({"message": "Points updated successfully", "points": new_points}, 200)
                return

            # 6. Yggdrasil Authenticate API (authlib-injector Login)
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
                        # Clear previous sessions
                        c.execute("DELETE FROM sessions WHERE uuid = ?", (user_data["uuid"],))
                        # Create new session
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

            # 7. Yggdrasil Validate API
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

            # 8. Yggdrasil Refresh API
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

            # 9. Yggdrasil Invalidate API
            if path == "/api/yggdrasil/authserver/invalidate":
                token = body.get("accessToken")
                c.execute("DELETE FROM sessions WHERE token = ?", (token,))
                conn.commit()
                self.send_response(204)
                self.end_headers()
                return

            # 10. Yggdrasil Signout API
            if path == "/api/yggdrasil/authserver/signout":
                self.send_response(204)
                self.end_headers()
                return

            # 11. Yggdrasil Join API
            if path == "/api/yggdrasil/sessionserver/session/minecraft/join":
                self.send_response(204)
                self.end_headers()
                return

            self.send_json({"error": "Endpoint not found"}, 404)

        except Exception as e:
            self.send_json({"error": str(e)}, 500)
        finally:
            conn.close()

if __name__ == "__main__":
    init_db()
    print("Database initialized.")
    httpd = socketserver.TCPServer(("0.0.0.0", PORT), AstraHandler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        httpd.server_close()
