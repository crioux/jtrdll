/*
 * Raw-MD5 (thick) based on Raw-MD4 w/ mmx/sse/intrinsics
 * This software is Copyright (c) 2011 magnum, and it is hereby released to the
 * general public under the following terms:  Redistribution and use in source
 * and binary forms, with or without modification, are permitted.
 *
 * OMP added May 2013, JimF
 */

#include <string.h>

#include "arch.h"

#include "md5.h"
#include "common.h"
#include "formats.h"

#ifdef _OPENMP
#ifdef MMX_COEF_SHA512
#define OMP_SCALE               1024
#else
#define OMP_SCALE				2048
#endif
#include <omp.h>
#endif
#include "sse-intrinsics.h"

#define FORMAT_LABEL			"raw-md5"
#define FORMAT_NAME				"Raw MD5"
#define ALGORITHM_NAME			MD5_ALGORITHM_NAME

#ifdef MD5_SSE_PARA
#  define MMX_COEF				4
#  define NBKEYS				(MMX_COEF * MD5_SSE_PARA)
#  define DO_MMX_MD5(in, out)	SSEmd5body(in, (unsigned int*)out, 1)
#elif MMX_COEF
#  define NBKEYS				MMX_COEF
#  define DO_MMX_MD5(in, out)	mdfivemmx_nosizeupdate(out, in, 1)
// 32bit .S does NOT handle multi-threading. Remove OMP for any build doing that
#  undef _OPENMP
#  undef FMT_OMP
#  define FMT_OMP 0
#endif

#define BENCHMARK_COMMENT		""
#define BENCHMARK_LENGTH		-1
#ifndef MD5_BUF_SIZ
#define MD5_BUF_SIZ				16
#endif

#define CIPHERTEXT_LENGTH		32

#define DIGEST_SIZE				16
#define BINARY_SIZE				16 // source()
#define BINARY_ALIGN			4
#define SALT_SIZE				0
#define SALT_ALIGN				1

#define FORMAT_TAG				"$dynamic_0$"
#define TAG_LENGTH				(sizeof(FORMAT_TAG) - 1)

static struct fmt_tests tests[] = {
	{"5a105e8b9d40e1329780d62ea2265d8a","test1"},
	{FORMAT_TAG "378e2c4a07968da2eca692320136433d","thatsworking"},
	{FORMAT_TAG "8ad8757baa8564dc136c1e07507f4a98","test3"},
	{"d41d8cd98f00b204e9800998ecf8427e", ""},
	{NULL}
};

#ifdef MMX_COEF
#define PLAINTEXT_LENGTH		55
#define MIN_KEYS_PER_CRYPT		NBKEYS
#define MAX_KEYS_PER_CRYPT		NBKEYS
#define GETPOS(i, index)		( (index&(MMX_COEF-1))*4 + ((i)&(0xffffffff-3))*MMX_COEF + ((i)&3) + (index>>(MMX_COEF>>1))*MD5_BUF_SIZ*4*MMX_COEF )
#else
#define PLAINTEXT_LENGTH		125
#define MIN_KEYS_PER_CRYPT		1
#define MAX_KEYS_PER_CRYPT		1
#endif

#ifdef MMX_COEF
static ARCH_WORD_32 (*saved_key)[MD5_BUF_SIZ*NBKEYS];
static ARCH_WORD_32 (*crypt_key)[DIGEST_SIZE/4*NBKEYS];
#else
static int (*saved_key_length);
static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static ARCH_WORD_32 (*crypt_key)[4];
#endif

static void init(struct fmt_main *self)
{
#ifdef _OPENMP
	int omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt = omp_t * MIN_KEYS_PER_CRYPT;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt = omp_t * MAX_KEYS_PER_CRYPT;
#endif
#ifndef MMX_COEF
	saved_key_length = mem_calloc_tiny(sizeof(*saved_key_length) * self->params.max_keys_per_crypt, MEM_ALIGN_WORD);
	saved_key = mem_calloc_tiny(sizeof(*saved_key) * self->params.max_keys_per_crypt, MEM_ALIGN_WORD);
	crypt_key = mem_calloc_tiny(sizeof(*crypt_key) * self->params.max_keys_per_crypt, MEM_ALIGN_WORD);
#else
	saved_key = mem_calloc_tiny(sizeof(*saved_key) * self->params.max_keys_per_crypt/NBKEYS, MEM_ALIGN_SIMD);
	crypt_key = mem_calloc_tiny(sizeof(*crypt_key) * self->params.max_keys_per_crypt/NBKEYS, MEM_ALIGN_SIMD);
#endif
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *p, *q;

	p = ciphertext;
	if (!strncmp(p, FORMAT_TAG, TAG_LENGTH))
		p += TAG_LENGTH;

	q = p;
	while (atoi16[ARCH_INDEX(*q)] != 0x7F) {
		if (*q >= 'A' && *q <= 'F') /* support lowercase only */
			return 0;
		q++;
	}
	return !*q && q - p == CIPHERTEXT_LENGTH;
}

