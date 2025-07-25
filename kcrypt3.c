// ---------------------------------------------------------------------------
//      KCrypt3 - 3rd iteration of the KCrypt algorithm.
//      Written on Sunday, 20th of April 2025 by Kamila Szewczyk.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include "yarg.h"

// ---------------------------------------------------------------------------
//      Galois field tables.
// ---------------------------------------------------------------------------
typedef uint8_t gf;
static gf LOG[256], EXP[510], PROD[256][256];
static void gentab(gf poly) {
  for (int l = 0, b = 1; l < 255; l++) {
    LOG[b] = l;  EXP[l] = EXP[l + 255] = b;
    if ((b <<= 1) >= 256)
      b = (b - 256) ^ poly;
  }
  for (int i = 1; i < 256; i++)
    for (int j = 1; j < 256; j++)
      PROD[i][j] = EXP[LOG[i] + LOG[j]];
}
#define gf_mul(a, b) PROD[a][b]
static gf gf_div(gf a, gf b) {
  if (!a || !b) return 0;
  int d = LOG[a] - LOG[b];
  return EXP[d < 0 ? d + 255 : d];
}

// ---------------------------------------------------------------------------
//      Lagrange interpolation and Horner's method in the Galois field.
//      Uses the optimised (numerically unstable) quadratic-time algorithm.
// ---------------------------------------------------------------------------
static void lagrange(gf * x, gf * y, int n, gf * coef) {
  gf c[n + 1]; memset(c, 0, sizeof(gf) * (n + 1)); c[0] = 1;
  for (int i = 0; i < n; i++) {
    for (int j = i; j > 0; j--)
      c[j] = c[j - 1] ^ gf_mul(c[j], x[i]);
    c[0] = gf_mul(c[0], x[i]);  c[i + 1] = 1;
  }
  gf P[n]; memset(P, 0, sizeof(gf) * n);
  for (int i = 0; i < n; i++) {
    gf d = 1, t;
    for (int j = 0; j < n; j++)
      if (i != j) d = gf_mul(d, x[i] ^ x[j]);
    t = gf_div(y[i], d);
    coef[n-1] ^= gf_mul(t, P[n-1] = 1);
    for (int j = n - 2; j >= 0; j--)
      coef[j] ^= gf_mul(t, P[j] = c[j+1] ^ gf_mul(x[i], P[j+1]));
  }
}

static gf horner(gf * coef, int n, gf x) {
  gf result = coef[n];
  for (int i = n - 1; i >= 0; i--)
    result = gf_mul(x, result) ^ coef[i];
  return result;
}

