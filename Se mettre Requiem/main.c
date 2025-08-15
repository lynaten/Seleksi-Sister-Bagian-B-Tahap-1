#include <stdio.h>

typedef unsigned int W;        // 32-bit
typedef unsigned long long WW; // 64-bit

#define MOD ((W)998244353)
#define ROOT ((W)15311432)    /* 3^119 mod MOD */
#define ROOT_1 ((W)469870224) /* inverse(ROOT) mod MOD */
#define ROOT_PW ((W)1u << 23) /* max power-of-two length: 8,388,608 */

char str_a[ROOT_PW];
char str_b[ROOT_PW];

W a_digits[ROOT_PW] = {0};
W b_digits[ROOT_PW] = {0};

/* ---------------- 32-bit bitwise helpers ---------------- */

int is_equal(W a, W b) { return !(a ^ b); }

W add(W a, W b)
{
    W carry;
loop_add:
    carry = a & b;
    a = a ^ b;
    b = carry << 1;
    if (is_equal(b, 0))
        goto done_add;
    goto loop_add;
done_add:
    return a;
}

W sub(W a, W b) { return add(a, add((W)~b, (W)1)); }

int is_less_than(W a, W b)
{
    if (is_equal(a, b))
        return 0;
    W diff = sub(a, b);
    W x_xor_y = a ^ b;
    W diff_xor_y = diff ^ b;
    W or_part = x_xor_y | diff_xor_y;
    W sign = (a ^ or_part) >> 31;
    return sign & 1;
}
int is_greater_than(W a, W b) { return ((~is_equal(a, b)) & (~is_less_than(a, b))) & 1; }

W subtract_mod(W a, W b)
{
    if (is_less_than(a, b))
        return sub(add(a, MOD), b);
    return sub(a, b);
}

/* multiply modulo MOD using only shifts/adds (no *) */
W multiply_mod(W a, W b)
{
    W res = 0, ta = a, tb = b;
loop_mult:
    if (tb & 1)
    {
        res = add(res, ta);
        if (!is_less_than(res, MOD))
            res = subtract_mod(res, MOD);
    }
    tb >>= 1;
    if (is_equal(tb, 0))
        goto end_mult;
    ta = add(ta, ta);
    if (!is_less_than(ta, MOD))
        ta = subtract_mod(ta, MOD);
    goto loop_mult;
end_mult:
    if (is_equal(res, MOD))
        return 0;
    return res;
}

/* exponentiation-by-squaring with bitwise ops only */
W power(W base, W exp)
{
    W res = 1;
loop_power:
    if (exp & 1)
        res = multiply_mod(res, base);
    exp >>= 1;
    if (is_equal(exp, 0))
        goto end_power;
    base = multiply_mod(base, base);
    goto loop_power;
end_power:
    return res;
}

/* ---------------- NTT tables ---------------- */

W rev[ROOT_PW] = {0};
void init_bit_reverse(W n)
{
    W i = 1, j = 0;
loop_init:
    if (!is_less_than(i, n))
        goto end_init;
    {
        W bit = n >> 1;
    inner_loop:
        if (!(j & bit))
            goto inner_end;
        j ^= bit;
        bit >>= 1;
        goto inner_loop;
    inner_end:
        j ^= bit;
        rev[i] = j;
        i = add(i, 1);
        goto loop_init;
    }
end_init:
    return;
}

W root_powers[24] = {0};
W root_1_powers[24] = {0};

void init_root_powers()
{
    W i = 0, exp = ROOT_PW, base = ROOT;
    root_powers[0] = 1;
loop_root:
    if (is_equal(i, 23))
        goto end_root;
    i = add(i, 1);
    exp >>= 1;
    root_powers[i] = power(base, exp);
    goto loop_root;
end_root:
    i = 0;
    exp = ROOT_PW;
    base = ROOT_1;
    root_1_powers[0] = 1;
loop_root_1:
    if (is_equal(i, 23))
        goto end_root_1;
    i = add(i, 1);
    exp >>= 1;
    root_1_powers[i] = power(base, exp);
    goto loop_root_1;
end_root_1:
    return;
}

W inv_n[24] = {0};
void init_inv_n()
{
    W i = 0, n = 1;
loop_inv:
    if (is_equal(i, 24))
        goto end_inv;
    inv_n[i] = power(n, sub(MOD, 2));
    n <<= 1;
    i = add(i, 1);
    goto loop_inv;
end_inv:
    return;
}

