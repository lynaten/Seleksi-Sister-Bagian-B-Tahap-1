#!/usr/bin/env python3
import os, subprocess, sys, urllib.request

IFACE = "enp0s3"  # ganti kalau nama interfacenya bed
DNS_VM1 = "192.168.10.2"
TEST_URL = "http://www.deeznudd.local:8080"  # proxy di VM4

def run(cmd):
    print(f"$ {' '.join(cmd)}")
    subprocess.run(cmd, check=False)

def set_dns():
    print(f"-> set DNS ke {DNS_VM1}")
    with open("/etc/resolv.conf","w") as f:
        f.write(f"nameserver {DNS_VM1}\n")

def use_dhcp():
    set_dns()
    run(["dhclient", "-r", IFACE])
    run(["dhclient", IFACE])
    show_ip()

def use_manual():
    ip = input("Masukkan IP manual (misal 192.168.10.50/24): ").strip()
    run(["ip", "addr", "flush", "dev", IFACE])
    run(["ip", "addr", "add", ip, "dev", IFACE])
    run(["ip", "link", "set", IFACE, "up"])
    set_dns()
    show_ip()

def show_ip():
    run(["ip", "-4", "addr", "show", IFACE])

def http_get(url):
    print(f"GET {url}")
    try:
        with urllib.request.urlopen(url, timeout=5) as r:
            body = r.read(2000).decode("utf-8", errors="ignore")
            print("Status:", r.status)
            print("Body preview:\n", body)
    except Exception as e:
        print("Request failed:", e)

def main():
    print("=== Client CLI ===")
    print("1) DHCP")
    print("2) Manual IP")
    choice = input("Pilih (1/2): ").strip()
    if choice == "1":
        use_dhcp()
    else:
        use_manual()

    http_get(TEST_URL)

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Jalankan dengan sudo/root (butuh ubah IP & DHCP).")
        sys.exit(1)
    main()