// ---------------------------------------------------------------------------
//      Feistel Network.
// ---------------------------------------------------------------------------
static void fisher(gf k[64], gf x[64]) {
  for (int i = 63; i > 0; i--) {
    int j = k[i] % (i + 1);
    gf t = x[i]; x[i] = x[j]; x[j] = t;
  }
}
static void fisher32(gf k[32], gf x[32]) {
  for (int i = 31; i > 0; i--) {
    int j = k[i] % (i + 1);
    gf t = x[i]; x[i] = x[j]; x[j] = t;
  }
}
static void feistelF(gf b[32], gf k1[32], gf k2[64]) {
  gf x[64], y[64], coeff[64] = { 0 };
  for (int i = 0; i < 64; i++) x[i] = i;
  for (int i = 0; i < 32; i++) y[i] = b[i] + i, y[i + 32] = k1[i] + i;
  fisher(k2, x);  lagrange(x, y, 64, coeff);
  for (int i = 0; i < 32; i++) b[i] = horner(coeff, 63, 255 - i);
}
static void keysched(gf in[32], gf k2[64], gf out[32], gf next[32]) {
  gf x[32], y[32], coeff[32] = { 0 };
  for (int i = 0; i < 32; i++) x[i] = i, y[i] = in[i] + i;
  lagrange(x, y, 32, coeff);  fisher32(k2, x);
  for (int i = 0; i < 32; i++)
    out[i] = horner(coeff, 31, 64 + x[i]),
    next[i] = horner(coeff, 31, 128 + x[i]),
    k2[i] = horner(coeff, 31, 192 + x[i]);
}
static void feistel0(gf L[32], gf R[32], gf k1[3][32], gf k2[64]) {
  for (int round = 0; round < 3; round++) {
    gf temp[32];
    memcpy(temp, R, 32);
    feistelF(R, k1[round], k2);
    for (int i = 0; i < 32; i++) R[i] ^= L[i];
    memcpy(L, temp, 32);
  }
}
static void feistel1(gf L[32], gf R[32], gf k1[3][32], gf k2[64]) {
  for (int round = 2; round >= 0; round--) {
    gf temp[32];
    memcpy(temp, L, 32);
    feistelF(L, k1[round], k2);
    for (int i = 0; i < 32; i++) L[i] ^= R[i];
    memcpy(R, temp, 32);
  }
}
typedef struct { gf k1[32]; gf k2[64]; } block_key_t;
static void encode_block(gf in[64], gf blk[64], uint32_t IV, block_key_t * key) {
  gf keys[3][32];  memcpy(blk, in, 64);
  for (int i = 0; i < 4; i++) key->k1[i] += (IV >> (i * 8)) & 0xff;
  keysched(key->k1, key->k2, keys[0], key->k1);
  keysched(key->k1, key->k2, keys[1], key->k1);
  keysched(key->k1, key->k2, keys[2], key->k1);
  feistel0(blk, blk + 32, keys, key->k2);
}
static void decode_block(gf in[64], gf blk[64], uint32_t IV, block_key_t * key) {
  gf keys[3][32];  memcpy(blk, in, 64);
  for (int i = 0; i < 4; i++) key->k1[i] += (IV >> (i * 8)) & 0xff;
  keysched(key->k1, key->k2, keys[0], key->k1);
  keysched(key->k1, key->k2, keys[1], key->k1);
  keysched(key->k1, key->k2, keys[2], key->k1);
  feistel1(blk, blk + 32, keys, key->k2);
}

// ---------------------------------------------------------------------------
//      Secure randomness source. Supports `dows, DOS and Unix systems.
// ---------------------------------------------------------------------------
static void eprintf(const char * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(1);
}

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
static void secrandom(void * buf, size_t len) {
  HCRYPTPROV hp;
  CryptAcquireContext(&hp, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
  CryptGenRandom(hp, len, buf);
  CryptReleaseContext(hp, 0);
}
#elif __unix__
#include <fcntl.h>
#include <unistd.h>
static void secrandom(void * buf, size_t len) {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0)
    eprintf("Could not open `/dev/urandom': %s\n", strerror(errno));
  if (read(fd, buf, len) < 0)
    eprintf("Could not read from `/dev/urandom': %s\n", strerror(errno));
  close(fd);
}
#elif __MSDOS__
static void secrandom(void * buf, size_t len) { // Doug Kaufman's NOISE.SYS
  FILE * f = fopen("/dev/urandom$", "rb");
  if (!f)
    eprintf("Could not open `/dev/urandom$': %s\n", strerror(errno));
  if (fread(buf, 1, len, f) < len)
    eprintf("Could not read from `/dev/urandom$': %s\n", strerror(errno));
  fclose(f);
}
#endif

