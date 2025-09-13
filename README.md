# SSRD

**SSRD** (**S**ecure **S**ocket **R**emote **D**esktop) is a lightweight remote desktop solution with end-to-end encrypted communication over sockets.  
This project is still in development but already functional enough to be tested.

---

## 🚀 Features

- Secure socket-based communication (RSA key exchange).  
- Client/Server architecture.  
- Easy setup with CMake.  
- Minimal dependencies.  

---

## 🛠️ Build Instructions

Clone the repository and build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The compiled binaries will be available in the `build/` folder:  

- `ssrd-server` – run this on the target machine (the one being shared).  
- `ssrd-client` – run this on the local machine (the one viewing).  

---

## ▶️ Usage

### 1. Run the Server

On the target machine:  

```bash
./ssrd-server
```

### 2. Setup Keys

On the **client machine**, generate RSA keys:  

```bash
openssl genpkey -algorithm RSA -out private.pem
openssl rsa -in private.pem -pubout -out public.pem
```

Copy the generated **public.pem** into the server machine under:  

```
~/.ssrd/public.pem
```

> Make sure the `~/.ssrd` folder exists on the server.  

### 3. Connect from the Client

From your client machine:  

```bash
./ssrd-client -h <server-ip-address>
```

---

## 📂 Project Structure

```
ssrd/
├── CMakeLists.txt
├── src/
│   ├── client/   # Client-side code
│   ├── server/   # Server-side code
│   └── common/   # Shared utilities
└── README.md
```

---

## ⚠️ Notes

- This project is **not production-ready** yet.  
- Error handling and additional security layers are still being added/improved.  
- Contributions and feedback are welcome.  

---

## 📜 License

[MIT License](LICENSE) – free to use, modify, and distribute.  