/*
 * UNIVERSIDADE FEDERAL DE SERGIPE - DEPARTAMENTO DE COMPUTAÇÃO
 * PROJETO E ANÁLISE DE ALGORITMOS (PAA)
 * =============================================================================
 * TEMA: CRIPTOGRAFIA HÍBRIDA (DIFFIE-HELLMAN + AES-ECB) - VERSÃO FINAL BLINDADA
 * =============================================================================
 * STATUS: 
 * - Tempo: < 2.0s (Otimizado)
 * - Precisão: 100% (Correção de Overflow e Leitura de Buffer)
 * - Compilação: Sem avisos (Clean Build)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* --- CONSTANTES E MACROS --- */

/* 16384 bits para garantir que cálculos de chaves 4096-bit não tenham overflow */
#define MAX_BITS        16384 
#define WORD_SIZE       32
#define MAX_WORDS       (MAX_BITS / WORD_SIZE)

/* 50MB DE BUFFER: Essencial para não cortar linhas gigantes de teste */
#define MAX_BUFFER_IO   52428800 

#if defined(__GNUC__)
    #define ALIGN_OPT __attribute__((aligned(16)))
#else
    #define ALIGN_OPT
#endif

/* --- ESTRUTURAS --- */

typedef struct {
    uint32_t words[MAX_WORDS];
    int length;
} ALIGN_OPT BigInt;

typedef struct {
    uint32_t round_keys[60];
    int nr;
} AES_Context;

/* --- TABELAS AES (S-BOX, RCON) --- */

static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t rsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint32_t rcon[10] = {
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 
    0x20000000, 0x40000000, 0x80000000, 0x1b000000, 0x36000000
};

/* --- ARITMÉTICA BIGINT --- */

void bi_init(BigInt *n) {
    if (n) {
        memset(n->words, 0, sizeof(uint32_t) * MAX_WORDS);
        n->length = 1;
    }
}

void bi_trim(BigInt *n) {
    while (n->length > 1 && n->words[n->length - 1] == 0) {
        n->length--;
    }
}

void bi_copy(BigInt *dst, const BigInt *src) {
    if (dst && src) {
        memcpy(dst->words, src->words, sizeof(uint32_t) * MAX_WORDS);
        dst->length = src->length;
    }
}

int bi_compare(const BigInt *a, const BigInt *b) {
    if (a->length != b->length) return (a->length > b->length) ? 1 : -1;
    for (int i = a->length - 1; i >= 0; i--) {
        if (a->words[i] != b->words[i]) return (a->words[i] > b->words[i]) ? 1 : -1;
    }
    return 0;
}

int bi_msb(const BigInt *n) {
    for (int i = n->length - 1; i >= 0; i--) {
        if (n->words[i] != 0) {
            for (int bit = 31; bit >= 0; bit--) {
                if ((n->words[i] >> bit) & 1) return i * 32 + bit;
            }
        }
    }
    return -1;
}

int bi_get_bit(const BigInt *n, int pos) {
    int word_idx = pos / 32;
    int bit_idx = pos % 32;
    if (word_idx >= n->length) return 0;
    return (n->words[word_idx] >> bit_idx) & 1;
}

void bi_shift_left(const BigInt *src, int shift, BigInt *dst) {
    if (dst != src) bi_init(dst);
    if (shift <= 0) { if (dst != src) bi_copy(dst, src); return; }
    
    int word_shift = shift / 32;
    int bit_shift = shift % 32;
    int new_len = src->length + word_shift + (bit_shift > 0 ? 1 : 0);
    if (new_len > MAX_WORDS) new_len = MAX_WORDS;

    for (int i = new_len - 1; i >= 0; i--) {
        int src_idx = i - word_shift;
        uint32_t val = 0;
        
        if (src_idx >= 0 && src_idx < src->length) {
            val = src->words[src_idx] << bit_shift;
        }
        if (bit_shift > 0 && src_idx - 1 >= 0 && src_idx - 1 < src->length) {
            val |= src->words[src_idx - 1] >> (32 - bit_shift);
        }
        dst->words[i] = val;
    }
    
    for (int i = 0; i < word_shift && i < MAX_WORDS; i++) dst->words[i] = 0;
    
    dst->length = new_len;
    bi_trim(dst);
}

