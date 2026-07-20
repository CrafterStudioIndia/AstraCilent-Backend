/* ==========================================================================
   ASTRA LAUNCHER APPLICATION LOGIC
   ========================================================================== */

// Global State Configurations
let backendUrl = localStorage.getItem("astra_launcher_backend_url") || "http://localhost:8080";
let currentUser = null;
let currentTheme = localStorage.getItem("astra_launcher_theme") || "purple";
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

// 3D Minecraft Viewer variables
let gameSkinViewer = null;

// Initialize on Load
document.addEventListener("DOMContentLoaded", () => {
    initStars();
    setTheme(currentTheme);
    checkBackendConnection();
    initDraggables();
    drawCustomCrosshair();

    // Fill URL input field
    const urlInput = document.getElementById("launcherBackendUrl");
    if (urlInput) urlInput.value = backendUrl;

    // Check if player has an active session
    const cachedUserUuid = localStorage.getItem("astra_launcher_user_uuid");
    if (cachedUserUuid) {
        syncProfileWithBackend(cachedUserUuid);
    }
    
    // Setup key listener for Keystrokes HUD preview (W, A, S, D visual feedback)
    setupKeystrokesListener();
});

// ----------------------------------------------------
// 1. STARFIELD ANIMATION
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
// 3. BACKEND CONNECTION & PROFILE SYNC
// ----------------------------------------------------
async function checkBackendConnection() {
    const statusText = document.getElementById("serverConnectionText");
    const indicator = document.querySelector(".status-indicator");
    
    try {
        const res = await fetch(`${backendUrl}/api/yggdrasil`);
        if (res.ok) {
            statusText.innerText = "Online (API Connected)";
            indicator.className = "status-indicator online";
        } else {
            throw new Error();
        }
    } catch (e) {
        statusText.innerText = "Standalone (Server Offline)";
        indicator.className = "status-indicator";
    }
}

function saveLauncherBackendUrl() {
    const inputUrl = document.getElementById("launcherBackendUrl").value.trim();
    if (inputUrl) {
        backendUrl = inputUrl;
        localStorage.setItem("astra_launcher_backend_url", backendUrl);
        alert("Launcher backend connection settings updated.");
        checkBackendConnection();
    }
}

async function syncProfileWithBackend(uuid) {
    try {
        const res = await fetch(`${backendUrl}/api/profile/${uuid}`);
        if (!res.ok) throw new Error();
        
        currentUser = await res.json();
        
        // Update header UI
        document.getElementById("headerUsername").innerText = currentUser.username;
        document.getElementById("headerPoints").innerText = `${currentUser.points.toLocaleString()} PTS`;
        
        // Find highest badge to show in offline header status
        let mainBadge = "User Profile";
        if (currentUser.badges.includes("Staff")) mainBadge = "👑 Staff Member";
        else if (currentUser.badges.includes("Founder")) mainBadge = "⭐ Founder Player";
        else if (currentUser.badges.includes("Beta Tester")) mainBadge = "🧪 Beta Tester";
        else if (currentUser.badges.includes("Supporter")) mainBadge = "💎 Client Supporter";
        document.getElementById("headerBadge").innerText = mainBadge;
        
        // Render avatar
        document.getElementById("headerAvatar").innerHTML = `<i class="fa-solid fa-user-astronaut" style="color:var(--accent-color);"></i>`;

        // Render Inventory List inside account tab
        renderLauncherInventory();
    } catch (err) {
        console.error("Failed to sync account profile with backend.", err);
        clearLauncherSession();
    }
}

async function handleAccountLogin(e) {
    e.preventDefault();
    const identifier = document.getElementById("launcherEmail").value.trim();
    const password = document.getElementById("launcherPassword").value;

    try {
        const res = await fetch(`${backendUrl}/api/yggdrasil/authserver/authenticate`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username: identifier, password: password })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            alert(`Account synced! Welcome, ${data.selectedProfile.name}.`);
            localStorage.setItem("astra_launcher_user_uuid", data.selectedProfile.id);
            syncProfileWithBackend(data.selectedProfile.id);
        } else {
            alert(data.errorMessage || "Login credentials rejected.");
        }
    } catch (e) {
        alert("Unable to contact backend authentication. Verify the backend service is running.");
    }
}