// ---------------------------------------------------------------------------
//      Stream ciphers.
// ---------------------------------------------------------------------------
enum { CIPHER_STREAM_FILE, CIPHER_STREAM_BLOCK, CIPHER_STREAM_FUNCTION };
typedef struct {
  int type;
  uint32_t max;
  union {
    FILE * file;
    struct {
      size_t (* read)(void * ptr, size_t size, size_t nmemb, void * stream);
      size_t (* write)(const void * ptr, size_t size, size_t nmemb, void * stream);
      uint32_t (* tell)(void * stream);
      void * stream;
    } stream;
    struct {
      uint8_t * buffer;
      uint32_t size, consumed;
    };
  };
} cipher_aux_t;
static void write32_le_buf(uint32_t val, gf * buf) {
  for (int i = 0; i < 4; i++)
    buf[i] = (val >> (i * 8)) & 0xff;
}
static void read32_le_buf(uint32_t * val, gf * buf) {
  *val = 0;
  for (int i = 0; i < 4; i++)
    *val |= buf[i] << (i * 8);
}
static size_t cipher_aux_fread(void * ptr, size_t size,
    size_t nmemb, cipher_aux_t * stream) {
  if (stream->type == CIPHER_STREAM_FILE) {
    ssize_t read;
    if((read = fread(ptr, size, nmemb, stream->file)) < 0)
      eprintf("Could not read from the input file: %s\n", strerror(errno));
    return read;
  } else if (stream->type == CIPHER_STREAM_FUNCTION) {
    return stream->stream.read(ptr, size, nmemb, stream->stream.stream);
  }
  if (stream->consumed + size * nmemb > stream->size)
    eprintf("Internal error.\n");
  memcpy(ptr, stream->buffer + stream->consumed, size * nmemb);
  stream->consumed += size * nmemb;
  return nmemb;
}
static size_t cipher_aux_fwrite(const void * ptr, size_t size,
    size_t nmemb, cipher_aux_t * stream) {
  if (nmemb == 0 || size == 0)
    return 0;
  if (stream->type == CIPHER_STREAM_FILE) {
    if(fwrite(ptr, size, nmemb, stream->file) != nmemb)
      eprintf("Could not write to the output file.\n", strerror(errno));
    return nmemb;
  }
  else if (stream->type == CIPHER_STREAM_FUNCTION)
    return stream->stream.write(ptr, size, nmemb, stream->stream.stream);
  if (stream->consumed + size * nmemb > stream->size)
    eprintf("Internal error.\n");
  memcpy(stream->buffer + stream->consumed, ptr, size * nmemb);
  stream->consumed += size * nmemb;
  return nmemb;
}
static uint32_t cipher_aux_ftell(cipher_aux_t * stream) {
  if (stream->type == CIPHER_STREAM_FILE)
    return ftell(stream->file);
  else if (stream->type == CIPHER_STREAM_FUNCTION)
    return stream->stream.tell(stream->stream.stream);
  return stream->consumed;
}

typedef void (* fprogress_cb)(uint32_t processed, uint32_t total);
typedef struct {
  fprogress_cb pcb;
  block_key_t key;
  cipher_aux_t input, output;
} mode_params_t;

typedef void (* stream_enc)(mode_params_t * params);
typedef void (* stream_dec)(mode_params_t * params);

static uint32_t cipher_check_header(mode_params_t * params) {
  uint32_t IV; char h[4];
  if (cipher_aux_fread(h, 1, 4, &params->input) != 4)
    eprintf("Truncated input.\n");
  read32_le_buf(&IV, h); return IV;
}
static uint32_t cipher_put_header(char * hdr, mode_params_t * params) {
  size_t hdr_len = strlen(hdr); char actual_hdr[hdr_len + 4];
  memcpy(actual_hdr, hdr, hdr_len);
  uint32_t IV; secrandom(&IV, 4);
  write32_le_buf(IV, actual_hdr + hdr_len);
  cipher_aux_fwrite(actual_hdr, 1, hdr_len + 4, &params->output);
  return IV;
}

// ---------------------------------------------------------------------------
//      CTR mode of operation. Assumes buffers are aligned to 32 bytes.
// ---------------------------------------------------------------------------
static void encode_ctr(mode_params_t * params) {
  uint32_t IV = cipher_put_header("KC3CTR", params);
  gf in[64] = { 0 }, out[64]; int8_t read = 0;
  while ((read = cipher_aux_fread(in, 1, 63, &params->input)) > 0) {
    for (int8_t i = read; i < 63; i++) in[i] = 63 - read;  in[63] = read;
    encode_block(in, out, IV, &params->key);
    if (params->pcb)
      params->pcb(cipher_aux_ftell(&params->input), params->input.max);
    cipher_aux_fwrite(out, 1, 64, &params->output);
    IV++;
  }
}

static void decode_ctr(mode_params_t * params) {
  uint32_t IV = cipher_check_header(params);
  gf in[64], out[64]; int8_t read = 63;
  while (read == 63) {
    if (cipher_aux_fread(in, 1, 64, &params->input) != 64)
      eprintf("Truncated input.\n");
    decode_block(in, out, IV, &params->key);
    if (params->pcb)
      params->pcb(cipher_aux_ftell(&params->input), params->input.max);
    cipher_aux_fwrite(out, 1, read = out[63], &params->output);
    IV++;
  }
}