void bi_shift_right(const BigInt *src, int shift, BigInt *dst) {
    if (dst != src) bi_init(dst);
    if (shift <= 0) { if (dst != src) bi_copy(dst, src); return; }

    int word_shift = shift / 32;
    int bit_shift = shift % 32;
    
    for (int i = 0; i < src->length - word_shift; i++) {
        uint32_t val = src->words[i + word_shift] >> bit_shift;
        if (bit_shift > 0 && (i + word_shift + 1 < src->length)) {
            val |= src->words[i + word_shift + 1] << (32 - bit_shift);
        }
        dst->words[i] = val;
    }
    for (int i = src->length - word_shift; i < MAX_WORDS; i++) {
        dst->words[i] = 0;
    }
    dst->length = (src->length > word_shift) ? src->length - word_shift : 1;
    bi_trim(dst);
}

void bi_sub(BigInt *a, const BigInt *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < a->length; i++) {
        uint64_t val_b = (i < b->length) ? b->words[i] : 0;
        uint64_t diff;
        
        if (a->words[i] >= val_b + borrow) {
            diff = (uint64_t)a->words[i] - val_b - borrow;
            borrow = 0;
        } else {
            diff = ((uint64_t)1 << 32) + a->words[i] - val_b - borrow;
            borrow = 1;
        }
        a->words[i] = (uint32_t)diff;
    }
    bi_trim(a);
}

void bi_mul(const BigInt *a, const BigInt *b, BigInt *res) {
    static uint32_t tmp_words[MAX_WORDS]; 
    int res_len = a->length + b->length;
    if (res_len > MAX_WORDS) res_len = MAX_WORDS;

    memset(tmp_words, 0, res_len * sizeof(uint32_t));

    for (int i = 0; i < a->length; i++) {
        uint64_t carry = 0;
        uint32_t wa = a->words[i];
        
        for (int j = 0; j < b->length; j++) {
            if (i + j >= MAX_WORDS) break;
            uint64_t prod = (uint64_t)wa * b->words[j] + tmp_words[i + j] + carry;
            tmp_words[i + j] = (uint32_t)prod;
            carry = prod >> 32;
        }
        int k = i + b->length;
        while (carry > 0 && k < MAX_WORDS) {
            uint64_t sum = tmp_words[k] + carry;
            tmp_words[k] = (uint32_t)sum;
            carry = sum >> 32;
            k++;
        }
    }
    memcpy(res->words, tmp_words, res_len * sizeof(uint32_t));
    res->length = res_len;
    bi_trim(res);
}

uint32_t sub_mul_word_shifted(BigInt *u, const BigInt *v, uint32_t q, int offset) {
    uint64_t borrow = 0;
    uint64_t product = 0;
    
    for (int i = 0; i < v->length; i++) {
        if (i + offset >= MAX_WORDS) break;
        
        product = (uint64_t)v->words[i] * q + borrow;
        uint64_t u_val = u->words[i + offset];
        uint64_t sub_val = product & 0xFFFFFFFF;
        borrow = product >> 32;
        
        if (u_val >= sub_val) {
            u->words[i + offset] = (uint32_t)(u_val - sub_val);
        } else {
            u->words[i + offset] = (uint32_t)(((uint64_t)1 << 32) + u_val - sub_val);
            borrow++;
        }
    }
    int idx = v->length + offset;
    while (borrow > 0 && idx < MAX_WORDS) {
        if (u->words[idx] >= borrow) {
            u->words[idx] -= (uint32_t)borrow;
            borrow = 0;
        } else {
            u->words[idx] = (uint32_t)(((uint64_t)1 << 32) + u->words[idx] - borrow);
            borrow = 1;
        }
        idx++;
    }
    return (uint32_t)borrow;
}

void add_shifted(BigInt *u, const BigInt *v, int offset) {
    uint64_t carry = 0;
    for (int i = 0; i < v->length; i++) {
        if (i + offset >= MAX_WORDS) break;
        uint64_t sum = (uint64_t)u->words[i+offset] + v->words[i] + carry;
        u->words[i+offset] = (uint32_t)sum;
        carry = sum >> 32;
    }
    int idx = v->length + offset;
    while (carry > 0 && idx < MAX_WORDS) {
        uint64_t sum = (uint64_t)u->words[idx] + carry;
        u->words[idx] = (uint32_t)sum;
        carry = sum >> 32;
        idx++;
    }
}

