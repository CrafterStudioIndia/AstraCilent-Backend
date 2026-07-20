/* ==========================================================================
   ASTRA PORTAL JAVASCRIPT LOGIC
   ========================================================================== */

// Global State
let backendUrl = localStorage.getItem("astra_backend_url") || "http://localhost:8080";
let currentUser = null; // Holds the logged in user profile json object
let skinViewerInstance = null;
let viewerWalkAnimation = null;
let viewerRunAnimation = null;

// Configure Default Backend URL inside input fields on load
document.addEventListener("DOMContentLoaded", () => {
    initStars();
    const bInput = document.getElementById("backendUrl");
    if (bInput) bInput.value = backendUrl;

    // Check if user is already logged in (simulated session)
    const storedUser = localStorage.getItem("astra_user_session");
    if (storedUser) {
        loadUserProfile(storedUser);
    }
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
    const starCount = 120;

    for (let i = 0; i < starCount; i++) {
        stars.push({
            x: Math.random() * width,
            y: Math.random() * height,
            size: Math.random() * 1.8 + 0.2,
            speed: Math.random() * 0.05 + 0.01,
            alpha: Math.random()
        });
    }

    function animate() {
        ctx.clearRect(0, 0, width, height);
        ctx.fillStyle = "rgba(255, 255, 255, 0.8)";
        
        stars.forEach(star => {
            ctx.beginPath();
            ctx.arc(star.x, star.y, star.size, 0, Math.PI * 2);
            ctx.fillStyle = `rgba(255, 255, 255, ${star.alpha})`;
            ctx.fill();
            
            // Move star leftward slowly
            star.x -= star.speed * width * 0.1;
            if (star.x < 0) {
                star.x = width;
                star.y = Math.random() * height;
            }
            
            // Twinkle
            star.alpha += (Math.random() - 0.5) * 0.08;
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
    localStorage.setItem("astra_portal_theme", themeName);
    toggleThemeMenu();
}

// Load Stored Theme
const storedTheme = localStorage.getItem("astra_portal_theme");
if (storedTheme) {
    document.body.className = `theme-${storedTheme}`;
}

// ----------------------------------------------------
// 3. AUTH FORM ROUTING
// ----------------------------------------------------
function switchAuthTab(tab) {
    const tabs = document.querySelectorAll(".tab-btn");
    const forms = document.querySelectorAll(".auth-form");
    
    tabs.forEach(btn => btn.classList.remove("active"));
    forms.forEach(form => form.classList.remove("active"));
    
    if (tab === "register") {
        tabs[0].classList.add("active");
        document.getElementById("registerForm").classList.add("active");
    } else {
        tabs[1].classList.add("active");
        document.getElementById("loginForm").classList.add("active");
    }
}

function switchPanel(panelId) {
    const panels = document.querySelectorAll(".panel");
    const navItems = document.querySelectorAll(".nav-item");
    
    panels.forEach(p => p.classList.remove("active"));
    navItems.forEach(n => n.classList.remove("active"));
    
    document.getElementById(`${panelId}Panel`).classList.add("active");
    
    // Add active class to corresponding navigation button
    const activeBtn = Array.from(navItems).find(btn => btn.getAttribute("onclick").includes(panelId));
    if (activeBtn) activeBtn.classList.add("active");

    // Recalculate 3D canvas size when rendering
    if (panelId === 'profile' && skinViewerInstance) {
        setTimeout(() => {
            const container = document.querySelector(".viewer-container");
            skinViewerInstance.setSize(container.clientWidth, container.clientHeight);
        }, 100);
    }
}

// ----------------------------------------------------
// 4. API CALL HANDLERS
// ----------------------------------------------------

// Register User
async function handleRegister(e) {
    e.preventDefault();
    const username = document.getElementById("regUsername").value.trim();
    const email = document.getElementById("regEmail").value.trim();
    const password = document.getElementById("regPassword").value;
    const status = document.getElementById("authStatus");
    
    status.className = "status-msg";
    status.innerText = "Registering user...";

    try {
        const res = await fetch(`${backendUrl}/api/register`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, email, password })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            status.className = "status-msg success";
            status.innerText = "Registration complete! You can now log in.";
            switchAuthTab("login");
            document.getElementById("loginIdentifier").value = username;
        } else {
            status.className = "status-msg error";
            status.innerText = data.error || "Failed to register user.";
        }
    } catch (err) {
        status.className = "status-msg error";
        status.innerText = "Error connecting to backend server. Make sure your C++/Python API is running.";
    }
}

// Login User
async function handleLogin(e) {
    e.preventDefault();
    const identifier = document.getElementById("loginIdentifier").value.trim();
    const password = document.getElementById("loginPassword").value;
    const status = document.getElementById("authStatus");
    
    status.className = "status-msg";
    status.innerText = "Logging in...";

    try {
        // Authenticate via Yggdrasil API compatible login
        const res = await fetch(`${backendUrl}/api/yggdrasil/authserver/authenticate`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username: identifier, password: password })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            status.className = "status-msg success";
            status.innerText = "Login successful!";
            
            // Query full profile by UUID
            const profileRes = await fetch(`${backendUrl}/api/profile/${data.selectedProfile.id}`);
            const profileData = await profileRes.json();
            
            localStorage.setItem("astra_user_session", profileData.uuid);
            loadUserProfile(profileData.uuid);
        } else {
            status.className = "status-msg error";
            status.innerText = data.errorMessage || "Invalid username or password.";
        }
    } catch (err) {
        status.className = "status-msg error";
        status.innerText = "Error: Backend unreachable. Validate the API settings URL.";
    }
}

