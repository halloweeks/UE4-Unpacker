# PUBG MOBILE Unpacker

Custom `.pak` file unpacker for **PUBG MOBILE** (Unreal Engine 4).  
This tool supports decryption and extraction of `.pak` files from specific older versions of the game.

> ⚠️ As of PUBG MOBILE **version 1.1.0 and later**, unpacking is **no longer supported** due to a new, unknown **custom encryption algorithm** replacing the older schemes.

---

## ✅ Supported Versions

- PUBG MOBILE versions **≥ 0.14.0 and ≤ 1.0.0**
- Tested on Android `.pak` files

---

## 🔐 Encryption Details

- Versions **0.14.0 to 1.0.0** use a **simple XOR cipher (key: `0x79`)** to obfuscate:
  - The **index table**
  - The **data blocks**
- Additionally, these `.pak` files use a **reordered structure** for header fields that differs from standard Unreal Engine 4 layout
- This tool automatically handles both the **XOR decryption** and **custom header parsing**
- **After version 1.0.0**, PUBG MOBILE switched to a **custom encryption algorithm** (not AES), which is currently **not supported**

---

## 🔧 How to Use

1. **Navigate to this folder**

   ```bash
   cd PUBG-MOBILE
   ```

2. **Build the unpacker**

   ```bash
   cc pubg_mobile_unpack.c -o pubg_mobile_unpack -lz -lzstd -O2
   ```

3. **Run it on a `.pak` file**

   ```bash
   ./pubg_mobile_unpack path/to/file.pak
   ```

✅ No decryption key needed — the logic is fully embedded.

---

## 📁 Output

The tool extracts readable files into the current directory, preserving original Unreal Engine mount paths when available.

---

## ❌ Unsupported After Version 1.0.0

Starting with **version 1.1.0**, PUBG MOBILE began using a **custom encryption algorithm** that is not AES and is currently **unreverse-engineered**.

> 🛠️ **Support may be added** if someone helps reverse the new encryption or provides detailed technical insight. Open an issue if you can contribute!

---

## 📄 License

MIT License