/* ---------------- iterative NTT (gotos only) ---------------- */

void fft(W *arr, W invert, W n)
{
    W i = 1, j = 0;
    W temp, len, len_half, wlen, i_outer, w, u, v;

    /* bit-reversal permutation */
    i = 1;
perm_loop:
    if (!is_less_than(i, n))
        goto perm_done;
    if (is_less_than(i, rev[i]))
    {
        temp = arr[i];
        arr[i] = arr[rev[i]];
        arr[rev[i]] = temp;
    }
    i = add(i, 1);
    goto perm_loop;
perm_done:

    /* len = 2 special-case: w == 1 */
    i_outer = 0;
len2_loop:
    if (!is_less_than(i_outer, n))
        goto len2_done;
    u = arr[i_outer];
    v = arr[add(i_outer, 1)];
    arr[i_outer] = add(u, v);
    if (!is_less_than(arr[i_outer], MOD))
        arr[i_outer] = subtract_mod(arr[i_outer], MOD);
    arr[add(i_outer, 1)] = subtract_mod(u, v);
    i_outer = add(i_outer, 2);
    goto len2_loop;
len2_done:

    /* main stages */
    len = 4;
stage_loop:
    if (is_greater_than(len, n))
        goto stages_done;
    len_half = len >> 1;

    /* compute wlen from log2(len) */
    {
        W log_len = 0, tlen = len;
    log_len_loop:
        if (is_equal(tlen, 1))
            goto log_len_done;
        log_len = add(log_len, 1);
        tlen >>= 1;
        goto log_len_loop;
    log_len_done:
        wlen = root_powers[log_len];
        if (is_equal(invert, 1))
            wlen = root_1_powers[log_len];
    }

    i_outer = 0;
outer_loop:
    if (!is_less_than(i_outer, n))
        goto outer_done;
    w = 1;
    j = 0;
inner_loop:
    if (!is_less_than(j, len_half))
        goto inner_done;
    {
        W idx1 = add(i_outer, j);
        W idx2 = add(idx1, len_half);
        u = arr[idx1];
        v = multiply_mod(arr[idx2], w);
        arr[idx1] = add(u, v);
        if (!is_less_than(arr[idx1], MOD))
            arr[idx1] = subtract_mod(arr[idx1], MOD);
        arr[idx2] = subtract_mod(u, v);
        w = multiply_mod(w, wlen);
        j = add(j, 1);
        goto inner_loop;
    }
inner_done:
    i_outer = add(i_outer, len);
    goto outer_loop;
outer_done:
    len <<= 1;
    goto stage_loop;
stages_done:

    /* scale by n^{-1} for inverse */
    if (is_equal(invert, 1))
    {
        W log_n = 0, tn = n;
    log_n_loop:
        if (is_equal(tn, 1))
            goto log_n_done;
        log_n = add(log_n, 1);
        tn >>= 1;
        goto log_n_loop;
    log_n_done:
    {
        W ninv = inv_n[log_n];
        i_outer = 0;
    final_scale:
        if (!is_less_than(i_outer, n))
            goto final_done;
        arr[i_outer] = multiply_mod(arr[i_outer], ninv);
        i_outer = add(i_outer, 1);
        goto final_scale;
    final_done:;
    }
    }
}

/* ---------------- 64-bit bitwise helpers (for /10 only) ---------------- */

int is_equal64(WW a, WW b) { return !(a ^ b); }

WW add64(WW a, WW b)
{
    WW carry;
loop_a64:
    carry = a & b;
    a = a ^ b;
    b = carry << 1;
    if (is_equal64(b, 0))
        goto done_a64;
    goto loop_a64;
done_a64:
    return a;
}

WW sub64(WW a, WW b) { return add64(a, add64(~b, (WW)1)); }

int is_greater_than64(WW a, WW b)
{
    if (is_equal64(a, b))
        return 0;
    /* unsigned compare via borrow */
    WW diff = sub64(a, b);
    /* if a<b then msb of diff is 1 in two's complement unsigned? easier: a<b iff (a^b)&(a^diff) has MSB set. */
    WW x_xor_y = a ^ b;
    WW diff_xor_y = diff ^ b;
    WW or_part = x_xor_y | diff_xor_y;
    WW sign = (a ^ or_part) >> 63;
    /* sign==0 means a<b; we want a>b */
    return ((~(int)sign) & (~(int)is_equal64(a, b))) & 1;
}