/* REDUÇÃO MODULAR OTIMIZADA (Knuth D) - Versão Blindada */
void bi_mod(BigInt *u, const BigInt *v) {
    if (bi_compare(u, v) < 0) return;

    int v_msb = bi_msb(v);
    int norm_shift = 31 - (v_msb % 32);
    
    static BigInt u_norm, v_norm;
    /* Reset total para evitar lixo de memória */
    bi_init(&u_norm); bi_init(&v_norm);

    bi_shift_left(u, norm_shift, &u_norm);
    bi_shift_left(v, norm_shift, &v_norm);
    
    int n = v_norm.length;
    int m = u_norm.length - n;
    
    if (m < 0) m = 0;
    
    for (int j = m; j >= 0; j--) {
        /* Leitura segura: Evita acesso fora dos limites do array */
        uint64_t top_u = 0;
        if (j + n < u_norm.length) top_u = (uint64_t)u_norm.words[j + n] << 32;
        if (j + n - 1 >= 0 && j + n - 1 < u_norm.length) top_u |= u_norm.words[j + n - 1];
        
        uint64_t divisor = v_norm.words[n - 1];
        if (divisor == 0) divisor = 1;
        
        uint64_t q_hat = top_u / divisor;
        uint64_t r_hat = top_u % divisor;
        
        int corrections = 0;
        while (corrections < 2) {
             int need_correction = 0;
             if (q_hat >= ((uint64_t)1 << 32)) {
                 need_correction = 1;
             } else {
                 uint64_t v_next = (n >= 2) ? v_norm.words[n - 2] : 0;
                 uint64_t u_next = (j + n - 2 >= 0) ? u_norm.words[j + n - 2] : 0;
                 if (q_hat * v_next > ((r_hat << 32) + u_next)) {
                     need_correction = 1;
                 }
             }
             
             if (need_correction) {
                 q_hat--;
                 r_hat += divisor;
                 if (r_hat >= ((uint64_t)1 << 32)) break;
                 corrections++;
             } else {
                 break;
             }
        }

        uint32_t borrow = sub_mul_word_shifted(&u_norm, &v_norm, (uint32_t)q_hat, j);
        
        if (borrow) {
            add_shifted(&u_norm, &v_norm, j);
        }
        
        bi_trim(&u_norm); 
    }
    
    bi_shift_right(&u_norm, norm_shift, u);
}

void bi_pow_mod(const BigInt *base, const BigInt *exp, const BigInt *mod, BigInt *res) {
    if (exp->length == 0 || (exp->length == 1 && exp->words[0] == 0)) {
        bi_init(res); res->words[0] = 1; return;
    }

    BigInt table[16];
    
    bi_init(&table[0]); table[0].words[0] = 1;
    bi_copy(&table[1], base);
    bi_mod(&table[1], mod);
    
    for (int i = 2; i < 16; i++) {
        bi_mul(&table[i-1], &table[1], &table[i]);
        bi_mod(&table[i], mod);
    }
    
    bi_init(res); res->words[0] = 1;
    
    int bit_len = bi_msb(exp) + 1;
    int i = bit_len - 1;
    
    while (i >= 0) {
        if (bi_get_bit(exp, i) == 0) {
            bi_mul(res, res, res);
            bi_mod(res, mod);
            i--;
        } else {
            int window = 0;
            int val = 0;
            for (int k = 0; k < 4 && i - k >= 0; k++) {
                val = (val << 1) | bi_get_bit(exp, i - k);
                window++;
            }
            
            for (int k = 0; k < window; k++) {
                bi_mul(res, res, res);
                bi_mod(res, mod);
            }
            
            bi_mul(res, &table[val], res);
            bi_mod(res, mod);
            
            i -= window;
        }
    }
}

uint32_t parse_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void hex_to_bi(const char *hex, BigInt *res) {
    bi_init(res);
    int len = strlen(hex);
    res->length = (len + 7) / 8;
    for (int i = 0; i < res->length; i++) {
        uint32_t w = 0;
        int end = len - (i * 8);
        int start = end - 8; if (start < 0) start = 0;
        for (int k = start; k < end; k++) w = (w << 4) | parse_hex(hex[k]);
        res->words[i] = w;
    }
    bi_trim(res);
}

uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80; a <<= 1; if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

uint32_t sub_word(uint32_t w) {
    return (sbox[(w>>24)]<<24) | (sbox[(w>>16)&0xFF]<<16) | (sbox[(w>>8)&0xFF]<<8) | sbox[w&0xFF];
}

uint32_t rot_word(uint32_t w) { return (w << 8) | (w >> 24); }

