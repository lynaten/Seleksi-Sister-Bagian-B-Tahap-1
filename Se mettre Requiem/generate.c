#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_DIGITS 1000000

int main()
{
    // Inisialisasi generator angka acak
    srand(time(NULL));

    FILE *f = fopen("input.txt", "w");

    if (!f)
    {
        printf("Gagal membuka file.\n");
        return 1;
    }

    // Tulis angka pertama dengan digit acak
    // Digit pertama tidak boleh '0' (1-9)
    fputc((rand() % 9) + '1', f);
    for (int i = 1; i < MAX_DIGITS; ++i)
    {
        // Digit selanjutnya bisa '0'-'9'
        fputc((rand() % 10) + '0', f);
    }
    fputc('\n', f);

    // Tulis angka kedua dengan digit acak
    // Digit pertama tidak boleh '0' (1-9)
    fputc((rand() % 9) + '1', f);
    for (int i = 1; i < MAX_DIGITS; ++i)
    {
        // Digit selanjutnya bisa '0'-'9'
        fputc((rand() % 10) + '0', f);
    }
    fputc('\n', f);

    fclose(f);

    printf("Berhasil membuat file input.txt dengan 2 angka acak berjumlah %d digit.\n", MAX_DIGITS);
    return 0;
}