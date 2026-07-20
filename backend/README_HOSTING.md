# 🌐 Astra Backend Free Hosting Guide

Since you want to host your backend in the cloud for free (and not run it locally on your computer), here are the easiest and most popular ways to deploy it.

---

## Option 1: Render.com (Recommended - 100% Free)

Render allows you to host web servers for free using their Web Services tier.

### Step-by-Step Deployment:
1. **Push to GitHub**:
   - Create a GitHub repository for your Astra Client project.
   - Commit and push all files (including the `backend` folder containing `run_backend.py` and `Dockerfile`).
2. **Sign up on Render**:
   - Go to [render.com](https://render.com) and create a free account (sign in with GitHub).
3. **Create a New Web Service**:
   - Click **New +** and select **Web Service**.
   - Connect your GitHub repository.
4. **Configure Service Details**:
   - **Name**: `astra-backend` (your URL will be `https://astra-backend.onrender.com`).
   - **Root Directory**: `backend` (this tells Render to look inside your backend folder).
   - **Language**: `Docker` (Render will automatically read the `Dockerfile` we created, compile, and run it).
   - **Instance Type**: Select **Free**.
5. **Deploy**:
   - Click **Deploy Web Service**.
   - Once it compiles, your server is live at `https://your-app-name.onrender.com`!

---

## Option 2: Koyeb.com (Fast & Free)

Koyeb offers a free tier for deploying web apps and Docker containers.

### Step-by-Step Deployment:
1. Connect your GitHub repository.
2. Select the repository and set the directory to `/backend`.
3. Set the builder type to **Docker** (it will auto-detect the `Dockerfile`).
4. Select the **Nano** free instance.
5. Click **Deploy**. Your app will be live at `https://<your-app>.koyeb.app`.

---

## Option 3: Vercel / Netlify Serverless (Alternative)

If you prefer serverless deployment (no running server, only APIs responding to requests), you can deploy the web panel directly to Netlify or Vercel and write the serverless backend using Netlify functions. However, standard Java agents like `authlib-injector` require a persistent HTTP endpoint rather than serverless functions because of how they verify sessions, making **Render** or **Koyeb** the ideal hosting platforms for the actual Minecraft auth backend.

---

## 🔗 Custom Domain & HTTPS

Both Render and Koyeb automatically provide a **free SSL/HTTPS certificate**.
To bind your own domain (e.g. `api.astraclient.com`):
1. In the hosting dashboard (Render/Koyeb), go to **Settings** -> **Domains** -> **Add Custom Domain**.
2. Type your domain/subdomain.
3. In your domain registrar (e.g., Namecheap, Cloudflare, GoDaddy), add a **CNAME** record pointing to your Render URL (e.g., `astra-backend.onrender.com`).