// Load profile data and switch layouts
async function loadUserProfile(uuid) {
    try {
        const res = await fetch(`${backendUrl}/api/profile/${uuid}`);
        if (!res.ok) throw new Error();
        
        currentUser = await res.json();
        
        // Hide Auth Card, Show Dashboard
        document.getElementById("authCard").style.display = "none";
        document.getElementById("dashboardPanel").style.display = "grid";
        
        // Update header & sidebar
        document.getElementById("summaryUsername").innerText = currentUser.username;
        document.getElementById("summaryPoints").innerHTML = `<i class="fa-solid fa-coins"></i> ${currentUser.points.toLocaleString()} PTS`;
        
        // Populate inputs
        document.getElementById("skinUrl").value = currentUser.skin_url;
        document.getElementById("capeUrl").value = currentUser.cape_url;

        // Render Badges
        renderBadgesList();
        
        // Render 3D Model
        init3DViewer();
        
        // Render shop lists
        initShopLists();

        switchPanel("profile");
    } catch (err) {
        console.error("Failed to load user profile", err);
        handleLogout();
    }
}

// Log out user
function handleLogout() {
    localStorage.removeItem("astra_user_session");
    currentUser = null;
    if (skinViewerInstance) {
        skinViewerInstance.dispose();
        skinViewerInstance = null;
    }
    document.getElementById("dashboardPanel").style.display = "none";
    document.getElementById("authCard").style.display = "block";
    document.getElementById("authStatus").innerText = "";
}

// Render User's Badges list
function renderBadgesList() {
    const list = document.getElementById("myBadges");
    list.innerHTML = "";
    
    if (currentUser.badges && currentUser.badges.length > 0) {
        currentUser.badges.forEach(b => {
            const badgeTag = document.createElement("div");
            badgeTag.className = `badge-tag ${b.toLowerCase()}`;
            
            // Add icon depending on badge
            let iconHtml = '<i class="fa-solid fa-star"></i>';
            if (b === "Founder") iconHtml = '<i class="fa-solid fa-star"></i>';
            else if (b === "Beta Tester") iconHtml = '<i class="fa-solid fa-flask"></i>';
            else if (b === "Staff") iconHtml = '<i class="fa-solid fa-crown"></i>';
            else if (b === "Supporter") iconHtml = '<i class="fa-solid fa-gem"></i>';
            else if (b === "Creator") iconHtml = '<i class="fa-solid fa-palette"></i>';
            
            badgeTag.innerHTML = `${iconHtml} ${b}`;
            list.appendChild(badgeTag);
        });
    } else {
        list.innerHTML = '<span style="color:#666; font-size:12px;">No active badges</span>';
    }
}

// Save custom Skin/Cape urls
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
            alert("Skins & Capes applied successfully! You will see them on all Astra networks.");
            currentUser.skin_url = skin_url;
            currentUser.cape_url = cape_url;
            
            // Reload 3D textures
            if (skin_url) skinViewerInstance.loadSkin(skin_url);
            if (cape_url && currentUser.equipped.cape) {
                skinViewerInstance.loadCape(cape_url);
            } else if (!currentUser.equipped.cape) {
                skinViewerInstance.loadCape(null);
            }
        } else {
            alert("Failed to update textures on backend.");
        }
    } catch (err) {
        alert("API connection failed while updating textures.");
    }
}

// Save Backend URL Configurations
function saveBackendSettings() {
    const inputUrl = document.getElementById("backendUrl").value.trim();
    if (inputUrl) {
        backendUrl = inputUrl;
        localStorage.setItem("astra_backend_url", backendUrl);
        const status = document.getElementById("settingsStatus");
        status.innerText = "Settings Saved!";
        status.style.color = "#4caf50";
        setTimeout(() => status.innerText = "", 2000);
    }
}

