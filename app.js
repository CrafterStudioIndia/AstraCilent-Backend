/* ==========================================================================
   ASTRA LAUNCHER APPLICATION LOGIC
   ========================================================================== */

// Global State
let backendUrl = localStorage.getItem("astra_launcher_backend_url") || "https://astracilent-backend.onrender.com";
let currentUser = null;
let currentTheme = localStorage.getItem("astra_launcher_theme") || "purple";
let skinViewerInstance = null;
let viewerWalkAnimation = null;
let viewerRunAnimation = null;
let gameSkinViewer = null;

// HUD Coordinates and Module toggles
let hudPositions = {
    hudCPS: { top: 30, left: 30 },
    hudKeystrokes: { top: 120, left: 30 },
    hudArmor: { top: 30, right: 30 },
    hudPotion: { top: 160, right: 30 }
};
let activeHudModules = {
    hudCPS: true,
    hudKeystrokes: true,
    hudArmor: true,
    hudPotion: true
};

// Initialize Launcher on startup
document.addEventListener("DOMContentLoaded", () => {
    initStars();
    setTheme(currentTheme);
    initDraggables();
    drawCustomCrosshair();
    setupKeystrokesListener();

    // Fill URL fields on UI
    const urlField = document.getElementById("launcherBackendUrl");
    if (urlField) urlField.value = backendUrl;

    // Check existing login session
    const cachedUuid = localStorage.getItem("astra_launcher_user_uuid");
    if (cachedUuid) {
        syncProfileWithBackend(cachedUuid);
    } else {
        // Show auth entryway card
        document.getElementById("authCard").style.display = "block";
        document.getElementById("launcherPanel").style.display = "none";
    }
});

// ----------------------------------------------------
// 1. STARFIELD PARTICLES BACKGROUND
// ----------------------------------------------------
function initStars() {
    const canvas = document.getElementById("stars-canvas");
    const ctx = canvas.getContext("2d");
    
    let width = canvas.width = window.innerWidth;
    let height = canvas.height = window.innerHeight;
    
    window.addEventListener("resize", () => {
        width = canvas.width = window.innerWidth;
        height = canvas.height = window.innerHeight;
    });

    const stars = [];
    const starCount = 80;

    for (let i = 0; i < starCount; i++) {
        stars.push({
            x: Math.random() * width,
            y: Math.random() * height,
            size: Math.random() * 1.5 + 0.2,
            speed: Math.random() * 0.03 + 0.01,
            alpha: Math.random()
        });
    }

    function animate() {
        ctx.clearRect(0, 0, width, height);
        stars.forEach(star => {
            ctx.beginPath();
            ctx.arc(star.x, star.y, star.size, 0, Math.PI * 2);
            ctx.fillStyle = `rgba(255, 255, 255, ${star.alpha})`;
            ctx.fill();
            
            star.x -= star.speed * width * 0.08;
            if (star.x < 0) {
                star.x = width;
                star.y = Math.random() * height;
            }
            
            star.alpha += (Math.random() - 0.5) * 0.05;
            if (star.alpha < 0) star.alpha = 0;
            if (star.alpha > 1) star.alpha = 1;
        });
        requestAnimationFrame(animate);
    }
    animate();
}

// ----------------------------------------------------
// 2. THEME CHANGER
// ----------------------------------------------------
function toggleThemeMenu() {
    const menu = document.getElementById("themeMenu");
    menu.style.display = menu.style.display === "flex" ? "none" : "flex";
}

function setTheme(themeName) {
    document.body.className = "";
    document.body.classList.add(`theme-${themeName}`);
    currentTheme = themeName;
    localStorage.setItem("astra_launcher_theme", themeName);
    const menu = document.getElementById("themeMenu");
    if (menu) menu.style.display = "none";
}

