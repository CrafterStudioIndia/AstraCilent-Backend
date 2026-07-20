# 🌌 Astra Client Launcher & Backend

Astra Client is a custom Minecraft client ecosystem featuring an interactive glassmorphism launcher interface, a player skin & cape customizer portal, and a lightweight Yggdrasil-compatible backend server.

---

## 🚀 Features

### 1. 🌐 Cloud Profile & Skin Injection (Yggdrasil API)
- Implements Mojang's Yggdrasil Authentication Protocol endpoints compatible with `authlib-injector`.
- Automatically base64-encodes user skin and cape texture URLs in profile lookups.
- Enables custom skins & capes to render in-game in singleplayer and multiplayer for all client users completely for free.
- Uses SQLite to manage accounts, session keys, points balance, and cosmetics inventory.

### 2. 🎨 Glassmorphic Desktop Launcher UI
- Translucent layouts with glowing borders and parallax particle starfields.
- Dynamic theme selector support (**Cosmic Purple**, **Supernova Red**, **Nebula Blue**, **Eclipse Black**).
- Draggable **PvP HUD Editor** (CPS tracker, Keystrokes WASD, Armor status, Potion timers) and a custom crosshair designer.
- **P2P Multiplayer Lobbies**: Masks public IP addresses through secure encrypted relay tunnels using STUN/TURN traversal.
- Fullscreen boot sequence emulator showing mod bundle verification, downloads, and a mock game preview overlay.

### 3. 👕 Netlify / Vercel Web Portal
- Glassmorphic login and registration panels.
- **Cosmetics Wardrobe**: Buy items (Astra Wings, Cosmic Capes, Neon Crowns, Fire Auras) using points, and equip/unequip them.
- **Points Economy**: Simulators to claim daily rewards, submit referrals, or report client bugs.
- **Interactive 3D Player Model**: Uses `skinview3d` and `Three.js` to render a rotatable, fully-animated 3D Steve model overlaying custom skins, capes, and equipped auras.

---

## 📂 Project Structure

```
├── backend/
│   ├── main.cpp             # C++ HTTP API Server (Yggdrasil endpoints)
│   ├── database.hpp         # C++ SQLite3 schema & transaction mapping
│   ├── run_backend.py       # Python 3 fallback server (runs zero-config)
│   ├── Dockerfile           # Docker configuration for cloud hosting
│   └── README_HOSTING.md    # Guide to deploy backend for free on Render/Koyeb
├── web_portal/
│   ├── index.html           # Web Wardrobe frontend layout
│   ├── style.css            # Portal glassmorphism styles & custom themes
│   └── app.js               # 3D model skinview3d rendering & API routing
├── index.html               # Launcher Desktop Dashboard layout
├── styles.css               # Launcher UI layouts, grid editor, overlays
├── app.js                   # HUD drag-and-drop, P2P handshake, launcher logic
└── .gitignore               # Excludes database files and built binaries
```

---

## 🛠️ How to Host the Backend for Free

You can deploy the backend web server to **Render** or **Koyeb** for free in just a few clicks:
1. Create a free account on [Render](https://render.com).
2. Create a new **Web Service** and link this GitHub repository.
3. Set the **Root Directory** to `backend` and set the **Language** to `Docker` (it will build automatically using our `Dockerfile`).
4. Click **Deploy**. Your API will be live at `https://your-service-name.onrender.com`!

---

## 🔗 Customization Settings
To link your web portal and launcher to your live backend:
- Open the settings panel inside the web portal or launcher, and change the **Backend Base URL** from `http://localhost:8080` to your hosted Render/Koyeb link.