// ----------------------------------------------------
// 5. INTERACTIVE 3D PLAYER VIEWER
// ----------------------------------------------------
function init3DViewer() {
    const container = document.querySelector(".viewer-container");
    const canvas = document.getElementById("skin-canvas");
    
    // Clear old skinviewer if it exists
    if (skinViewerInstance) {
        skinViewerInstance.dispose();
    }
    
    const skinUrl = currentUser.skin_url || "https://textures.minecraft.net/texture/3b6184ef4e4b5e28ef997b1a20a442845c43d8396ecf7b9f5f0b8af4fcf23d"; // Default Steve
    
    skinViewerInstance = new skinview3d.SkinViewer({
        canvas: canvas,
        width: container.clientWidth,
        height: container.clientHeight,
        skin: skinUrl
    });
    
    // Apply Cape if equipped
    if (currentUser.equipped.cape && currentUser.cape_url) {
        skinViewerInstance.loadCape(currentUser.cape_url);
    }
    
    // Apply animations
    viewerWalkAnimation = skinViewerInstance.animations.add(skinview3d.WalkingAnimation);
    viewerWalkAnimation.speed = 0.8;
    
    skinViewerInstance.autoRotate = true;
    skinViewerInstance.autoRotateSpeed = 0.5;

    // Trigger Aura Glow Visuals
    updateAuraVisuals();
}

function setViewerAnimation(animType) {
    if (!skinViewerInstance) return;
    
    // Clear existing
    if (viewerWalkAnimation) { viewerWalkAnimation.remove(); viewerWalkAnimation = null; }
    if (viewerRunAnimation) { viewerRunAnimation.remove(); viewerRunAnimation = null; }
    
    if (animType === "walk") {
        viewerWalkAnimation = skinViewerInstance.animations.add(skinview3d.WalkingAnimation);
        viewerWalkAnimation.speed = 0.8;
    } else if (animType === "run") {
        viewerRunAnimation = skinViewerInstance.animations.add(skinview3d.RunningAnimation);
        viewerRunAnimation.speed = 1.0;
    }
}

function toggleViewerRotation() {
    if (!skinViewerInstance) return;
    skinViewerInstance.autoRotate = !skinViewerInstance.autoRotate;
}

// Update the glowing cosmetic Aura surrounding the 3D model
function updateAuraVisuals() {
    const aura = document.getElementById("auraGlow");
    if (currentUser.equipped.aura) {
        aura.style.opacity = "1";
        // Customize color depending on active aura
        if (currentUser.equipped.aura === "Fire Aura") {
            aura.style.background = "radial-gradient(circle, rgba(255, 82, 82, 0.3) 0%, rgba(255, 82, 82, 0) 70%)";
        } else {
            // Neon / Astral aura
            aura.style.background = "radial-gradient(circle, rgba(187, 134, 252, 0.3) 0%, rgba(187, 134, 252, 0) 70%)";
        }
    } else {
        aura.style.opacity = "0";
    }
}

// ----------------------------------------------------
// 6. COSMETICS STORE & SHOP
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
            <div class="cosmetic-icon">
                <i class="${item.icon}"></i>
            </div>
            <h4>${item.name}</h4>
            <span class="cosmetic-tag">${item.slot}</span>
            <div class="cosmetic-price">
                <i class="fa-solid fa-coins"></i> ${item.price} PTS
            </div>
            ${isOwned 
                ? `<button class="btn btn-sm" disabled style="background:rgba(255,255,255,0.05); border-color:#555; color:#555;">Purchased</button>`
                : `<button class="btn btn-sm btn-primary" onclick="buyCosmetic('${item.name}', ${item.price})">Buy Item</button>`
            }
        `;
        shopGrid.appendChild(card);
    });
    
    // Inventory Grid
    const invGrid = document.getElementById("inventoryGrid");
    invGrid.innerHTML = "";
    
    if (currentUser.cosmetics && currentUser.cosmetics.length > 0) {
        currentUser.cosmetics.forEach(itemName => {
            const itemObj = SHOP_ITEMS.find(s => s.name === itemName);
            if (!itemObj) return; // ignore unknown
            
            const isEquipped = currentUser.equipped[itemObj.slot] === itemName;
            
            const card = document.createElement("div");
            card.className = `cosmetic-card glass-panel ${isEquipped ? 'equipped' : ''}`;
            card.innerHTML = `
                ${isEquipped ? `<span class="equipped-badge">EQUIPPED</span>` : ''}
                <div class="cosmetic-icon">
                    <i class="${itemObj.icon}"></i>
                </div>
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
        invGrid.innerHTML = `
            <div style="grid-column: 1/-1; text-align:center; padding:40px; color:#666;">
                <i class="fa-solid fa-box-open" style="font-size:32px; margin-bottom:10px;"></i>
                <p>Your wardrobe inventory is empty. Go buy cosmetics in the shop!</p>
            </div>
        `;
    }
}