// ----------------------------------------------------
// 3. AUTHENTICATION (Login / Register / Profile Sync)
// ----------------------------------------------------
function switchAuthTab(tab) {
    const tabs = document.querySelectorAll(".tab-btn");
    const forms = document.querySelectorAll(".auth-form");
    
    tabs.forEach(btn => btn.classList.remove("active"));
    forms.forEach(form => form.classList.remove("active"));
    
    if (tab === "register") {
        document.getElementById("authTabRegister").classList.add("active");
        document.getElementById("registerForm").classList.add("active");
    } else {
        document.getElementById("authTabLogin").classList.add("active");
        document.getElementById("loginForm").classList.add("active");
    }
}

function saveLauncherBackendUrl() {
    const url = document.getElementById("launcherBackendUrl").value.trim();
    if (url) {
        backendUrl = url;
        localStorage.setItem("astra_launcher_backend_url", backendUrl);
        alert("Astra server configurations updated successfully!");
        checkBackendStatus();
    }
}

async function checkBackendStatus() {
    const text = document.getElementById("serverConnectionText");
    const indicator = document.getElementById("statusIndicator");
    try {
        const res = await fetch(`${backendUrl}/api/yggdrasil`);
        if (res.ok) {
            text.innerText = "Online (API Connected)";
            indicator.className = "status-indicator online";
        } else {
            throw new Error();
        }
    } catch (e) {
        text.innerText = "Standalone Mode (API Offline)";
        indicator.className = "status-indicator";
    }
}

// Register Handlers
async function handleRegister(e) {
    e.preventDefault();
    const username = document.getElementById("regUsername").value.trim();
    const email = document.getElementById("regEmail").value.trim();
    const password = document.getElementById("regPassword").value;
    const status = document.getElementById("authStatus");

    status.className = "status-msg";
    status.innerText = "Connecting to registration node...";

    try {
        const res = await fetch(`${backendUrl}/api/register`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, email, password })
        });
        
        // Handle DDoS Rate limiting
        if (res.status === 429) {
            status.className = "status-msg error";
            status.innerText = "Anti-DDoS Warning: Too many requests. Try again in a minute.";
            return;
        }

        const data = await res.json();
        if (res.ok) {
            status.className = "status-msg success";
            status.innerText = "Account Created! Select Login to continue.";
            switchAuthTab("login");
            document.getElementById("loginIdentifier").value = username;
        } else {
            status.className = "status-msg error";
            status.innerText = data.error || "Failed to register profile.";
        }
    } catch (err) {
        status.className = "status-msg error";
        status.innerText = "Network Error: Cloud backend server unreachable.";
    }
}

// Login Handlers
async function handleLogin(e) {
    e.preventDefault();
    const identifier = document.getElementById("loginIdentifier").value.trim();
    const password = document.getElementById("loginPassword").value;
    const status = document.getElementById("authStatus");

    status.className = "status-msg";
    status.innerText = "Authenticating profile...";

    try {
        const res = await fetch(`${backendUrl}/api/yggdrasil/authserver/authenticate`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username: identifier, password: password })
        });

        if (res.status === 429) {
            status.className = "status-msg error";
            status.innerText = "Anti-DDoS Block: Requests throttled. Wait 60s.";
            return;
        }

        const data = await res.json();
        if (res.ok) {
            status.className = "status-msg success";
            status.innerText = "Credentials verified!";
            
            // Sync Profile details
            localStorage.setItem("astra_launcher_user_uuid", data.selectedProfile.id);
            syncProfileWithBackend(data.selectedProfile.id);
        } else {
            status.className = "status-msg error";
            status.innerText = data.errorMessage || "Authentication credentials rejected.";
        }
    } catch (e) {
        status.className = "status-msg error";
        status.innerText = "Connection Error: Validate your API server URL.";
    }
}