/* fast unsigned division by 10 using only shifts, adds, gotos */
static inline WW divu10_u64(WW n, WW *r_out)
{
    WW q = add64(n >> 1, n >> 2);
    q = add64(q, q >> 4);
    q = add64(q, q >> 8);
    q = add64(q, q >> 16);
    q = add64(q, q >> 32);
    q >>= 3;

    WW ten_q = add64(q << 3, q << 1); /* 8q + 2q */
    WW r = sub64(n, ten_q);

    if (is_greater_than64(r, (WW)9))
    {
        q = add64(q, (WW)1);
        r = sub64(r, (WW)10);
    }
    if (r_out)
        *r_out = r;
    return q;
}

/* ---------------- main ---------------- */

int main()
{
    init_root_powers();
    init_inv_n();

    W i, len_a, len_b, combined_len, n, result_len;
    W carry;

    (void)scanf("%s", str_a);
    (void)scanf("%s", str_b);

    /* lengths (with gotos) */
    i = 0;
len_a_loop:
    if (str_a[i] == '\0')
        goto len_a_done;
    i = add(i, 1);
    goto len_a_loop;
len_a_done:
    len_a = i;

    i = 0;
len_b_loop:
    if (str_b[i] == '\0')
        goto len_b_done;
    i = add(i, 1);
    goto len_b_loop;
len_b_done:
    len_b = i;

    /* convert to little-endian digits (chars '0'..'9') */
    i = 0;
conv_a_loop:
    if (!is_less_than(i, len_a))
        goto conv_a_done;
    a_digits[i] = sub((W)str_a[sub(sub(len_a, 1), i)], (W)'0');
    i = add(i, 1);
    goto conv_a_loop;
conv_a_done:

    i = 0;
conv_b_loop:
    if (!is_less_than(i, len_b))
        goto conv_b_done;
    b_digits[i] = sub((W)str_b[sub(sub(len_b, 1), i)], (W)'0');
    i = add(i, 1);
    goto conv_b_loop;
conv_b_done:

    /* choose n = next power of two >= len_a + len_b */
    combined_len = add(len_a, len_b);
    n = 1;
grow_n:
    if (is_less_than(n, combined_len))
    {
        n <<= 1;
        goto grow_n;
    }

    init_bit_reverse(n);

    fft(a_digits, 0, n);

    fft(b_digits, 0, n);

    /* pointwise multiply */
    i = 0;
pointwise:
    if (!is_less_than(i, n))
        goto pointwise_done;
    a_digits[i] = multiply_mod(a_digits[i], b_digits[i]);
    i = add(i, 1);
    goto pointwise;
pointwise_done:

    fft(a_digits, 1, n);

    /* normalize base-10 digits, carry propagate using divu10_u64 */
    carry = 0;
    i = 0;
normalize:
    if (!is_less_than(i, n) && is_equal(carry, 0))
        goto normalize_done;
    if (is_equal(i, n))
    {
        a_digits[i] = 0;
        n = add(n, 1);
    }
    a_digits[i] = add(a_digits[i], carry);

    {
        WW q, r;
        q = divu10_u64((WW)a_digits[i], &r);
        carry = (W)q;
        a_digits[i] = (W)r; /* 0..9 */
    }

    i = add(i, 1);
    goto normalize;
normalize_done:

    /* trim leading zeros */
    result_len = n;
    i = add(result_len, (W) ~(W)0);
trim_loop:
    if (is_equal(i, 0))
        goto trim_done;
    if (is_equal(a_digits[i], 0))
    {
        result_len = add(result_len, (W) ~(W)0);
        i = add(i, (W) ~(W)0);
        goto trim_loop;
    }
trim_done:

    /* print */
    i = result_len;
    if (is_less_than(i, 1))
    {
        printf("0\n");
        goto end_print;
    }
print_loop:
    if (is_equal(i, 0))
        goto end_print;
    i = add(i, (W) ~(W)0);
    printf("%d", a_digits[i]);
    goto print_loop;
end_print:
    printf("\n");
    return 0;
}