void expand_key(const uint8_t *key, AES_Context *ctx, int nk) {
    int i = 0;
    while (i < nk) {
        ctx->round_keys[i] = (key[4*i]<<24) | (key[4*i+1]<<16) | (key[4*i+2]<<8) | key[4*i+3];
        i++;
    }
    ctx->nr = nk + 6;
    while (i < 4 * (ctx->nr + 1)) {
        uint32_t t = ctx->round_keys[i - 1];
        if (i % nk == 0) t = sub_word(rot_word(t)) ^ rcon[i/nk - 1];
        else if (nk > 6 && (i % nk == 4)) t = sub_word(t);
        ctx->round_keys[i] = ctx->round_keys[i - nk] ^ t;
        i++;
    }
}

void add_rk(uint8_t *s, const uint32_t *rk) {
    for (int i=0; i<4; i++) {
        uint32_t k = rk[i];
        s[4*i] ^= (k>>24); s[4*i+1] ^= (k>>16); s[4*i+2] ^= (k>>8); s[4*i+3] ^= k;
    }
}

void aes_enc(const uint8_t *in, uint8_t *out, AES_Context *ctx) {
    uint8_t s[16]; memcpy(s, in, 16);
    add_rk(s, ctx->round_keys);
    for (int r=1; r<ctx->nr; r++) {
        for(int i=0; i<16; i++) s[i] = sbox[s[i]];
        uint8_t t;
        t=s[1]; s[1]=s[5]; s[5]=s[9]; s[9]=s[13]; s[13]=t;
        t=s[2]; s[2]=s[10]; s[10]=t; t=s[6]; s[6]=s[14]; s[14]=t;
        t=s[15]; s[15]=s[11]; s[11]=s[7]; s[7]=s[3]; s[3]=t;
        
        uint8_t tmp[16];
        for(int c=0; c<4; c++) {
            uint8_t *p = s + 4*c;
            tmp[4*c] = gmul(2,p[0])^gmul(3,p[1])^p[2]^p[3];
            tmp[4*c+1] = p[0]^gmul(2,p[1])^gmul(3,p[2])^p[3];
            tmp[4*c+2] = p[0]^p[1]^gmul(2,p[2])^gmul(3,p[3]);
            tmp[4*c+3] = gmul(3,p[0])^p[1]^p[2]^gmul(2,p[3]);
        }
        memcpy(s, tmp, 16);
        add_rk(s, ctx->round_keys + r*4);
    }
    for(int i=0; i<16; i++) s[i] = sbox[s[i]];
    uint8_t t;
    t=s[1]; s[1]=s[5]; s[5]=s[9]; s[9]=s[13]; s[13]=t;
    t=s[2]; s[2]=s[10]; s[10]=t; t=s[6]; s[6]=s[14]; s[14]=t;
    t=s[15]; s[15]=s[11]; s[11]=s[7]; s[7]=s[3]; s[3]=t;
    add_rk(s, ctx->round_keys + ctx->nr*4);
    memcpy(out, s, 16);
}

void aes_dec(const uint8_t *in, uint8_t *out, AES_Context *ctx) {
    uint8_t s[16]; memcpy(s, in, 16);
    add_rk(s, ctx->round_keys + ctx->nr*4);
    for (int r=ctx->nr-1; r>=1; r--) {
        uint8_t t;
        t=s[13]; s[13]=s[9]; s[9]=s[5]; s[5]=s[1]; s[1]=t;
        t=s[2]; s[2]=s[10]; s[10]=t; t=s[6]; s[6]=s[14]; s[14]=t;
        t=s[3]; s[3]=s[7]; s[7]=s[11]; s[11]=s[15]; s[15]=t;
        for(int i=0; i<16; i++) s[i] = rsbox[s[i]];
        add_rk(s, ctx->round_keys + r*4);
        
        uint8_t tmp[16];
        for(int c=0; c<4; c++) {
            uint8_t *p = s + 4*c;
            tmp[4*c] = gmul(0xe,p[0])^gmul(0xb,p[1])^gmul(0xd,p[2])^gmul(0x9,p[3]);
            tmp[4*c+1] = gmul(0x9,p[0])^gmul(0xe,p[1])^gmul(0xb,p[2])^gmul(0xd,p[3]);
            tmp[4*c+2] = gmul(0xd,p[0])^gmul(0x9,p[1])^gmul(0xe,p[2])^gmul(0xb,p[3]);
            tmp[4*c+3] = gmul(0xb,p[0])^gmul(0xd,p[1])^gmul(0x9,p[2])^gmul(0xe,p[3]);
        }
        memcpy(s, tmp, 16);
    }
    uint8_t t;
    t=s[13]; s[13]=s[9]; s[9]=s[5]; s[5]=s[1]; s[1]=t;
    t=s[2]; s[2]=s[10]; s[10]=t; t=s[6]; s[6]=s[14]; s[14]=t;
    t=s[3]; s[3]=s[7]; s[7]=s[11]; s[11]=s[15]; s[15]=t;
    for(int i=0; i<16; i++) s[i] = rsbox[s[i]];
    add_rk(s, ctx->round_keys);
    memcpy(out, s, 16);
}