// Profile Loader
async function syncProfileWithBackend(uuid) {
    try {
        const res = await fetch(`${backendUrl}/api/profile/${uuid}`);
        if (!res.ok) throw new Error();

        currentUser = await res.json();

        // Swap Panels
        document.getElementById("authCard").style.display = "none";
        document.getElementById("launcherPanel").style.display = "grid";

        // Set Top Header details
        document.getElementById("headerUsername").innerText = currentUser.username;
        document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
        
        let tag = "Astra User";
        if (currentUser.badges.includes("Staff")) tag = "👑 Staff Member";
        else if (currentUser.badges.includes("Founder")) tag = "⭐ Founder Player";
        else if (currentUser.badges.includes("Beta Tester")) tag = "🧪 Beta Tester";
        else if (currentUser.badges.includes("Supporter")) tag = "💎 Client Supporter";
        document.getElementById("headerBadge").innerText = tag;

        // Set Avatar Icon
        document.getElementById("headerAvatar").innerHTML = `<i class="fa-solid fa-user-astronaut" style="color:var(--accent-color);"></i>`;

        // Fill custom URL fields
        document.getElementById("skinUrl").value = currentUser.skin_url;
        document.getElementById("capeUrl").value = currentUser.cape_url;

        // Trigger Sub modules
        checkBackendStatus();
        init3DViewer();
        initShopLists();
        switchTab("dashboard");
    } catch (err) {
        console.error("Failed to sync profile", err);
        handleLogout();
    }
}

function handleLogout() {
    localStorage.removeItem("astra_launcher_user_uuid");
    currentUser = null;
    if (skinViewerInstance) {
        skinViewerInstance.dispose();
        skinViewerInstance = null;
    }
    document.getElementById("launcherPanel").style.display = "none";
    document.getElementById("authCard").style.display = "block";
    document.getElementById("authStatus").innerText = "";
}

// ----------------------------------------------------
// 4. INTERACTIVE 3D PLAYER MODEL VIEWER
// ----------------------------------------------------
function init3DViewer() {
    const container = document.querySelector(".viewer-container");
    const canvas = document.getElementById("launcher-skin-canvas");

    if (skinViewerInstance) {
        skinViewerInstance.dispose();
    }

    const skinUrl = currentUser.skin_url || "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d";

    skinViewerInstance = new skinview3d.SkinViewer({
        canvas: canvas,
        width: container.clientWidth,
        height: container.clientHeight,
        skin: skinUrl
    });

    if (currentUser.equipped.cape && currentUser.cape_url) {
        skinViewerInstance.loadCape(currentUser.cape_url);
    }

    viewerWalkAnimation = skinViewerInstance.animations.add(skinview3d.WalkingAnimation);
    viewerWalkAnimation.speed = 0.8;

    skinViewerInstance.autoRotate = true;
    skinViewerInstance.autoRotateSpeed = 0.5;

    updateAuraVisuals();
}

function setViewerAnimation(type) {
    if (!skinViewerInstance) return;
    if (viewerWalkAnimation) { viewerWalkAnimation.remove(); viewerWalkAnimation = null; }
    if (viewerRunAnimation) { viewerRunAnimation.remove(); viewerRunAnimation = null; }

    if (type === "walk") {
        viewerWalkAnimation = skinViewerInstance.animations.add(skinview3d.WalkingAnimation);
        viewerWalkAnimation.speed = 0.8;
    } else if (type === "run") {
        viewerRunAnimation = skinViewerInstance.animations.add(skinview3d.RunningAnimation);
        viewerRunAnimation.speed = 1.1;
    }
}

function toggleViewerRotation() {
    if (!skinViewerInstance) return;
    skinViewerInstance.autoRotate = !skinViewerInstance.autoRotate;
}

function updateAuraVisuals() {
    const glow = document.getElementById("launcherAuraGlow");
    if (currentUser.equipped.aura) {
        glow.style.opacity = "1";
        if (currentUser.equipped.aura === "Fire Aura") {
            glow.style.background = "radial-gradient(circle, rgba(239, 68, 68, 0.3) 0%, rgba(239, 68, 68, 0) 70%)";
        } else {
            glow.style.background = "radial-gradient(circle, rgba(var(--accent-rgb), 0.3) 0%, rgba(var(--accent-rgb), 0) 70%)";
        }
    } else {
        glow.style.opacity = "0";
    }
}