function clearLauncherSession() {
    localStorage.removeItem("astra_launcher_user_uuid");
    currentUser = null;
    document.getElementById("headerUsername").innerText = "Guest Player";
    document.getElementById("headerPoints").innerText = "0 PTS";
    document.getElementById("headerBadge").innerText = "Offline Mode";
    document.getElementById("headerAvatar").innerHTML = `<i class="fa-solid fa-user-astronaut"></i>`;
    document.getElementById("launcherInvList").innerHTML = `
        <div style="text-align:center; padding:30px; color:#555;">
            <i class="fa-solid fa-lock" style="font-size:32px; margin-bottom:10px;"></i>
            <p>Sign in with your Astra Account credentials to view and inspect your synchronized cosmetics.</p>
        </div>
    `;
}

function renderLauncherInventory() {
    const invList = document.getElementById("launcherInvList");
    invList.innerHTML = "";

    const slotIcons = {
        wings: "fa-solid fa-dove",
        cape: "fa-solid fa-square-full",
        hat: "fa-solid fa-crown",
        aura: "fa-solid fa-fire"
    };

    if (currentUser.cosmetics && currentUser.cosmetics.length > 0) {
        currentUser.cosmetics.forEach(item => {
            // Determine slot
            let slot = "cosmetic";
            if (item.toLowerCase().includes("wing")) slot = "wings";
            else if (item.toLowerCase().includes("cape")) slot = "cape";
            else if (item.toLowerCase().includes("crown") || item.toLowerCase().includes("hat")) slot = "hat";
            else if (item.toLowerCase().includes("aura")) slot = "aura";

            const isEquipped = currentUser.equipped[slot] === item;
            
            const card = document.createElement("div");
            card.className = "launcher-cosmetic-item";
            card.innerHTML = `
                <div class="cos-info">
                    <div class="cos-icon"><i class="${slotIcons[slot] || 'fa-solid fa-shirt'}"></i></div>
                    <div class="cos-details">
                        <h4>${item}</h4>
                        <p>${slot.toUpperCase()}</p>
                    </div>
                </div>
                <div>
                    ${isEquipped 
                        ? `<span class="badge badge-purple" style="font-size:10px;">Equipped</span>`
                        : `<span style="font-size:11px; color:#555;">Unequipped</span>`
                    }
                </div>
            `;
            invList.appendChild(card);
        });
    } else {
        invList.innerHTML = `
            <div style="text-align:center; padding:30px; color:#555;">
                <i class="fa-solid fa-shirt" style="font-size:32px; margin-bottom:10px;"></i>
                <p>Your cosmetic storage is empty. Earn points and purchase items from the web wardrobe portal!</p>
            </div>
        `;
    }
}

// ----------------------------------------------------
// 4. TAB ROUTER
// ----------------------------------------------------
function switchTab(tabId) {
    const panels = document.querySelectorAll(".panel");
    const navItems = document.querySelectorAll(".nav-item");
    
    panels.forEach(p => p.classList.remove("active"));
    navItems.forEach(n => n.classList.remove("active"));
    
    document.getElementById(`${tabId}Panel`).classList.add("active");
    
    const activeNav = Array.from(navItems).find(n => n.getAttribute("onclick").includes(tabId));
    if (activeNav) activeNav.classList.add("active");
}

