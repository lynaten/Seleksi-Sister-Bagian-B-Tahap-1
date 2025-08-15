# Dokumentasi Implementasi DNS, Web Server, Client, dan Reverse Proxy

Proyek ini menggunakan **4 VM Debian tanpa desktop** pada **VirtualBox**, tanpa akses internet saat demo.

**Dokumen ini memuat tautan ke sebuah video demonstrasi yang menunjukkan hal-hal berikut:**
* Keempat VM terisolasi dan tidak memiliki koneksi ke internet.
* Akses dari VM3 ke VM2 berhasil dilakukan melalui nama domain yang telah dikonfigurasi.
* *(Opsi Bonus DHCP)* Alamat IP VM3 secara otomatis diberikan oleh DHCP server.
* *(Opsi Bonus Firewall)* Penanganan lalu lintas di VM4 diuji, termasuk pemblokiran protokol non-HTTP, IP yang dilarang, dan port selain 8080.

**Tautan menuju video tersebut dapat diakses [di sini](https://drive.google.com/file/d/1EPxVw4-hwrwWx9oK2_wgg0o3PwFJ7luE/view).**

## Struktur VM
- **VM1** : DNS + DHCP Server (`192.168.10.2/24`)
- **VM2** : HTTP Server (`192.168.10.3/24`)
- **VM3** : Client (DHCP/manual via CLI)
- **VM4** : Reverse Proxy + Firewall (`192.168.10.4/24`)

> Semua file konfigurasi ada di repo ini per folder **VM1/.. VM4/** (path relatif meniru lokasi aslinya).

---

## 📦 Paket yang Digunakan

### VM1 (DNS + DHCP Server)
| Paket               | Fungsi                                                                 |
|---------------------|------------------------------------------------------------------------|
| `isc-dhcp-server`   | Memberikan alamat IP otomatis (DHCP) untuk klien.                     |
| `bind9`             | DNS server untuk domain `deeznudd.local`.                             |
| `net-tools`         | Utilitas jaringan.                                          |

**File konfigurasi di repo (salin ke VM1):**
- `/etc/dhcp/dhcpd.conf`
- `/etc/default/isc-dhcp-server`
- `/etc/network/interfaces`
- `/etc/bind/named.conf.local`
- `/etc/bind/db.deeznudd.local`
- `/etc/hosts`

**Perintah instalasi & enable service (VM1):**
```bash
apt update
apt install -y isc-dhcp-server bind9 net-tools

# aktifkan service saat boot
systemctl enable --now bind9
systemctl enable --now isc-dhcp-server
```

**Perintah validasi & apply (VM1):**
```bash
# Validasi BIND
named-checkconf
named-checkzone deeznudd.local /etc/bind/db.deeznudd.local

# Restart & cek status
systemctl restart bind9
systemctl status  bind9 --no-pager

# Validasi DHCP
dhcpd -t -4 -cf /etc/dhcp/dhcpd.conf enp0s3

# Restart & cek status DHCP
systemctl restart isc-dhcp-server
systemctl status  isc-dhcp-server --no-pager

# Kunci file agar tidak berubah
chattr +i /etc/dhcp/dhcpd.conf /etc/bind/db.deeznudd.local /etc/bind/named.conf.local
```

**Uji cepat (VM1):**
```bash
dig @127.0.0.1 www.deeznudd.local
dig @192.168.10.2 www.deeznudd.local
```

---

### VM2 (Web Server)
| Paket     | Fungsi                          |
|-----------|----------------------------------|
| `apache2` | HTTP server (listen **8080**)   |
| `curl`    | Testing HTTP                     |
| `net-tools` | Utility networking                       |

**File konfigurasi di repo (VM2):**
- `/var/www/html/index.html`
- `/etc/apache2/ports.conf` (set **Listen 8080**)
- `/etc/network/interfaces`
- `/etc/hosts`

**Perintah instalasi & apply (VM2):**
```bash
apt update
apt install -y apache2 curl net-tools

# Ubah ports.conf sesuai repo (Listen 8080), lalu:
systemctl restart apache2
systemctl enable  apache2

# Verifikasi
ss -ltnp | grep 8080
curl http://127.0.0.1:8080
```

---

### VM3 (Client – CLI DHCP/Manual)
| Paket               | Fungsi                        |
|---------------------|-------------------------------|
| `python3`           | Menjalankan `client.py`       |
| `isc-dhcp-client`   | DHCP klien                    |
| `curl`              | Testing HTTP                  |
| `net-tools` (ops.)  | Utilitas jaringan             |

**File di repo (VM3):**
- `/root/client.py`
- `/etc/network/interfaces`
- `/etc/hosts`
- `/etc/resolv.conf` (bukti nameserver `192.168.10.2`)

**Perintah instalasi & jalankan (VM3):**
```bash
apt update
apt install -y python3 isc-dhcp-client curl net-tools

# Jalankan program CLI (harus root)
python3 /root/client.py
# -> Pilih DHCP atau Manual, program akan set IP & DNS lalu HTTP GET ke
#    http://www.deeznudd.local:8080
```

---

### VM4 (Reverse Proxy + Firewall)
| Paket                   | Fungsi                                                           |
|-------------------------|-------------------------------------------------------------------|
| `nginx`                 | Reverse proxy (listen **8080**, proxy ke VM2:8080)               |
| `iptables-persistent`   | Simpan & restore aturan firewall iptables saat boot              |
| `curl`, `net-tools`     | Testing & utilitas                                               |

**File konfigurasi di repo (VM4):**
- `/etc/nginx/sites-available/default`
- `/etc/network/interfaces`
- `/etc/hosts`
- `/etc/resolv.conf`
- `/etc/iptables/rules.v4`
- `/etc/iptables/rules.v6`

**Perintah instalasi & apply (VM4):**
```bash
apt update
apt install -y nginx iptables-persistent curl net-tools

# Apply nginx
nginx -t
systemctl reload nginx
systemctl enable  nginx

# Verifikasi reverse proxy (dari VM4)
curl http://192.168.10.3:8080
curl http://www.deeznudd.local:8080
```

**Aturan firewall – inbound hanya TCP 8080, blok range 192.168.10.120/29, outbound dibatasi 8080 saja:**
> Sudah disediakan pada `/etc/iptables/rules.v4` dan `/etc/iptables/rules.v6` (di repo).
```bash
# Terapkan file rules secara manual
iptables-restore  < /etc/iptables/rules.v4
ip6tables-restore < /etc/iptables/rules.v6

# Simpan & restore otomatis saat boot
netfilter-persistent reload
netfilter-persistent save
```

**Cek iptables (VM4):**
```bash
iptables -S
iptables -L -n -v
```

---

## 🔎 Langkah Demonstrasi

1. **Tidak ada internet**  
   Semua VM: Adapter = **Internal Network (intnet)**.  
   Uji: `ping 8.8.8.8` dari VM manapun harus **gagal**.

2. **DNS dari VM1**  
   Dari VM3/VM4:
   ```bash
   dig @192.168.10.2 www.deeznudd.local
   # Jawaban harus A 192.168.10.4 (VM4), bukan IP VM2
   ```

3. **DHCP (bonus)**  
   Jalankan `client.py` di VM3 → pilih **DHCP**.  
   Tampilkan IP yang didapat: `ip -4 addr show enp0s3`.

4. **Akses via domain**  
   Dari VM3:
   ```bash
   curl http://www.deeznudd.local:8080
   # Muncul "Hello from VM2 HTTP Server"
   ```

5. **Firewall (bonus)**
   - **Hanya HTTP/8080**:  
     `curl http://www.deeznudd.local:80` → gagal.  
     `ping 192.168.10.4` → gagal (INPUT DROP).  
     `nc -u www.deeznudd.local 8080` → gagal (UDP diblok default).
   - **Blok range IP**: set IP VM3 ke `192.168.10.130/24` lalu:  
     `curl http://www.deeznudd.local:8080` → **gagal** (diblok range).

---

## 📁 Struktur Repo
```
VM1/
  etc/bind/db.deeznudd.local
  etc/bind/named.conf.local
  etc/dhcp/dhcpd.conf
  etc/default/isc-dhcp-server
  etc/network/interfaces
  etc/hosts

VM2/
  var/www/html/index.html
  etc/apache2/ports.conf
  etc/network/interfaces
  etc/hosts

VM3/
  root/client.py
  etc/network/interfaces
  etc/hosts
  etc/resolv.conf

VM4/
  etc/nginx/sites-available/default
  etc/network/interfaces
  etc/hosts
  etc/resolv.conf
  etc/iptables/rules.v4
  etc/iptables/rules.v6
```

---

## ✍️ Catatan
- `resolv.conf` disertakan sebagai **bukti** DNS menunjuk ke VM1 (`nameserver 192.168.10.2`).  
  Pada mode DHCP, file ini bisa berubah; sumber kebenaran adalah **DHCP/`client.py`**.