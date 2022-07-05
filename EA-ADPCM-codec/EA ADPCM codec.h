#pragma once

#include <string.h>
#include <iostream>

typedef unsigned char byte;

const int fixed_point_offset = 8;
const int fixp_exponent = 1 << fixed_point_offset;
/*
	S0 - prev-prev sample
	S1 - prev sample
	S2 - curr sample prediction
	dS = S1 - S0
*/

typedef int16_t table_type;
const table_type ea_adpcm_table_v2[][2] = {
	{(table_type)(0.000000*fixp_exponent), (table_type)( 0.000000*fixp_exponent)},	// S2  = 0										silent,			also will used for high freq sound
	{(table_type)(0.937500*fixp_exponent), (table_type)( 0.000000*fixp_exponent)},	// S2 ~= 0.94*S1								slight fading,	also will used for high freq noise
	{(table_type)(1.796875*fixp_exponent), (table_type)(-0.812500*fixp_exponent)},	// S2 ~= S1 + dS*0.8		= S0 + dS*1.8		follow trend,	commonly used for low freq sound with high sample rate
	{(table_type)(1.531250*fixp_exponent), (table_type)(-0.859375*fixp_exponent)},	// S2 ~= 0.67*S1 + 0.86*dS	= 0.67*S0 + dS*1.53
};


#ifdef __GNUC__

#if defined(_M_AMD64) || defined(__i386__)
#include <x86intrin.h>
#define ToBigEndian16 __builtin_bswap16
#else
#include <netinet/in.h>
inline int16_t ToBigEndian16(int16_t val){
	return htons(val); // is it fast enough?
}
#endif

#else

#ifndef _MSC_VER
#define ToBigEndian16 _byteswap_ushort
#endif // !_MSC_VER


#endif

const int subchunks_in_XAS_chunk = 4;
const int samples_in_XAS_subchunk = 30;
const int samples_in_XAS_header = 2;
const int samples_in_XAS_per_subchunk = samples_in_XAS_subchunk + samples_in_XAS_header; // but not IN subchunk

const int samples_in_EA_XA_R_chunk = 28;
const int sizeof_EA_XA_R1_chunk = 1 + 2*sizeof(int16_t) + samples_in_EA_XA_R_chunk / 2;
const int sizeof_uncompr_EA_XA_R23_block = 1 + (samples_in_EA_XA_R_chunk + 2) * sizeof(int16_t);
const int sizeof_compr_EA_XA_R23_block = 1 + samples_in_EA_XA_R_chunk / 2;

#pragma pack(push, 1)

// for x86, x64 MSVC!

// size 4 bytes, 2 samples
// Little Endian
struct XAS_SubChunkHeader {
	unsigned coef_index : 2;	// index for table for coeficien
	unsigned unused : 2;        // must be 0
	signed sample_0 : 12;
	unsigned exp_shift : 4;	    // shift right, bits
	signed sample_1 : 12;
};

// MSVC thinking it's 4 bytes xD
struct SamplesByte {
	signed sample1 : 4;
	signed sample0 : 4;
};

struct SamplesDWORD {
	int16_t samples[2];
};

// size 76 bytes, 128 samples
struct XAS_Chunk {
	XAS_SubChunkHeader headers[subchunks_in_XAS_chunk];	// total size 16 bytes, 8 samples
	byte XAS_data[15][subchunks_in_XAS_chunk];	// data for each 2 samples (1 bytes) interleaved, total size 60 bytes, 120 samples
};
#pragma pack(pop, 1)

#ifdef _MSC_VER

#include <intrin.h>

#define _memcpy(DST, SRC, _SIZE) __movsb((byte*)DST, (byte*)SRC, _SIZE)
#define _memset(DST, VAL, _SIZE) __stosb((byte*)DST, (byte)VAL, _SIZE)

#else // _MSC_VER

#define _memcpy memcpy
#define _memset memset

#endif


inline int16_t Get_s16be(const void* ptr) {
	return (short)ToBigEndian16(*(unsigned short*)ptr);
}
inline int16_t bytestream2_get_le16s(byte** ptr) {
	short val = **(short**)ptr;
	*ptr += 2;
	return val;
}
inline char bytestream2_get_bytes(byte** ptr) {
	char val = **(char**)ptr;
	*ptr += 1;
	return val;
}
inline char low_sNibble(char _byte) {
	return (char)((byte)_byte << 4) >> 4;
}
inline int16_t Clip_int16(int val) {
#ifdef __SSE2__
	return _mm_cvtsi128_si32(_mm_packs_epi32(_mm_cvtsi32_si128(val), _mm_undefined_si128()));
#else
	return (val >= 0x7FFF) ? 0x7FFF : (val <= -0x8000) ? -0x8000 : val;
#endif // __SSE2__
}
inline char Clip_int4(char val) {
	if (val >= 7) return 7;
	if (val <= -8) return -8;
	return val;
}
inline int Clip_fix_p16(int val) {
	if (val >= (0x7FFF << fixed_point_offset)) return 0x7FFF << fixed_point_offset;
	if (val <= (-0x8000 << fixed_point_offset)) return -0x8000 << fixed_point_offset;
	return val;
}