// ---------------------------------------------------------------------------
//      OFB mode of operation. Assumes buffers are aligned to 64 bytes.
// ---------------------------------------------------------------------------
static void encode_ofb(mode_params_t * params) {
  uint32_t IV = cipher_put_header("KC3OFB", params);
  gf in[64] = { 0 }, out[64], prev_out[64]; int8_t read;
  read = cipher_aux_fread(in, 1, 63, &params->input);
  for (int8_t i = read; i < 63; i++) in[i] = 63 - read;  in[63] = read;
  encode_block(in, prev_out, IV, &params->key);
  if (params->pcb)
    params->pcb(cipher_aux_ftell(&params->input), params->input.max);
  cipher_aux_fwrite(prev_out, 1, 64, &params->output);
  IV++;
  while ((read = cipher_aux_fread(in, 1, 63, &params->input)) > 0) {
    for (int8_t i = read; i < 63; i++) in[i] = 63 - read;  in[63] = read;
    for (int i = 0; i < 64; i++) in[i] ^= prev_out[i];
    encode_block(in, out, IV, &params->key);
    if (params->pcb)
      params->pcb(cipher_aux_ftell(&params->input), params->input.max);
    cipher_aux_fwrite(out, 1, 64, &params->output);
    IV++;
    memcpy(prev_out, out, 64);
  }
}

static void decode_ofb(mode_params_t * params) {
  uint32_t IV = cipher_check_header(params);
  gf in[64], prev_in[64], out[64]; int8_t read;
  if (cipher_aux_fread(prev_in, 1, 64, &params->input) != 64)
    eprintf("Truncated input.\n");
  decode_block(prev_in, out, IV, &params->key);
  if (params->pcb)
    params->pcb(cipher_aux_ftell(&params->input), params->input.max);
  cipher_aux_fwrite(out, 1, read = out[63], &params->output);
  IV++;
  while (read == 63) {
    if (cipher_aux_fread(in, 1, 64, &params->input) != 64)
      eprintf("Truncated input.\n");
    decode_block(in, out, IV, &params->key);
    if (params->pcb)
      params->pcb(cipher_aux_ftell(&params->input), params->input.max);
    for (int i = 0; i < 64; i++) out[i] ^= prev_in[i];
    cipher_aux_fwrite(out, 1, read = out[63], &params->output);
    IV++;
    memcpy(prev_in, in, 64);
  }
}

// ---------------------------------------------------------------------------
//      Command-line stub.
// ---------------------------------------------------------------------------
enum { MODE_ENCODE, MODE_DECODE, MODE_KEYGEN, MODE_RANDOM };

static uint32_t file_size(FILE * f) {
  fseek(f, 0, SEEK_END);
  uint32_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  return size;
}

static void detect_mode_of_operation(FILE * ciphertext,
    stream_enc * e, stream_dec * d) {
  char hdr[6];
  if (fread(hdr, 1, 6, ciphertext) != 6)
    eprintf("Truncated input.\n");
  if (!memcmp(hdr, "KC3CTR", 6))      *e = encode_ctr, *d = decode_ctr;
  else if (!memcmp(hdr, "KC3OFB", 6)) *e = encode_ofb, *d = decode_ofb;
  else eprintf("Input corrupted: unknown mode of operation.\n");
}

static void help(void) {
  fprintf(stdout,
    "kcrypt3 (Sun, 26 Jan 2025) - 3rd iteration of the KCrypt algorithm.\n"
    "Usage: kcrypt3 [-e/d/g/r] [-v/p/h/f/c] [-m mode] [-k key] files...\n"
    "Operations:\n"
    "  -e, --encode        Encode the input file.\n"
    "  -d, --decode        Decode the input file.\n"
    "  -g, --keygen        Generate a new key file.\n"
    "  -r, --random        Generate random data using the key.\n"
    "General options:\n"
    "  -v, --version       Print the version information.\n"
    "  -p, --progress      Show progress information.\n"
    "  -h, --help          Print this help message.\n"
    "  -f, --force         Overwrite existing files.\n"
    "  -c, --stdout        Write output to the standard output.\n"
    "Additional options:\n"
    "  -m, --mode=mode     Set the mode of operation (OFB/CTR).\n"
    "  -k, --key=key       Specify the key file.\n"
    "Written by Kamila Szewczyk (k@iczelia.net).\n"
    "Released to the public domain.\n"
  );
}

