# Big Integer Multiplication (Bitwise-Only)

## 📜 Description

This program multiplies two large positive integers using **only** the allowed operations:

- Bitwise operators: `<<`, `>>`, `&`, `|`, `^`, `~`, `!`
- `goto` statements
- `if` statements
- Static primitive arrays (no `malloc`)
- `printf` and `scanf` from `<stdio.h>`
- Any data types (including `__uint128_t`) and casting via pointers
- Custom functions and macros

It supports input sizes up to **10¹⁰⁰⁰⁰⁰⁰⁰** digits with an optimized **O(N log N)** approach (e.g., FFT/NTT) for maximum performance.

---

## 📂 Project Structure

```
.
├── main.c          # Core multiplication program
├── generate.c      # Random input generator for
├── Makefile        # Build and run automation
├── input.txt       # Input file (generated or user-provided)
└── output.txt      # Output file containing result
```

---

## ⚙️ Requirements

- GCC compiler
- Linux or similar environment (Makefile syntax compatible with GNU Make)
- Standard C library

---

## 🚀 Usage

### 1. Build & Run with Generated Input

```bash
make all
```

This will:

1. **Clean** previous builds & outputs
2. **Compile** `main.c` and `generate.c`
3. **Generate** random `input.txt` containing two big integers
4. **Run** the multiplication program and save result to `output.txt`

You can adjust the max generated digits by editing the generate.c file.

---

### 2. Run with Your Own Input

1. Create an `input.txt` file with two integers (each on its own line).

   ```
   123456789
   987654321
   ```

2. Run:

   ```bash
   make run
   ```

---

### 3. Individual Commands

```bash
make build     # Compile programs without running
make generate  # Only generate input.txt
make run       # Run using existing input.txt
make clean     # Remove compiled binaries and outputs
```

---

## 📝 Example

**input.txt**

```
9999999999999999999999999999
8888888888888888888888888888
```

**output.txt**

```
888888888888888888888888888711111111111111111111111111112
```

**terminal output**

```
Execution time: 0:00.05 (0.05s)
```

---

## 📌 Notes

- `generate.c` creates random **MAX_DIGITS**-digit integers for stress testing.
- The execution time will be automatically displayed when running make run or make all, since the Makefile uses the /usr/bin/time command to measure and print how long the program takes to execute.