// ----------------------------------------------------
// 5. DRAG-AND-DROP HUD EDITOR ENGINE
// ----------------------------------------------------
function initDraggables() {
    const elements = document.querySelectorAll(".draggable-module");
    const canvas = document.getElementById("hudCanvas");

    elements.forEach(el => {
        el.onmousedown = function(e) {
            e.preventDefault();
            
            // Get cursor start position
            let pos3 = e.clientX;
            let pos4 = e.clientY;

            // Handle movements
            document.onmousemove = function(e) {
                e.preventDefault();
                // calculate coordinate deltas
                let deltaX = pos3 - e.clientX;
                let deltaY = pos4 - e.clientY;
                pos3 = e.clientX;
                pos4 = e.clientY;

                // Set new positions relative to canvas boundaries
                let canvasRect = canvas.getBoundingClientRect();
                let elementRect = el.getBoundingClientRect();
                
                let targetTop = el.offsetTop - deltaY;
                let targetLeft = el.offsetLeft - deltaX;

                // Boundary clipping
                if (targetTop < 0) targetTop = 0;
                if (targetLeft < 0) targetLeft = 0;
                if (targetTop + elementRect.height > canvasRect.height) {
                    targetTop = canvasRect.height - elementRect.height;
                }
                if (targetLeft + elementRect.width > canvasRect.width) {
                    targetLeft = canvasRect.width - elementRect.width;
                }

                el.style.top = targetTop + "px";
                el.style.left = targetLeft + "px";

                // Save positions in pixel values
                hudPositions[el.id] = {
                    top: targetTop,
                    left: targetLeft
                };
            };

            // Stop moving when mouse is released
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

// Draw preview for custom crosshairs
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
    
    // Draw top
    ctx.beginPath();
    ctx.moveTo(midX, midY - gap - size/2);
    ctx.lineTo(midX, midY - gap);
    ctx.stroke();

    // Draw bottom
    ctx.beginPath();
    ctx.moveTo(midX, midY + gap);
    ctx.lineTo(midX, midY + gap + size/2);
    ctx.stroke();

    // Draw left
    ctx.beginPath();
    ctx.moveTo(midX - gap - size/2, midY);
    ctx.lineTo(midX - gap, midY);
    ctx.stroke();

    // Draw right
    ctx.beginPath();
    ctx.moveTo(midX + gap, midY);
    ctx.lineTo(midX + gap + size/2, midY);
    ctx.stroke();
}

// ----------------------------------------------------
// 6. P2P SIMULATOR ENGINE
// ----------------------------------------------------
let p2pActiveRoomCode = "";

function appendP2PLog(text) {
    const consoleBox = document.getElementById("p2pConsole");
    const div = document.createElement("div");
    div.className = "con-line";
    div.innerText = `[P2P Broker] ${text}`;
    consoleBox.appendChild(div);
    consoleBox.scrollTop = consoleBox.scrollHeight;
}

function hostP2PLobby() {
    const worldName = document.getElementById("worldSelect").value;
    document.getElementById("hostOutputCard").style.display = "none";
    
    appendP2PLog(`Spinning up hosting for world: "${worldName}"...`);
    appendP2PLog("Initializing STUN server queries for hole punching...");
    
    setTimeout(() => {
        appendP2PLog("STUN server returned public mapping: 104.22.45.19:50221 -> Local Port 25565");
        appendP2PLog("Requesting TURN relay fallback server registration...");
    }, 1000);

    setTimeout(() => {
        appendP2PLog("Encrypted AES-256 secure tunnel established through relay node [US-West-1]");
        appendP2PLog("Generating unique Room ID for invite handshake...");
        
        // Generate code
        const code = `ASTRA-${Math.random().toString(36).substr(2, 4).toUpperCase()}-${Math.random().toString(36).substr(2, 4).toUpperCase()}`;
        p2pActiveRoomCode = code;
        
        document.getElementById("roomCodeText").innerText = code;
        document.getElementById("hostOutputCard").style.display = "block";
        appendP2PLog(`Lobby listening. Invitation Room ID: "${code}"`);
    }, 2200);
}

function copyRoomCode() {
    if (!p2pActiveRoomCode) return;
    navigator.clipboard.writeText(p2pActiveRoomCode);
    alert("Room Invitation code copied to clipboard!");
}

function joinP2PLobby() {
    const code = document.getElementById("joinRoomCode").value.trim().toUpperCase();
    if (!code) {
        alert("Please enter a valid lobby room code.");
        return;
    }
    
    appendP2PLog(`Requesting connection to room: "${code}"...`);
    appendP2PLog("Resolving relay tunnel nodes...");
    
    setTimeout(() => {
        appendP2PLog("Direct NAT Traversal mapping failed. Routing tunnel packets through TURN relay...");
        appendP2PLog("Establishing Diffie-Hellman cryptographic handshake...");
    }, 1200);

    setTimeout(() => {
        appendP2PLog(`Handshake completed! Joined multiplayer session room "${code}" successfully.`);
        alert(`Successfully connected to P2P multiplayer session: ${code}`);
    }, 2500);
}

// ----------------------------------------------------
// 7. MINECRAFT LAUNCH FLOW SIMULATOR
// ----------------------------------------------------
let logsOffset = 0;

function addLaunchLog(text, style = "") {
    const box = document.getElementById("launchLogsBox");
    const div = document.createElement("div");
    div.className = `log-line ${style}`;
    div.innerText = text;
    box.appendChild(div);
    box.scrollTop = box.scrollHeight;
}

async function startLaunchSequence() {
    const version = document.getElementById("clientVersion").value;
    document.getElementById("launchLogsBox").innerHTML = "";
    
    addLaunchLog(`[Astra Client Boot] Launching profile ${version}...`, "text-purple");
    addLaunchLog("[Database] Verifying cloud synchronization...");

    // Check if backend is running, otherwise alert or log
    let hasBackend = false;
    let modBundle = [];
    try {
        const res = await fetch(`${backendUrl}/api/mods/bundle`);
        if (res.ok) {
            modBundle = await res.json();
            hasBackend = true;
        }
    } catch (e) {
        hasBackend = false;
    }

    setTimeout(() => {
        if (hasBackend) {
            addLaunchLog("[Database] Handshake success. Verifying active session tokens...", "text-green");
        } else {
            addLaunchLog("[Database] Service offline. Launching in Local/Offline profile Mode...", "text-red");
        }
    }, 1000);

    setTimeout(() => {
        addLaunchLog("[Assets] Syncing mods bundle configuration from server...");
        if (hasBackend && modBundle.length > 0) {
            modBundle.forEach(m => {
                addLaunchLog(`[Assets] Checked mod package: ${m.name} (${m.size}) -> Hash OK.`);
            });
        } else {
            addLaunchLog("[Assets] Loaded local mod cached directory. 4 active files matched.");
        }
    }, 2000);

    setTimeout(() => {
        addLaunchLog("[Injector] Injecting javaagent: authlib-injector.jar");
        addLaunchLog(`[Injector] Hooking authentication routes onto: ${backendUrl}`);
    }, 3200);

    setTimeout(() => {
        addLaunchLog("[HUD Engine] Compiling draggable positions configurations...");
        addLaunchLog("[Launcher] Starting Minecraft Client window context...");
    }, 4000);

    setTimeout(() => {
        // Trigger Minecraft window mock overlays
        launchMinecraftMock();
    }, 4800);
}

function launchMinecraftMock() {
    const overlay = document.getElementById("gameOverlay");
    overlay.style.display = "flex";
    
    // Populate HUD elements inside Minecraft overlay
    const gameHud = document.getElementById("gameHudDisplay");
    gameHud.innerHTML = "";
    
    // Get hud screen element size boundaries to calculate exact percentages
    const canvas = document.getElementById("hudCanvas");
    const rect = canvas.getBoundingClientRect();
    
    // Clone draggable elements to HUD canvas screen
    if (activeHudModules.hudCPS) {
        const div = document.createElement("div");
        div.className = "hud-element";
        div.style.top = (hudPositions.hudCPS.top / rect.height * 100) + "vh";
        div.style.left = (hudPositions.hudCPS.left / rect.width * 100) + "vw";
        div.innerHTML = `<i class="fa-solid fa-computer-mouse"></i> CPS: <span class="val">14</span>`;
        gameHud.appendChild(div);
    }
    
    if (activeHudModules.hudKeystrokes) {
        const div = document.createElement("div");
        div.className = "hud-element";
        div.style.top = (hudPositions.hudKeystrokes.top / rect.height * 100) + "vh";
        div.style.left = (hudPositions.hudKeystrokes.left / rect.width * 100) + "vw";
        div.innerHTML = `
            <div class="keys-grid">
                <div class="key-box blank"></div> <div class="key-box active">W</div> <div class="key-box blank"></div>
                <div class="key-box">A</div> <div class="key-box">S</div> <div class="key-box">D</div>
            </div>
        `;
        gameHud.appendChild(div);
    }

    if (activeHudModules.hudArmor) {
        const div = document.createElement("div");
        div.className = "hud-element";
        div.style.top = (hudPositions.hudArmor.top / rect.height * 100) + "vh";
        
        // Handle right aligned calculations
        if (hudPositions.hudArmor.right) {
            div.style.right = (hudPositions.hudArmor.right / rect.width * 100) + "vw";
        } else {
            div.style.left = (hudPositions.hudArmor.left / rect.width * 100) + "vw";
        }
        
        div.innerHTML = `
            <div class="hud-armor-row"><i class="fa-solid fa-shield-halved"></i> Helmet (100%)</div>
            <div class="hud-armor-row"><i class="fa-solid fa-shield-halved"></i> Chestplate (84%)</div>
        `;
        gameHud.appendChild(div);
    }

    if (activeHudModules.hudPotion) {
        const div = document.createElement("div");
        div.className = "hud-element";
        div.style.top = (hudPositions.hudPotion.top / rect.height * 100) + "vh";
        
        if (hudPositions.hudPotion.right) {
            div.style.right = (hudPositions.hudPotion.right / rect.width * 100) + "vw";
        } else {
            div.style.left = (hudPositions.hudPotion.left / rect.width * 100) + "vw";
        }
        
        div.innerHTML = `<i class="fa-solid fa-bottle-droplet"></i> Speed II (1:45)`;
        gameHud.appendChild(div);
    }

    // Set connection context
    const connText = document.getElementById("gameConnection");
    if (p2pActiveRoomCode) {
        connText.innerText = `Multiplayer Room: ${p2pActiveRoomCode}`;
    } else {
        connText.innerText = "Singleplayer World";
    }

    // Initialize 3D skin viewer in mock Minecraft game screen
    initGame3DViewer();
}

function initGame3DViewer() {
    const container = document.querySelector(".game-avatar-container");
    const canvas = document.getElementById("game-skin-canvas");

    if (gameSkinViewer) {
        gameSkinViewer.dispose();
    }

    let skin = "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d"; // Default Steve
    let cape = null;
    let aura = false;

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

    if (cape) {
        gameSkinViewer.loadCape(cape);
    }

    // Set running animation inside game mockup
    let run = gameSkinViewer.animations.add(skinview3d.RunningAnimation);
    run.speed = 1.2;

    gameSkinViewer.autoRotate = true;
    gameSkinViewer.autoRotateSpeed = 1.0;

    // Toggle Aura Glow Overlay
    const auraGlow = document.getElementById("gameAuraGlow");
    if (aura) {
        auraGlow.style.opacity = "1";
        if (currentUser.equipped.aura === "Fire Aura") {
            auraGlow.style.background = "radial-gradient(circle, rgba(255, 82, 82, 0.3) 0%, rgba(255, 82, 82, 0) 70%)";
        } else {
            auraGlow.style.background = "radial-gradient(circle, rgba(var(--accent-rgb), 0.3) 0%, rgba(var(--accent-rgb), 0) 70%)";
        }
    } else {
        auraGlow.style.opacity = "0";
    }
}

function closeMinecraftMock() {
    document.getElementById("gameOverlay").style.display = "none";
    if (gameSkinViewer) {
        gameSkinViewer.dispose();
        gameSkinViewer = null;
    }
    addLaunchLog("[Launcher] Minecraft Client closed. Standing by...", "text-purple");
}

// ----------------------------------------------------
// 8. OTHER UTILITIES
// ----------------------------------------------------
function setupKeystrokesListener() {
    window.addEventListener("keydown", (e) => {
        const key = e.key.toUpperCase();
        if (["W", "A", "S", "D"].includes(key)) {
            const keyEl = document.getElementById(`key${key}`);
            if (keyEl) keyEl.classList.add("active");
        }
    });

    window.addEventListener("keyup", (e) => {
        const key = e.key.toUpperCase();
        if (["W", "A", "S", "D"].includes(key)) {
            const keyEl = document.getElementById(`key${key}`);
            if (keyEl) keyEl.classList.remove("active");
        }
    });
}