function switchWardrobeTab(tab) {
    const tabs = document.querySelectorAll(".w-tab");
    const contents = document.querySelectorAll(".wardrobe-content");
    
    tabs.forEach(t => t.classList.remove("active"));
    contents.forEach(c => c.classList.remove("active"));
    
    if (tab === "shop") {
        tabs[0].classList.add("active");
        document.getElementById("shopSection").classList.add("active");
    } else {
        tabs[1].classList.add("active");
        document.getElementById("inventorySection").classList.add("active");
    }
}

// Purchase Transaction
async function buyCosmetic(itemName, cost) {
    if (currentUser.points < cost) {
        alert("Insufficient points! Complete daily tasks or play on partner servers to earn more points.");
        return;
    }

    try {
        const res = await fetch(`${backendUrl}/api/cosmetics/purchase`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, item: itemName, cost: cost })
        });
        
        const data = await res.json();
        
        if (res.ok) {
            alert(`Successfully purchased: ${itemName}!`);
            currentUser.points = data.points;
            currentUser.cosmetics = data.cosmetics;
            
            // Refresh visuals
            document.getElementById("summaryPoints").innerHTML = `<i class="fa-solid fa-coins"></i> ${currentUser.points.toLocaleString()} PTS`;
            initShopLists();
        } else {
            alert(data.error || "Failed to make purchase.");
        }
    } catch (err) {
        alert("Error connecting to server. Purchase aborted.");
    }
}

// Equip/Unequip
async function equipCosmetic(slot, itemName) {
    try {
        const res = await fetch(`${backendUrl}/api/cosmetics/equip`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, slot: slot, item: itemName })
        });
        
        if (res.ok) {
            currentUser.equipped[slot] = itemName;
            initShopLists();
            
            // Live Update 3D Viewer
            if (slot === "cape") {
                if (itemName && currentUser.cape_url) {
                    skinViewerInstance.loadCape(currentUser.cape_url);
                } else {
                    skinViewerInstance.loadCape(null);
                }
            } else if (slot === "aura") {
                updateAuraVisuals();
            }
        } else {
            alert("Failed to equip cosmetic.");
        }
    } catch (err) {
        alert("API connection failed.");
    }
}

// ----------------------------------------------------
// 7. POINTS AND REWARDS ACTIONS
// ----------------------------------------------------

// Daily Rewards Claimer
async function claimDailyReward() {
    const btn = document.getElementById("dailyRewardBtn");
    const timerText = document.getElementById("dailyTimer");
    
    // Check 24 hour cooldown locally
    const lastClaim = localStorage.getItem(`daily_claim_${currentUser.uuid}`);
    const now = Date.now();
    const cooldown = 24 * 60 * 60 * 1000; // 24 hours

    if (lastClaim && (now - lastClaim) < cooldown) {
        alert("Daily reward has a 24 hour cooldown!");
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
            document.getElementById("summaryPoints").innerHTML = `<i class="fa-solid fa-coins"></i> ${currentUser.points.toLocaleString()} PTS`;
            alert("Claimed 100 points!");
            
            // Set cooldown styling
            btn.disabled = true;
            btn.innerText = "Claimed";
        } else {
            alert("Server failed to add points.");
        }
    } catch (err) {
        alert("Failed to contact API server.");
    }
}

// Referral verification
async function submitReferral() {
    const friend = document.getElementById("refUser").value.trim();
    if (!friend) {
        alert("Please enter a valid friend's username.");
        return;
    }
    
    try {
        const res = await fetch(`${backendUrl}/api/points/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, amount: 150 })
        });
        
        const data = await res.json();
        if (res.ok) {
            alert(`Referral code applied! You rewarded your friend ${friend} and earned 150 points!`);
            currentUser.points = data.points;
            document.getElementById("summaryPoints").innerHTML = `<i class="fa-solid fa-coins"></i> ${currentUser.points.toLocaleString()} PTS`;
            document.getElementById("refUser").value = "";
        }
    } catch (err) {
        alert("Referral validation server error.");
    }
}

// Bug reporting
async function claimBugReward() {
    const id = document.getElementById("bugReportId").value.trim();
    if (!id) {
        alert("Enter your report submission code.");
        return;
    }

    try {
        const res = await fetch(`${backendUrl}/api/points/add`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ uuid: currentUser.uuid, amount: 300 })
        });
        
        const data = await res.json();
        if (res.ok) {
            alert("Bug report verified! You earned 300 bonus points.");
            currentUser.points = data.points;
            document.getElementById("summaryPoints").innerHTML = `<i class="fa-solid fa-coins"></i> ${currentUser.points.toLocaleString()} PTS`;
            document.getElementById("bugReportId").value = "";
        }
    } catch (err) {
        alert("Verification server error.");
    }
}
