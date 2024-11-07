// Same as kcrypt2b but uses RIPEMD160 to protect the MT19937 state.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// ========================================================
// Prelude.
// ========================================================
#define Fi(n, a...)for(int i=0;i<n;i++){a;}
#define Fj(n, a...)for(int j=0;j<n;j++){a;}
#define Fk(n, a...)for(int k=0;k<n;k++){a;}
#define Fid(n, a...)for(int i=n-1;i>=0;i--){a;}
#define Fid1(n, a...)for(int i=n-1;i>0;i--){a;}
#define Fj(n, a...)for(int j=0;j<n;j++){a;}
#define XCH(k,x,y){k t=x;x=y;y=t;};
#define _(a...) {return({a;});}
#define S static
#define V void
// GF(2^8) generated by 0x1D, MT19937.
typedef uint8_t u8;   typedef uint16_t u16;
typedef uint32_t u32; typedef uint64_t u64;
S u8 M[256][256], D[256], LOG[256], EXP[510];   S FILE * rng;
S V init() {
  int b = 1;  rng = fopen("/dev/random", "rb");
  Fi(255, LOG[b] = i;  EXP[i] = EXP[i + 255] = b;
          if ((b <<= 1) >= 256) b = (b - 256) ^ 0x1D)
  Fi(256, Fj(256, if (i && j) M[i][j] = EXP[LOG[i] + LOG[j]]))
  Fi(256, int d = LOG[1] - LOG[i]; D[i] = EXP[d < 0 ? d + 255 : d])}
S int cleanup()_(fclose(rng))
typedef struct { u32 mt[624], mti; } key;
S u32 mtn(key * k) {
  u32 y, mag01[2] = { 0, 0x9908b0df };
  if (k->mti >= 624) {
    Fi(227, y = (k->mt[i] & 0x80000000) | (k->mt[i + 1] & 0x7fffffff);
            k->mt[i] = k->mt[i + 397] ^ (y >> 1) ^ mag01[y & 1])
    Fi(396, y = (k->mt[i + 227] & 0x80000000) | (k->mt[i + 228] & 0x7fffffff);
            k->mt[i + 227] = k->mt[i] ^ (y >> 1) ^ mag01[y & 1])
    y = (k->mt[623] & 0x80000000) | (k->mt[0] & 0x7fffffff);
    k->mt[623] = k->mt[396] ^ (y >> 1) ^ mag01[y & 1]; k->mti = 0;
  }
  y = k->mt[k->mti++];         y ^= y >> 11;
  y ^= (y << 7) & 0x9d2c5680;  y ^= (y << 15) & 0xefc60000;
  return y ^ (y >> 18);}