static char *split(char *ciphertext, int index, struct fmt_main *self)
{
	static char out[TAG_LENGTH + CIPHERTEXT_LENGTH + 1];

	if (!strncmp(ciphertext, FORMAT_TAG, TAG_LENGTH))
		return ciphertext;

	memcpy(out, FORMAT_TAG, TAG_LENGTH);
	memcpy(out + TAG_LENGTH, ciphertext, CIPHERTEXT_LENGTH + 1);
	return out;
}

static void *binary(char *ciphertext)
{
	static unsigned char *out;
	char *p;
	int i;

	if (!out) out = mem_alloc_tiny(DIGEST_SIZE, MEM_ALIGN_WORD);

	p = ciphertext + TAG_LENGTH;
	for (i = 0; i < DIGEST_SIZE; i++) {
		out[i] =
		    (atoi16[ARCH_INDEX(*p)] << 4) |
		    atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}

	return out;
}

static int binary_hash_0(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xf; }
static int binary_hash_1(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xff; }
static int binary_hash_2(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xfff; }
static int binary_hash_3(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xffff; }
static int binary_hash_4(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xfffff; }
static int binary_hash_5(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0xffffff; }
static int binary_hash_6(void *binary) { return ((ARCH_WORD_32*)binary)[0] & 0x7ffffff; }

#ifdef MMX_COEF
#define HASH_OFFSET (index&(MMX_COEF-1))+((index%NBKEYS)/MMX_COEF)*MMX_COEF*4
static int get_hash_0(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xf; }
static int get_hash_1(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xff; }
static int get_hash_2(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xfff; }
static int get_hash_3(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xffff; }
static int get_hash_4(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xfffff; }
static int get_hash_5(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0xffffff; }
static int get_hash_6(int index) { return crypt_key[index/NBKEYS][HASH_OFFSET] & 0x7ffffff; }
#else
static int get_hash_0(int index) { return crypt_key[index][0] & 0xf; }
static int get_hash_1(int index) { return crypt_key[index][0] & 0xff; }
static int get_hash_2(int index) { return crypt_key[index][0] & 0xfff; }
static int get_hash_3(int index) { return crypt_key[index][0] & 0xffff; }
static int get_hash_4(int index) { return crypt_key[index][0] & 0xfffff; }
static int get_hash_5(int index) { return crypt_key[index][0] & 0xffffff; }
static int get_hash_6(int index) { return crypt_key[index][0] & 0x7ffffff; }
#endif

#ifdef MMX_COEF
static void set_key(char *_key, int index)
{
	const ARCH_WORD_32 *key = (ARCH_WORD_32*)_key;
	ARCH_WORD_32 *keybuffer = &((ARCH_WORD_32*)saved_key)[(index&(MMX_COEF-1)) + (index>>(MMX_COEF>>1))*MD5_BUF_SIZ*MMX_COEF];
	ARCH_WORD_32 *keybuf_word = keybuffer;
	unsigned int len;
	ARCH_WORD_32 temp;

	len = 0;
	while((temp = *key++) & 0xff) {
		if (!(temp & 0xff00))
		{
			*keybuf_word = (temp & 0xff) | (0x80 << 8);
			len++;
			goto key_cleaning;
		}
		if (!(temp & 0xff0000))
		{
			*keybuf_word = (temp & 0xffff) | (0x80 << 16);
			len+=2;
			goto key_cleaning;
		}
		if (!(temp & 0xff000000))
		{
			*keybuf_word = temp | (0x80 << 24);
			len+=3;
			goto key_cleaning;
		}
		*keybuf_word = temp;
		len += 4;
		keybuf_word += MMX_COEF;
	}
	*keybuf_word = 0x80;

key_cleaning:
	keybuf_word += MMX_COEF;
	while(*keybuf_word) {
		*keybuf_word = 0;
		keybuf_word += MMX_COEF;
	}
	/*
	 * This works for MMX, SSE2 and SSE2i.  Note, for 32 bit MMX/SSE2, we now use
	 * mdfivemmx_nosizeupdate and not mdfivemmx function. Setting the size here,
	 * and calling the 'nosizeupdate' function is about 5% faster, AND it makes the
	 * code much more similar between SSE2i and older 32 bit SSE2
	 */
	keybuffer[14*MMX_COEF] = len << 3;
}
#else
static void set_key(char *key, int index)
{
	int len = strlen(key);
	saved_key_length[index] = len;
	memcpy(saved_key[index], key, len);
}
#endif