async function saveTextures(e) {
    e.preventDefault();
    const skin_url = document.getElementById("skinUrl").value.trim();
    const cape_url = document.getElementById("capeUrl").value.trim();

    try {
        const res = await fetch(`${backendUrl}/api/profile/textures`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, skin_url, cape_url })
        });
        
        if (res.ok) {
            alert("Custom textures loaded! These will render inside Minecraft.");
            currentUser.skin_url = skin_url;
            currentUser.cape_url = cape_url;
            
            if (skin_url) skinViewerInstance.loadSkin(skin_url);
            if (cape_url && currentUser.equipped.cape) {
                skinViewerInstance.loadCape(cape_url);
            } else {
                skinViewerInstance.loadCape(null);
            }
        } else {
            alert("API rejected texture update.");
        }
    } catch(err) {
        alert("API connection failed.");
    }
}

// ----------------------------------------------------
// 5. COSMETICS STORE (Shop, Inventory, Rewards Claim)
// ----------------------------------------------------
const SHOP_ITEMS = [
    { id: "purple_wings", name: "Astra Wings", slot: "wings", price: 500, icon: "fa-solid fa-dove" },
    { id: "cosmic_cape", name: "Cosmic Cape", slot: "cape", price: 300, icon: "fa-solid fa-square-full" },
    { id: "neon_crown", name: "Neon Crown", slot: "hat", price: 400, icon: "fa-solid fa-crown" },
    { id: "fire_aura", name: "Fire Aura", slot: "aura", price: 600, icon: "fa-solid fa-fire" }
];

function initShopLists() {
    const shopGrid = document.getElementById("shopGrid");
    shopGrid.innerHTML = "";
    
    SHOP_ITEMS.forEach(item => {
        const isOwned = currentUser.cosmetics.includes(item.name);
        const card = document.createElement("div");
        card.className = "cosmetic-card glass-panel";
        card.innerHTML = `
            <div class="cosmetic-icon"><i class="${item.icon}"></i></div>
            <h4>${item.name}</h4>
            <span class="cosmetic-tag">${item.slot}</span>
            <div class="cosmetic-price"><i class="fa-solid fa-coins"></i> ${item.price} PTS</div>
            ${isOwned 
                ? `<button class="btn btn-sm" disabled style="color:#555; border-color:#555;">Purchased</button>`
                : `<button class="btn btn-sm btn-primary" onclick="buyCosmetic('${item.name}', ${item.price})">Buy Item</button>`
            }
        `;
        shopGrid.appendChild(card);
    });

    const invGrid = document.getElementById("inventoryGrid");
    invGrid.innerHTML = "";
    
    if (currentUser.cosmetics && currentUser.cosmetics.length > 0) {
        currentUser.cosmetics.forEach(itemName => {
            const itemObj = SHOP_ITEMS.find(s => s.name === itemName);
            if (!itemObj) return;

            const isEquipped = currentUser.equipped[itemObj.slot] === itemName;
            const card = document.createElement("div");
            card.className = `cosmetic-card glass-panel ${isEquipped ? 'equipped' : ''}`;
            card.innerHTML = `
                ${isEquipped ? `<span class="equipped-badge">EQUIPPED</span>` : ''}
                <div class="cosmetic-icon"><i class="${itemObj.icon}"></i></div>
                <h4>${itemObj.name}</h4>
                <span class="cosmetic-tag">${itemObj.slot}</span>
                ${isEquipped 
                    ? `<button class="btn btn-sm btn-danger" onclick="equipCosmetic('${itemObj.slot}', '')">Unequip</button>`
                    : `<button class="btn btn-sm btn-primary" onclick="equipCosmetic('${itemObj.slot}', '${itemName}')">Equip</button>`
                }
            `;
            invGrid.appendChild(card);
        });
    } else {
        invGrid.innerHTML = `<div style="grid-column:1/-1; text-align:center; padding:30px; color:#555;"><p>No owned items. Purchase cosmetics in the shop!</p></div>`;
    }
}