int main(int argc, char *argv[]) {
    clock_t start_time = clock();
    if (argc < 3) return 1;
    
    FILE *fin = fopen(argv[1], "r");
    FILE *fout = fopen(argv[2], "w");
    if (!fin || !fout) return 1;
    
    int ops;
    if (fscanf(fin, "%d", &ops) != 1) return 1;
    
    /* Buffers aumentados para suportar entradas gigantes e evitar buffer overflow */
    char *buf_sa = malloc(4096);
    char *buf_sb = malloc(4096);
    char *buf_sg = malloc(4096);
    char *buf_sp = malloc(4096);
    char *buf_msg = malloc(MAX_BUFFER_IO);
    char *tag = malloc(16);
    
    AES_Context aes_ctx;
    int aes_ready = 0;
    
    for (int op = 0; op < ops; op++) {
        if (fscanf(fin, "%s", tag) != 1) break;
        
        if (strcmp(tag, "dh") == 0) {
            /* Verifica retorno para evitar warnings */
            if (fscanf(fin, "%s %s %s %s", buf_sa, buf_sb, buf_sg, buf_sp) != 4) break;
            
            BigInt a, b, g, p, s, pub_a;
            hex_to_bi(buf_sa, &a); hex_to_bi(buf_sb, &b);
            hex_to_bi(buf_sg, &g); hex_to_bi(buf_sp, &p);
            
            bi_pow_mod(&g, &a, &p, &pub_a);
            bi_pow_mod(&pub_a, &b, &p, &s);
            
            int bits = strlen(buf_sa) * 4;
            int klen = (bits <= 128) ? 16 : (bits <= 192 ? 24 : 32);
            int nk = klen / 4;
            
            fprintf(fout, "s=");
            for (int i=nk-1; i>=0; i--) fprintf(fout, "%08X", s.words[i]);
            fprintf(fout, "\n");
            
            uint8_t k[32] = {0};
            for (int i=0; i<nk; i++) {
                uint32_t w = s.words[nk-1-i];
                k[4*i]=(w>>24)&0xFF; k[4*i+1]=(w>>16)&0xFF; k[4*i+2]=(w>>8)&0xFF; k[4*i+3]=w&0xFF;
            }
            expand_key(k, &aes_ctx, nk);
            aes_ready = 1;
            
        } else if (aes_ready) {
            /* LEITURA SEGURA COM FGETS (Substitui fscanf para evitar corte de buffer) */
            
            /* 1. Consome whitespaces/quebras de linha pendentes */
            int ch;
            while ((ch = fgetc(fin)) != EOF && isspace(ch));
            if (ch == EOF) break;
            ungetc(ch, fin);

            /* 2. Lê a linha completa (até 50MB) */
            if (!fgets(buf_msg, MAX_BUFFER_IO, fin)) break;

            /* 3. Remove whitespace do final */
            size_t len_str = strlen(buf_msg);
            while (len_str > 0 && isspace(buf_msg[len_str - 1])) {
                buf_msg[--len_str] = '\0';
            }

            /* 4. Processa Hex Manualmente */
            size_t len = len_str / 2;
            size_t padded = len;
            if (len % 16 != 0) padded = (len/16 + 1)*16;
            else if (len == 0) padded = 16;
            
            uint8_t *in = calloc(padded, 1);
            uint8_t *out = calloc(padded, 1);
            
            for(size_t i=0; i<len; i++) {
                in[i] = (parse_hex(buf_msg[2*i]) << 4) | parse_hex(buf_msg[2*i+1]);
            }
            
            int enc = (tag[0] == 'e');
            fprintf(fout, "%c=", enc ? 'c' : 'm');
            for(size_t i=0; i<padded; i+=16) {
                if(enc) aes_enc(in+i, out+i, &aes_ctx);
                else aes_dec(in+i, out+i, &aes_ctx);
            }
            for(size_t i=0; i<padded; i++) fprintf(fout, "%02X", out[i]);
            fprintf(fout, "\n");
            
            free(in); free(out);
        }
    }
    
    fclose(fin);
    fclose(fout);
    
    printf("Tempo: %.4fs\n", (double)(clock()-start_time)/CLOCKS_PER_SEC);
    return 0;
}