// ========================================================
// http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/efaq.html
// ========================================================
void mtcrypt(key * k[16], u8 * dest) {
  // Write 16 bytes of data to dest. Based on RIPEMD-160.
  u32 state[5];
  state[0] = 0x67452301;
  state[1] = 0xEFCDAB89;
  state[2] = 0x98BADCFE;
  state[3] = 0x10325476;
  state[4] = 0xC3D2E1F0;
  u32 X[16]; Fi(16, X[i] = mtn(k[i]));
  uint32_t A, B, C, D, E, Ap, Bp, Cp, Dp, Ep;
  A = Ap = state[0]; B = Bp = state[1]; C = Cp = state[2];
  D = Dp = state[3]; E = Ep = state[4];
#define F1( x, y, z )   ( x ^ y ^ z )
#define F2( x, y, z )   ( ( x & y ) | ( ~x & z ) )
#define F3( x, y, z )   ( ( x | ~y ) ^ z )
#define F4( x, y, z )   ( ( x & z ) | ( y & ~z ) )
#define F5( x, y, z )   ( x ^ ( y | ~z ) )
#define Sf( x, n ) ( ( x << n ) | ( x >> (32 - n) ) )
#define P( a, b, c, d, e, r, s, f, k )      \
  a += f( b, c, d ) + X[r] + k;           \
  a = Sf( a, s ) + e;                      \
  c = Sf( c, 10 );
#define P2( a, b, c, d, e, r, s, rp, sp )   \
  P( a, b, c, d, e, r, s, F, K );         \
  P( a ## p, b ## p, c ## p, d ## p, e ## p, rp, sp, Fp, Kp );
#define F   F1
#define K   0x00000000
#define Fp  F5
#define Kp  0x50A28BE6
  P2( A, B, C, D, E,  0, 11,  5,  8 );
  P2( E, A, B, C, D,  1, 14, 14,  9 );
  P2( D, E, A, B, C,  2, 15,  7,  9 );
  P2( C, D, E, A, B,  3, 12,  0, 11 );
  P2( B, C, D, E, A,  4,  5,  9, 13 );
  P2( A, B, C, D, E,  5,  8,  2, 15 );
  P2( E, A, B, C, D,  6,  7, 11, 15 );
  P2( D, E, A, B, C,  7,  9,  4,  5 );
  P2( C, D, E, A, B,  8, 11, 13,  7 );
  P2( B, C, D, E, A,  9, 13,  6,  7 );
  P2( A, B, C, D, E, 10, 14, 15,  8 );
  P2( E, A, B, C, D, 11, 15,  8, 11 );
  P2( D, E, A, B, C, 12,  6,  1, 14 );
  P2( C, D, E, A, B, 13,  7, 10, 14 );
  P2( B, C, D, E, A, 14,  9,  3, 12 );
  P2( A, B, C, D, E, 15,  8, 12,  6 );
#define F   F2
#define K   0x5A827999
#define Fp  F4
#define Kp  0x5C4DD124
  P2( E, A, B, C, D,  7,  7,  6,  9 );
  P2( D, E, A, B, C,  4,  6, 11, 13 );
  P2( C, D, E, A, B, 13,  8,  3, 15 );
  P2( B, C, D, E, A,  1, 13,  7,  7 );
  P2( A, B, C, D, E, 10, 11,  0, 12 );
  P2( E, A, B, C, D,  6,  9, 13,  8 );
  P2( D, E, A, B, C, 15,  7,  5,  9 );
  P2( C, D, E, A, B,  3, 15, 10, 11 );
  P2( B, C, D, E, A, 12,  7, 14,  7 );
  P2( A, B, C, D, E,  0, 12, 15,  7 );
  P2( E, A, B, C, D,  9, 15,  8, 12 );
  P2( D, E, A, B, C,  5,  9, 12,  7 );
  P2( C, D, E, A, B,  2, 11,  4,  6 );
  P2( B, C, D, E, A, 14,  7,  9, 15 );
  P2( A, B, C, D, E, 11, 13,  1, 13 );
  P2( E, A, B, C, D,  8, 12,  2, 11 );
#define F   F3
#define K   0x6ED9EBA1
#define Fp  F3
#define Kp  0x6D703EF3
  P2( D, E, A, B, C,  3, 11, 15,  9 );
  P2( C, D, E, A, B, 10, 13,  5,  7 );
  P2( B, C, D, E, A, 14,  6,  1, 15 );
  P2( A, B, C, D, E,  4,  7,  3, 11 );
  P2( E, A, B, C, D,  9, 14,  7,  8 );
  P2( D, E, A, B, C, 15,  9, 14,  6 );
  P2( C, D, E, A, B,  8, 13,  6,  6 );
  P2( B, C, D, E, A,  1, 15,  9, 14 );
  P2( A, B, C, D, E,  2, 14, 11, 12 );
  P2( E, A, B, C, D,  7,  8,  8, 13 );
  P2( D, E, A, B, C,  0, 13, 12,  5 );
  P2( C, D, E, A, B,  6,  6,  2, 14 );
  P2( B, C, D, E, A, 13,  5, 10, 13 );
  P2( A, B, C, D, E, 11, 12,  0, 13 );
  P2( E, A, B, C, D,  5,  7,  4,  7 );
  P2( D, E, A, B, C, 12,  5, 13,  5 );
#define F   F4
#define K   0x8F1BBCDC
#define Fp  F2
#define Kp  0x7A6D76E9
  P2( C, D, E, A, B,  1, 11,  8, 15 );
  P2( B, C, D, E, A,  9, 12,  6,  5 );
  P2( A, B, C, D, E, 11, 14,  4,  8 );
  P2( E, A, B, C, D, 10, 15,  1, 11 );
  P2( D, E, A, B, C,  0, 14,  3, 14 );
  P2( C, D, E, A, B,  8, 15, 11, 14 );
  P2( B, C, D, E, A, 12,  9, 15,  6 );
  P2( A, B, C, D, E,  4,  8,  0, 14 );
  P2( E, A, B, C, D, 13,  9,  5,  6 );
  P2( D, E, A, B, C,  3, 14, 12,  9 );
  P2( C, D, E, A, B,  7,  5,  2, 12 );
  P2( B, C, D, E, A, 15,  6, 13,  9 );
  P2( A, B, C, D, E, 14,  8,  9, 12 );
  P2( E, A, B, C, D,  5,  6,  7,  5 );
  P2( D, E, A, B, C,  6,  5, 10, 15 );
  P2( C, D, E, A, B,  2, 12, 14,  8 );
#define F   F5
#define K   0xA953FD4E
#define Fp  F1
#define Kp  0x00000000
  P2( B, C, D, E, A,  4,  9, 12,  8 );
  P2( A, B, C, D, E,  0, 15, 15,  5 );
  P2( E, A, B, C, D,  5,  5, 10, 12 );
  P2( D, E, A, B, C,  9, 11,  4,  9 );
  P2( C, D, E, A, B,  7,  6,  1, 12 );
  P2( B, C, D, E, A, 12,  8,  5,  5 );
  P2( A, B, C, D, E,  2, 13,  8, 14 );
  P2( E, A, B, C, D, 10, 12,  7,  6 );
  P2( D, E, A, B, C, 14,  5,  6,  8 );
  P2( C, D, E, A, B,  1, 12,  2, 13 );
  P2( B, C, D, E, A,  3, 13, 13,  6 );
  P2( A, B, C, D, E,  8, 14, 14,  5 );
  P2( E, A, B, C, D, 11, 11,  0, 15 );
  P2( D, E, A, B, C,  6,  8,  3, 13 );
  P2( C, D, E, A, B, 15,  5,  9, 11 );
  P2( B, C, D, E, A, 13,  6, 11, 11 );
#undef F
#undef K
#undef Fp
#undef Kp
  state[1] = state[2] + D + Ep;
  state[2] = state[3] + E + Ap;
  state[3] = state[4] + A + Bp;
  state[4] = state[0] + B + Cp;
  u32 * dest_view = (u32 *) dest;
  dest_view[0] = state[1];
  dest_view[1] = state[2];
  dest_view[2] = state[3];
  dest_view[3] = state[4];}
// ========================================================
// Block cipher
// ========================================================
S V lagrange(u8 * x, u8 * y, int n, u8 * coef) {
  memset(coef, 0, n); // Data Compression in Depth, Kamila Szewczyk, pp. 288-290, O(n^2).
  u8 c[n + 1]; memset(c, 0, n + 1); c[0] = 1;
  for (int i = 0; i < n; i++) {
    for (int j = i; j > 0; j--)
      c[j] = c[j - 1] ^ M[c[j]][x[i]];
    c[0] = M[c[0]][x[i]]; c[i + 1] = 1;
  }
  u8 P[n]; memset(P, 0, n);
  for (int i = 0; i < n; i++) {
    u8 d = 1, t;
    for (int j = 0; j < n; j++)
      if (i != j) d = M[d][x[i] ^ x[j]];
    t = M[D[d]][y[i]];
    coef[n-1] ^= M[t][P[n-1] = 1];
    for (int j = n - 2; j >= 0; j--)
      coef[j] ^= M[t][P[j] = c[j+1] ^ M[x[i]][P[j+1]]];
  }
}
S u8 horner(u8 * coef, int n, u8 x) {
  u8 result = coef[n];
  for (int i = n - 1; i >= 0; i--)
    result = M[x][result] ^ coef[i];
  return result;
}
S V fiya(u8 vec[], int n, key * k[16]) {
  int sel = 0, j;
  Fid1(n, j = mtn(k[sel++ % 16]) % (i + 1); XCH(u8, vec[i], vec[j]))}
#define FIYA_EP u8 ep[224]; Fi(224, ep[i] = i + 32) fiya(ep, 224, k)
S V kc2bE(u8 blk[16], key * k[16]) {
  u8 x[32], y[32], coef[32];
  Fi(32, x[i] = i) Fi(16, y[i] = blk[i]) mtcrypt(k, &y[16]);
  lagrange(x, y, 32, coef);
  FIYA_EP; Fi(16, blk[i] = horner(coef, 31, ep[i]));}
S V kc2bD(u8 blk[16], key * k[16]) {
  u8 x[32], y[32], coef[32];
  Fi(16, x[i] = i + 16) mtcrypt(k, y);
  FIYA_EP; Fi(16, x[i + 16] = ep[i], y[i + 16] = blk[i]) 
  lagrange(x, y, 32, coef);
  Fi(16, blk[i] = horner(coef, 31, i));}
// ========================================================
// File encryption/decryption
// ========================================================
S V kc2bEF(FILE * in, FILE * out, key * k[16]) {
  u8 buf[16] = { 0xAA };
  long sz; while((sz = fread(buf, 1, 16, in)) > 0) {
    if (sz < 16) buf[15] = sz; kc2bE(buf, k);
    fwrite(buf, 16, 1, out);  memset(buf, 0xAA, 16);
  }}
S V kc2bDF(FILE * in, FILE * out, key * k[16]) {
  u8 buf1[16], buf2[16], sz;
  if (fread(buf1, 16, 1, in) != 1) return;
  for (; fread(buf2, 16, 1, in) == 1; memcpy(buf1, buf2, 16)) {
    kc2bD(buf1, k);
    fwrite(buf1, 16, 1, out);
  }
  kc2bD(buf1, k); sz = buf1[15]; if (sz > 16) sz = 16; fwrite(buf1, sz, 1, out);}
// ========================================================
// Command-line stub.
// ========================================================
#define E(x) { fprintf(stderr, x); exit(1); }
S void keygen(FILE * out) {
  u8 buf[39936]; fread(buf, sizeof(buf), 1, rng); fwrite(buf, sizeof(buf), 1, out);}
S void keyget(FILE * in, key * k[16]) { // Big Endian?
  Fi(16, k[i]->mti = 0; fread(k[i]->mt, 624, 4, in))}
S FILE * xopen(const char * file, const char * mode) {
  FILE * f = fopen(file, mode); if(!f) { perror("fopen"); exit(1); } else return f;}
int main(int argc, char * argv[]) {
  init();
  if (argc < 2) E("kcrypt2b [-g/-e/-d] [FILES...]\n")
  if (!strcmp(argv[1], "-g")) {
    if (argc < 3) E("kcrypt2b -g key.kck\n");
    FILE * kf = xopen(argv[2], "wb");
    keygen(kf); fclose(kf);
  } else if (!strcmp(argv[1], "-e")) {
    if (argc < 4) E("kcrypt2b -e key.kck plaintext ciphertext\n")
    key * k[16]; Fi(16, k[i] = malloc(sizeof(key)))
    FILE * kf = xopen(argv[2], "rb"); keyget(kf, k); fclose(kf);
    FILE * plaintext = xopen(argv[3], "rb"), * ciphertext = xopen(argv[4], "wb");
    kc2bEF(plaintext, ciphertext, k);
    fclose(plaintext); fclose(ciphertext);
    Fi(16, free(k[i]));
  } else if (!strcmp(argv[1], "-d")) {
    if (argc < 4) E("kcrypt2b -d key.kck ciphertext plaintext\n")
    key * k[16]; Fi(16, k[i] = malloc(sizeof(key)))
    FILE * kf = xopen(argv[2], "rb"); keyget(kf, k); fclose(kf);
    FILE * ciphertext = xopen(argv[3], "rb"), * plaintext = xopen(argv[4], "wb");
    kc2bDF(ciphertext, plaintext, k);
    fclose(plaintext); fclose(ciphertext);
    Fi(16, free(k[i]));
  }
}