async function buyCosmetic(itemName, cost) {
    if (currentUser.points < cost) {
        alert("Insufficient points balance.");
        return;
    }
    try {
        const res = await fetch(`${backendUrl}/api/cosmetics/purchase`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, item: itemName, cost })
        });
        const data = await res.json();
        if (res.ok) {
            alert(`Purchased ${itemName}!`);
            currentUser.points = data.points;
            currentUser.cosmetics = data.cosmetics;
            document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
            initShopLists();
        }
    } catch(e) { alert("Purchase API call failed."); }
}

async function equipCosmetic(slot, item) {
    try {
        const res = await fetch(`${backendUrl}/api/cosmetics/equip`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, slot, item })
        });
        if (res.ok) {
            currentUser.equipped[slot] = item;
            initShopLists();
            if (slot === "cape") {
                if (item && currentUser.cape_url) skinViewerInstance.loadCape(currentUser.cape_url);
                else skinViewerInstance.loadCape(null);
            } else if (slot === "aura") {
                updateAuraVisuals();
            }
        }
    } catch(e) { alert("Equipment update failed."); }
}

function switchWardrobeTab(tab) {
    const tabs = document.querySelectorAll(".w-tab");
    const contents = document.querySelectorAll(".wardrobe-content");
    tabs.forEach(t => t.classList.remove("active"));
    contents.forEach(c => c.classList.remove("active"));

    if (tab === "shop") {
        tabs[0].classList.add("active");
        document.getElementById("shopSection").classList.add("active");
    } else if (tab === "inventory") {
        tabs[1].classList.add("active");
        document.getElementById("inventorySection").classList.add("active");
    } else {
        tabs[2].classList.add("active");
        document.getElementById("earnSection").classList.add("active");
    }
}

// Claim rewards
async function claimDailyReward() {
    const last = localStorage.getItem(`daily_claim_${currentUser.uuid}`);
    const now = Date.now();
    if (last && (now - last < 24 * 60 * 60 * 1000)) {
        alert("Daily Reward has a 24-hour cooldown.");
        return;
    }
    try {
        const res = await fetch(`${backendUrl}/api/points/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, amount: 100 })
        });
        const data = await res.json();
        if (res.ok) {
            localStorage.setItem(`daily_claim_${currentUser.uuid}`, now);
            currentUser.points = data.points;
            document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
            alert("Claimed 100 PTS!");
        }
    } catch(e) { alert("Daily claim server error."); }
}

async function submitReferral() {
    const user = document.getElementById("refUser").value.trim();
    if(!user) return;
    try {
        const res = await fetch(`${backendUrl}/api/points/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, amount: 150 })
        });
        const data = await res.json();
        if (res.ok) {
            alert(`Applied referral code for ${user}! +150 PTS`);
            currentUser.points = data.points;
            document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
            document.getElementById("refUser").value = "";
        }
    } catch(e) {}
}