static void version(void) {
  fprintf(stdout,
    "kcrypt3 (Sun, 26 Jan 2025) - 3rd iteration of the KCrypt algorithm.\n"
    "Written by Kamila Szewczyk. Released to the public domain.\n"
  );
}

static void progress_callback(uint32_t processed, uint32_t total) {
  if ((processed % 8192) == 0) {
    processed /= 1024; total /= 1024;
    if (total == 0)
      fprintf(stderr, "\rProcessed: %ukB.", processed);
    else
      fprintf(stderr, "\rProcessed: %u/%ukB.", processed, total);
  }
}

static size_t zerodev_read(void * ptr, size_t size,
    size_t nmemb, void * stream) {
  memset(ptr, 0, size * nmemb);
  *((uint32_t *) stream) += size * nmemb;
  return nmemb;
}
static size_t zerodev_write(const void * ptr, size_t size,
    size_t nmemb, void * stream) {
  return nmemb;
}
static uint32_t zerodev_tell(void * stream) {
  return *((uint32_t *) stream);
}

int main(int argc, char * argv[]) {
  gentab(0x1d);
  yarg_options opt[] = {
    // Actions
    { 'e', no_argument, "encode" },
    { 'd', no_argument, "decode" },
    { 'g', no_argument, "genkey" },
    { 'r', no_argument, "random" },
    // General
    { 'v', no_argument, "version" },
    { 'p', no_argument, "progress" },
    { 'h', no_argument, "help" },
    { 'c', no_argument, "stdout" },
    { 'f', no_argument, "force" },
    { 'm', required_argument, "mode" },
    { 'k', required_argument, "key" },
    { 0, 0, 0 }
  };
  yarg_settings settings = {
    .dash_dash = 1, .style = YARG_STYLE_UNIX
  };
  yarg_result * res = yarg_parse(argc, argv, opt, settings);
  if (!res) eprintf("Out of memory.\n");
  if (res->error)
    eprintf("%s\nTry `kcrypt3 --help' for more information.\n", res->error);
  int mode = -1, force = 0, progress = 0, force_stdout = 0;
  stream_enc enc = NULL; stream_dec dec = NULL;
  const char * key_path = NULL;
  for (int i = 0; i < res->argc; i++) {
    switch(res->args[i].opt) {
      case 'e': mode = MODE_ENCODE; break;
      case 'd': mode = MODE_DECODE; break;
      case 'g': mode = MODE_KEYGEN; break;
      case 'r': mode = MODE_RANDOM; break;
      case 'f': force = 1; break;
      case 'h': help(); return 0;
      case 'v': version(); return 0;
      case 'p': progress = 1; break;
      case 'c': force_stdout = 1; break;
      case 'k': key_path = res->args[i].arg; break;
      case 'm':
        for (char * p = res->args[i].arg; *p; p++) *p = tolower(*p);
        if (!strcmp(res->args[i].arg, "ofb"))
          enc = encode_ofb, dec = decode_ofb;
        else if (!strcmp(res->args[i].arg, "ctr"))
          enc = encode_ctr, dec = decode_ctr;
        else
          eprintf("Unknown mode of operation `%s'.\n", res->args[i].arg);
        break;
    }
  }
  if (mode == -1)
    eprintf("No action specified.\n"
            "Try `kcrypt3 --help' for more information.\n");
  #if defined(__MSVCRT__)
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
  #endif
  char * f1 = NULL, * f2 = NULL;
  for (int i = 0; i < res->pos_argc; i++) {
    char * arg = res->pos_args[i];
    if (f1 != NULL && f2 != NULL)
      eprintf("Too many positional arguments.\n");
    if (f1 == NULL) f1 = arg; else f2 = arg;
  }
  char * input = NULL, * output = NULL;
  if (f1 != NULL || f2 != NULL) {
    if (mode == MODE_ENCODE) {
      if (f2 == NULL) {
        input = f1;
        if (!force_stdout) {
          output = malloc(strlen(f1) + 5);
          strcpy(output, f1);
          strcat(output, ".kc3");
        }
      } else { input = f1, output = f2; }
    } else if (mode == MODE_DECODE) {
      if (f2 == NULL) {
        input = f1;
        if(!force_stdout) {
          output = malloc(strlen(f1) + 1);
          strcpy(output, f1);
          if (strlen(f1) > 4 && !strcmp(f1 + strlen(f1) - 4, ".kc3"))
            output[strlen(f1) - 4] = 0;
          else
            eprintf("File `%s' has an unrecognised extension.\n", f1);
        }
      } else { input = f1, output = f2; }
    } else if (mode == MODE_RANDOM) {
      output = f1;
      if (f2 != NULL)
        eprintf("Too many positional arguments.\n");
    } else if (mode == MODE_KEYGEN) {
      if (f1 != NULL || f2 != NULL)
        eprintf("Too many positional arguments.\n");
    }
  }
  FILE * in_file = stdin, * out_file = stdout, * key_file = NULL;
  if (input != NULL) {
    in_file = fopen(input, "rb");
    if (!in_file)
      eprintf("Could not open `%s': %s\n", input, strerror(errno));
  }
  if (key_path != NULL) {
    key_file = fopen(key_path, mode == MODE_KEYGEN ? "wb" : "rb");
    if (!key_file)
      eprintf("Could not open `%s': %s\n", key_path, strerror(errno));
  }
  if (output && !force && access(output, F_OK) == 0)
    eprintf("File `%s' already exists. Use `-f' to overwrite.\n", output);
  if (output != NULL) {
    out_file = fopen(output, "wb");
    if (!out_file)
      eprintf("Could not open `%s': %s\n", output, strerror(errno));
  }
  switch(mode) {
    case MODE_KEYGEN: {
      if (!key_file) eprintf("No key file specified.\n");
      block_key_t k; secrandom(&k, sizeof(k));
      if (fwrite(&k, sizeof(k), 1, key_file) != 1)
        eprintf("Could not write to key file: %s\n", strerror(errno));
      break;
    }
    case MODE_RANDOM: {
      if (!key_file) eprintf("No key file specified.\n");
      if (!enc || !dec)
        eprintf("No mode of operation specified.\n");
      block_key_t k;
      if (fread(&k, sizeof(k), 1, key_file) != 1)
        eprintf("Truncated input.\n");
      uint32_t zero_tell = 0;
      cipher_aux_t zero_device = {
        .type = CIPHER_STREAM_FUNCTION, .max = 0,
        .stream = { zerodev_read, zerodev_write, zerodev_tell, &zero_tell }
      };
      mode_params_t params = {
        .pcb = progress ? progress_callback : NULL,
        .key = k, .input = zero_device, .output = {
          .type = CIPHER_STREAM_FILE, .file = out_file
        }
      };
      enc(&params);
      break;
    }
    case MODE_ENCODE: {
      if (!key_file) eprintf("No key file specified.\n");
      if (!enc || !dec)
        eprintf("No mode of operation specified.\n");
      block_key_t k;
      if (fread(&k, sizeof(k), 1, key_file) != 1)
        eprintf("Truncated input.\n");
      cipher_aux_t input = {
        .type = CIPHER_STREAM_FILE, .file = in_file, .max = file_size(in_file)
      };
      cipher_aux_t output = {
        .type = CIPHER_STREAM_FILE, .file = out_file
      };
      mode_params_t params = {
        .pcb = progress ? progress_callback : NULL,
        .key = k, .input = input, .output = output
      };
      enc(&params);
      break;
    }
    case MODE_DECODE: {
      if (!key_file) eprintf("No key file specified.\n");
      if (enc || dec)
        eprintf("Mode of operation needs not specified for decryption.\n");
      block_key_t k;
      if (fread(&k, sizeof(k), 1, key_file) != 1)
        eprintf("Truncated input.\n");
      cipher_aux_t input = {
        .type = CIPHER_STREAM_FILE, .file = in_file, .max = file_size(in_file)
      };
      cipher_aux_t output = {
        .type = CIPHER_STREAM_FILE, .file = out_file
      };
      mode_params_t params = {
        .pcb = progress ? progress_callback : NULL,
        .key = k, .input = input, .output = output
      };
      detect_mode_of_operation(in_file, &enc, &dec);
      dec(&params);
      break;
    }
  }
  if (input != NULL && fclose(in_file) != 0)
    eprintf("Could not close `%s': %s\n", input, strerror(errno));
  if (output != NULL && fclose(out_file) != 0)
    eprintf("Could not close `%s': %s\n", output, strerror(errno));
}