#ifdef MMX_COEF
static char *get_key(int index)
{
	static char out[PLAINTEXT_LENGTH + 1];
	unsigned int i;
	ARCH_WORD_32 len = ((ARCH_WORD_32*)saved_key)[14*MMX_COEF + (index&(MMX_COEF-1)) + (index>>(MMX_COEF>>1))*MD5_BUF_SIZ*MMX_COEF] >> 3;

	for(i=0;i<len;i++)
		out[i] = ((char*)saved_key)[GETPOS(i, index)];
	out[i] = 0;
	return (char*)out;
}
#else
static char *get_key(int index)
{
	saved_key[index][saved_key_length[index]] = 0;
	return saved_key[index];
}
#endif

static int crypt_all(int *pcount, struct db_salt *salt)
{
	int count = *pcount;
	int index = 0;

#ifdef MMX_COEF
	count = (count+NBKEYS-1)/NBKEYS;
#endif

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (index = 0; index < count; index++)
	{
#if MMX_COEF
		DO_MMX_MD5(saved_key[index], crypt_key[index]);
#else
		MD5_CTX ctx;
		MD5_Init(&ctx);
		MD5_Update(&ctx, saved_key[index], saved_key_length[index]);
		MD5_Final((unsigned char *)crypt_key[index], &ctx);
#endif
	}
	return *pcount;
}

static int cmp_all(void *binary, int count) {
	int index;
	for (index = 0; index < count; index++)
#ifdef MMX_COEF
        if (((ARCH_WORD_32 *) binary)[0] == ((ARCH_WORD_32*)crypt_key)[(index&(MMX_COEF-1)) + (index>>(MMX_COEF>>1))*4*MMX_COEF])
#else
		if ( ((ARCH_WORD_32*)binary)[0] == crypt_key[index][0] )
#endif
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
#ifdef MMX_COEF
    int i;
	for (i=1; i < BINARY_SIZE/sizeof(ARCH_WORD_32); i++)
        if (((ARCH_WORD_32 *) binary)[i] != ((ARCH_WORD_32*)crypt_key)[(index&(MMX_COEF-1)) + (index>>(MMX_COEF>>1))*4*MMX_COEF+i*MMX_COEF])
            return 0;
	return 1;
#else
	return !memcmp(binary, crypt_key[index], BINARY_SIZE);
#endif
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

static char *source(char *source, void *binary)
{
	static char Buf[CIPHERTEXT_LENGTH + TAG_LENGTH + 1];
	unsigned char *cpi;
	char *cpo;
	int i;

	strcpy(Buf, FORMAT_TAG);
	cpo = &Buf[TAG_LENGTH];

	cpi = (unsigned char*)(binary);

	for (i = 0; i < BINARY_SIZE; ++i) {
		*cpo++ = itoa16[(*cpi)>>4];
		*cpo++ = itoa16[*cpi&0xF];
		++cpi;
	}
	*cpo = 0;
	return Buf;
}

struct fmt_main fmt_rawMD5 = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_OMP,
		tests
	}, {
		init,
		fmt_default_done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		split,
		binary,
		fmt_default_salt,
		source,
		{
			binary_hash_0,
			binary_hash_1,
			binary_hash_2,
			binary_hash_3,
			binary_hash_4,
			binary_hash_5,
			binary_hash_6
		},
		fmt_default_salt_hash,
		fmt_default_set_salt,
		set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			get_hash_0,
			get_hash_1,
			get_hash_2,
			get_hash_3,
			get_hash_4,
			get_hash_5,
			get_hash_6
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};