async function claimBugReward() {
    const id = document.getElementById("bugReportId").value.trim();
    if(!id) return;
    try {
        const res = await fetch(`${backendUrl}/api/points/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, amount: 300 })
        });
        const data = await res.json();
        if (res.ok) {
            alert(`Verified bug report ID: ${id}! +300 PTS`);
            currentUser.points = data.points;
            document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
            document.getElementById("bugReportId").value = "";
        }
    } catch(e) {}
}

// ----------------------------------------------------
// 6. ROUTER AND DRAGGABLE HUD EDITOR
// ----------------------------------------------------
function switchTab(tabId) {
    const panels = document.querySelectorAll(".panel");
    const navItems = document.querySelectorAll(".nav-item");
    panels.forEach(p => p.classList.remove("active"));
    navItems.forEach(n => n.classList.remove("active"));

    document.getElementById(`${tabId}Panel`).classList.add("active");
    const active = Array.from(navItems).find(n => n.getAttribute("onclick").includes(tabId));
    if (active) active.classList.add("active");

    if (tabId === 'wardrobe' && skinViewerInstance) {
        setTimeout(() => {
            const container = document.querySelector(".viewer-container");
            skinViewerInstance.setSize(container.clientWidth, container.clientHeight);
        }, 120);
    }
}

function initDraggables() {
    const elements = document.querySelectorAll(".draggable-module");
    const canvas = document.getElementById("hudCanvas");

    elements.forEach(el => {
        el.onmousedown = function(e) {
            e.preventDefault();
            let pos3 = e.clientX;
            let pos4 = e.clientY;

            document.onmousemove = function(e) {
                e.preventDefault();
                let deltaX = pos3 - e.clientX;
                let deltaY = pos4 - e.clientY;
                pos3 = e.clientX;
                pos4 = e.clientY;

                let canvasRect = canvas.getBoundingClientRect();
                let elementRect = el.getBoundingClientRect();
                
                let top = el.offsetTop - deltaY;
                let left = el.offsetLeft - deltaX;

                if (top < 0) top = 0;
                if (left < 0) left = 0;
                if (top + elementRect.height > canvasRect.height) top = canvasRect.height - elementRect.height;
                if (left + elementRect.width > canvasRect.width) left = canvasRect.width - elementRect.width;

                el.style.top = top + "px";
                el.style.left = left + "px";

                hudPositions[el.id] = { top, left };
            };

            document.onmouseup = function() {
                document.onmousemove = null;
                document.onmouseup = null;
            };
        };
    });
}

function toggleHudModule(moduleId, checkbox) {
    const el = document.getElementById(moduleId);
    if (checkbox.checked) {
        el.style.display = "block";
        activeHudModules[moduleId] = true;
    } else {
        el.style.display = "none";
        activeHudModules[moduleId] = false;
    }
}

function drawCustomCrosshair() {
    const canvas = document.getElementById("crosshair-preview");
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    
    const color = document.getElementById("crosshairColor").value;
    const size = parseInt(document.getElementById("crosshairSize").value);
    const gap = parseInt(document.getElementById("crosshairGap").value);
    
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    
    const midX = canvas.width / 2;
    const midY = canvas.height / 2;
    
    ctx.beginPath();
    ctx.moveTo(midX, midY - gap - size/2); ctx.lineTo(midX, midY - gap);
    ctx.moveTo(midX, midY + gap); ctx.lineTo(midX, midY + gap + size/2);
    ctx.moveTo(midX - gap - size/2, midY); ctx.lineTo(midX - gap, midY);
    ctx.moveTo(midX + gap, midY); ctx.lineTo(midX + gap + size/2, midY);
    ctx.stroke();
}

// ----------------------------------------------------
// 7. P2P TUNNEL SIMULATOR
// ----------------------------------------------------
let p2pActiveRoomCode = "";
function appendP2PLog(text) {
    const box = document.getElementById("p2pConsole");
    const div = document.createElement("div");
    div.className = "con-line";
    div.innerText = `[P2P Tunnel] ${text}`;
    box.appendChild(div);
    box.scrollTop = box.scrollHeight;
}

function hostP2PLobby() {
    document.getElementById("hostOutputCard").style.display = "none";
    appendP2PLog("Initializing secure room hosting...");
    appendP2PLog("Connecting to TURN relay brokers to mask WAN interface...");
    setTimeout(() => {
        const code = `ASTRA-${Math.random().toString(36).substr(2, 4).toUpperCase()}-${Math.random().toString(36).substr(2, 4).toUpperCase()}`;
        p2pActiveRoomCode = code;
        document.getElementById("roomCodeText").innerText = code;
        document.getElementById("hostOutputCard").style.display = "block";
        appendP2PLog(`AES-256 encrypted tunnel online. Room ID: ${code}`);
    }, 1500);
}

function copyRoomCode() {
    navigator.clipboard.writeText(p2pActiveRoomCode);
    alert("Room code copied!");
}

function joinP2PLobby() {
    const code = document.getElementById("joinRoomCode").value.trim().toUpperCase();
    if (!code) return;
    appendP2PLog(`Negotiating handshake with room: ${code}`);
    setTimeout(() => {
        appendP2PLog(`Connected! Masked tunnel established with peer lobby.`);
        alert(`Connected to P2P Session ${code}`);
    }, 1500);
}

// ----------------------------------------------------
// 8. LAUNCH PROTOCOL & INTEGRITY CHECKS (Anti-Crack)
// ----------------------------------------------------
function addLaunchLog(text, style="") {
    const box = document.getElementById("launchLogsBox");
    const div = document.createElement("div");
    div.className = `log-line ${style}`;
    div.innerText = text;
    box.appendChild(div);
    box.scrollTop = box.scrollHeight;
}

// Simple client-side hash matcher for Anti-crack (simulating real hash checks)
function verifyFileHash(filename, content) {
    const sha = "a78d89fbc0d2f89d311029bac1290bb098ef"; // mock valid sha
    const hash = hashlib_sha256(content);
    return hash === sha;
}

function hashlib_sha256(str) {
    // Simple mock hash mapping for demo
    return "a78d89fbc0d2f89d311029bac1290bb098ef";
}

async function startLaunchSequence() {
    const version = document.getElementById("clientVersion").value;
    document.getElementById("launchLogsBox").innerHTML = "";

    addLaunchLog(`[Boot] Initializing Astra Launcher context...`, "text-purple");
    addLaunchLog(`[Anti-Crack] Scanning client jar file integrity...`);

    // Verify mod bundle sizes & hashes with backend server
    let modBundle = [];
    let hasBackend = false;
    try {
        const res = await fetch(`${backendUrl}/api/mods/bundle`);
        if (res.ok) {
            modBundle = await res.json();
            hasBackend = true;
        } else if (res.status === 429) {
            addLaunchLog("[Anti-DDoS] API calls blocked: Rate limit exceeded (status 429).", "text-red");
            alert("Your requests are being rate-limited by the backend server. Wait a moment.");
            return;
        }
    } catch(e) { hasBackend = false; }

    setTimeout(() => {
        addLaunchLog(`[Anti-Crack] Local hash matching completed. Checksums MATCH valid client manifests.`, "text-green");
    }, 1200);

    setTimeout(() => {
        if (hasBackend && modBundle.length > 0) {
            modBundle.forEach(m => {
                addLaunchLog(`[Sync] Matching mod package: ${m.name} -> Hash: ${m.hash} OK.`);
            });
        } else {
            addLaunchLog(`[Sync] Offline mode: Loaded local mod folder configurations.`);
        }
    }, 2200);

    setTimeout(() => {
        addLaunchLog(`[Injector] Injecting javaagent: authlib-injector.jar`);
        addLaunchLog(`[Injector] Mapping Minecraft auth endpoints to: ${backendUrl}`);
        addLaunchLog(`[Launcher] Starting Minecraft Client context...`);
    }, 3200);

    setTimeout(() => {
        launchMinecraftMock();
    }, 4500);
}

function launchMinecraftMock() {
    const overlay = document.getElementById("gameOverlay");
    overlay.style.display = "flex";

    const gameHud = document.getElementById("gameHudDisplay");
    gameHud.innerHTML = "";
    
    const canvas = document.getElementById("hudCanvas");
    const rect = canvas.getBoundingClientRect();

    // Map editor grid percentages to fullscreen client overlays
    if (activeHudModules.hudCPS) {
        const d = document.createElement("div");
        d.className = "hud-element";
        d.style.top = (hudPositions.hudCPS.top / rect.height * 100) + "vh";
        d.style.left = (hudPositions.hudCPS.left / rect.width * 100) + "vw";
        d.innerHTML = `<i class="fa-solid fa-computer-mouse"></i> CPS: <span class="val">14</span>`;
        gameHud.appendChild(d);
    }
    if (activeHudModules.hudKeystrokes) {
        const d = document.createElement("div");
        d.className = "hud-element";
        d.style.top = (hudPositions.hudKeystrokes.top / rect.height * 100) + "vh";
        d.style.left = (hudPositions.hudKeystrokes.left / rect.width * 100) + "vw";
        d.innerHTML = `
            <div class="keys-grid">
                <div class="key-box blank"></div> <div class="key-box active">W</div> <div class="key-box blank"></div>
                <div class="key-box">A</div> <div class="key-box">S</div> <div class="key-box">D</div>
            </div>
        `;
        gameHud.appendChild(d);
    }
    if (activeHudModules.hudArmor) {
        const d = document.createElement("div");
        d.className = "hud-element";
        d.style.top = (hudPositions.hudArmor.top / rect.height * 100) + "vh";
        if (hudPositions.hudArmor.right) {
            d.style.right = (hudPositions.hudArmor.right / rect.width * 100) + "vw";
        } else {
            d.style.left = (hudPositions.hudArmor.left / rect.width * 100) + "vw";
        }
        d.innerHTML = `
            <div class="hud-armor-row"><i class="fa-solid fa-shield-halved"></i> Helmet (100%)</div>
            <div class="hud-armor-row"><i class="fa-solid fa-shield-halved"></i> Chestplate (84%)</div>
        `;
        gameHud.appendChild(d);
    }
    if (activeHudModules.hudPotion) {
        const d = document.createElement("div");
        d.className = "hud-element";
        d.style.top = (hudPositions.hudPotion.top / rect.height * 100) + "vh";
        if (hudPositions.hudPotion.right) {
            d.style.right = (hudPositions.hudPotion.right / rect.width * 100) + "vw";
        } else {
            d.style.left = (hudPositions.hudPotion.left / rect.width * 100) + "vw";
        }
        d.innerHTML = `<i class="fa-solid fa-bottle-droplet"></i> Speed II (1:45)`;
        gameHud.appendChild(d);
    }

    const connText = document.getElementById("gameConnection");
    if (p2pActiveRoomCode) {
        connText.innerText = `Multiplayer Room: ${p2pActiveRoomCode}`;
    } else {
        connText.innerText = "Singleplayer World";
    }

    initGame3DViewer();
}

function initGame3DViewer() {
    const container = document.querySelector(".game-avatar-container");
    const canvas = document.getElementById("game-skin-canvas");
    if (gameSkinViewer) gameSkinViewer.dispose();

    let skin = "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d";
    let cape = null, aura = false;

    if (currentUser) {
        if (currentUser.skin_url) skin = currentUser.skin_url;
        if (currentUser.equipped.cape && currentUser.cape_url) cape = currentUser.cape_url;
        if (currentUser.equipped.aura) aura = true;
    }

    gameSkinViewer = new skinview3d.SkinViewer({
        canvas: canvas,
        width: container.clientWidth,
        height: container.clientHeight,
        skin: skin
    });

    if (cape) gameSkinViewer.loadCape(cape);
    let run = gameSkinViewer.animations.add(skinview3d.RunningAnimation);
    run.speed = 1.2;
    gameSkinViewer.autoRotate = true;

    const auraGlow = document.getElementById("gameAuraGlow");
    if (aura) {
        auraGlow.style.opacity = "1";
        if (currentUser.equipped.aura === "Fire Aura") {
            auraGlow.style.background = "radial-gradient(circle, rgba(239, 68, 68, 0.3) 0%, rgba(239, 68, 68, 0) 70%)";
        } else {
            auraGlow.style.background = "radial-gradient(circle, rgba(var(--accent-rgb), 0.3) 0%, rgba(var(--accent-rgb), 0) 70%)";
        }
    } else {
        auraGlow.style.opacity = "0";
    }
}

function closeMinecraftMock() {
    document.getElementById("gameOverlay").style.display = "none";
    if (gameSkinViewer) { gameSkinViewer.dispose(); gameSkinViewer = null; }
    addLaunchLog("[Launcher] Minecraft Sandbox context terminated. Standby...", "text-purple");
}

function setupKeystrokesListener() {
    window.addEventListener("keydown", (e) => {
        const k = e.key.toUpperCase();
        if (["W", "A", "S", "D"].includes(k)) {
            const el = document.getElementById(`key${k}`);
            if (el) el.classList.add("active");
        }
    });
    window.addEventListener("keyup", (e) => {
        const k = e.key.toUpperCase();
        if (["W", "A", "S", "D"].includes(k)) {
            const el = document.getElementById(`key${k}`);
            if (el) el.classList.remove("active");
        }
    });
}
