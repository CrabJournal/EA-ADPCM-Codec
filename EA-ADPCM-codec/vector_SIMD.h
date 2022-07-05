#pragma once

#ifdef _MSC_VER
#define inline __forceinline
#define _decl _vectorcall
#else
#define _decl
#endif


#include "stdint.h"

#ifndef __SSE2__

#if (_M_IX86_FP >= 2) || defined(_M_X64) || defined(_M_AMD64)
#define __SSE2__
#endif

#endif // !__SSE2__

#ifdef __SSE2__

// #define __OPTIMIZE__

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
// #include <xmmintrin.h>

struct vec128;
inline vec128 GetOnes128();


struct vec128 {
    template <typename T>
    inline T _decl SIMD_reinterpret_cast(){
        return { i128 };
    }
    inline vec128 _decl operator&(vec128 b) {
        return { _mm_and_si128(i128, b.i128) };
    }
    inline vec128 _decl operator|(vec128 b) {
        return { _mm_or_si128(i128, b.i128) };
    }
    inline vec128 _decl operator^(vec128 b) {
        return { _mm_xor_si128(i128, b.i128) };
    }
    inline vec128 _decl operator~() {
        return { (*this)^GetOnes128() };
    }

public:
    __m128i i128;
};

inline vec128 LoadUnaligned(const void* ptr){
    return {_mm_loadu_si128((__m128i*)ptr)};
}

inline vec128 GetOnes128() {
    __m128i undef = _mm_undefined_si128();
    return { _mm_cmpeq_epi32(undef, undef) };
}

inline vec128 GetZeros128(){
    return { _mm_setzero_si128() };
}

struct uint32x4;

struct int32x4 : vec128 {};
struct uint32x4 : int32x4 {};
struct int16x8 : vec128 {};
struct uint16x8 : vec128 {
    inline _decl operator int32x4() const { return {_mm_cvtepu16_epi32(i128)}; }
};
struct uint8x16_t : vec128 {
    inline _decl operator int32x4 () const { return {_mm_cvtepu8_epi32(i128)}; }
};

inline int16x8 operator<<(int16x8 a, const int shift_imm8) {
	return { _mm_slli_epi16(a.i128, shift_imm8) };
}
inline int32x4 operator<<(int32x4 a, const int shift_imm8) {
	return { _mm_slli_epi32(a.i128, shift_imm8) };
}

inline int16x8 operator>>(int16x8 a, const int shift_imm8) {
	return { _mm_srli_epi16(a.i128, shift_imm8) };
}
inline int32x4 operator>>(int32x4 a, const int shift_imm8) {
	return { _mm_srai_epi32(a.i128, shift_imm8) };
}
inline uint32x4 operator>>(uint32x4 a, const int shift_imm8) {
    return { _mm_srli_epi32(a.i128, shift_imm8) };
}

#ifdef __AVX2__

inline int32x4 operator>>(int32x4 a, int32x4 shift) {
	return { _mm_srav_epi32(a.i128, shift.i128) };
}

#else

#define RA_SHIFT_ELEMENT(RES, VAL, SHIFT, NUM_EL) RES = _mm_insert_epi32(RES, _mm_extract_epi32(VAL, NUM_EL) >> _mm_extract_epi32(SHIFT, NUM_EL), NUM_EL)
inline int32x4 operator>>(int32x4 a, int32x4 shift) {
	__m128i _val = a.i128;
	__m128i _sh = shift.i128;
	
	__m128i res = _mm_cvtsi32_si128(_mm_cvtsi128_si32(_val) >> _mm_cvtsi128_si32(_sh));

	RA_SHIFT_ELEMENT(res, _val, _sh, 1);
	RA_SHIFT_ELEMENT(res, _val, _sh, 2);
	RA_SHIFT_ELEMENT(res, _val, _sh, 3);

	return { res };
}
#undef SHIFT_ELEMENT

#endif

inline int32x4 operator+(int32x4 a, int32x4 b) {
	return { _mm_add_epi32(a.i128, b.i128) };
}
inline int16x8 operator+(int16x8 a, int16x8 b) {
	return { _mm_add_epi16(a.i128, b.i128) };
}
inline int32x4 operator-(int32x4 a, int32x4 b) {
	return { _mm_sub_epi32(a.i128, b.i128) };
}
inline int16x8 operator-(int16x8 a, int16x8 b) {
	return { _mm_sub_epi16(a.i128, b.i128) };
}

inline int32x4 mul16_add32(int16x8 a, int16x8 b) {
	return { _mm_madd_epi16(a.i128, b.i128) };
}

inline int16x8 Clip_int16(int32x4 a) {
	return {  _mm_packs_epi32(a.i128, a.i128)  };
}

#ifdef __AVX__

inline int32x4 LoadByIndex(int32x4 indexes, const int* mem) {
    return { _mm_castps_si128(_mm_permutevar_ps(*(__m128*)mem, indexes.i128)) };
}

#else
inline int32x4 LoadByIndex(int32x4 indexes, const int* mem) {
    __m128i tmp;
    tmp = _mm_cvtsi32_si128(mem[_mm_extract_epi32(indexes.i128, 0)]);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 1)], 1);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 2)], 2);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 3)], 3);
    return { tmp };
}
#endif // __AVX__

inline void SaveWithStep(int32x4 vect, int32_t * mem, int step) {
    mem[0]      = _mm_extract_epi32(vect.i128, 0);
    mem[step]   = _mm_extract_epi32(vect.i128, 1);
    mem[step*2] = _mm_extract_epi32(vect.i128, 2);
    mem[step*3] = _mm_extract_epi32(vect.i128, 3);
}

inline void SaveWithStep_low_4(int16x8 vect, int16_t* mem, int step) {
    *mem = _mm_extract_epi16(vect.i128, 0), mem += step;
	*mem = _mm_extract_epi16(vect.i128, 1), mem += step;
	*mem = _mm_extract_epi16(vect.i128, 2), mem += step;
	*mem = _mm_extract_epi16(vect.i128, 3);
}

inline vec128 PermuteByIndex(vec128 vect, vec128 index) {
	return { _mm_shuffle_epi8(vect.i128, index.i128) };
}

#endif // __SSE2__


#undef inline
#undef _decl


#ifdef __ARM_NEON

#include <arm_neon.h>

struct vec128;

// could not overload typedef'ed :(
struct vec128 {
    template <typename T>
    inline T SIMD_reinterpret_cast(){
        return { i128 };
    }
    inline vec128 operator&(vec128 b) {
        return { vandq_s8(i128, b.i128) };
    }
    inline vec128 operator|(vec128 b) {
        return { vorrq_s8(i128, b.i128) };
    }
    inline vec128 operator^(vec128 b) {
        return { veorq_s8(i128, b.i128) };
    }
    inline vec128 operator~() {
        return { vmvnq_s8(i128) };
    }
public:
    int8x16_t i128;
};
struct int16x8 : vec128 { };
struct int32x4 : vec128 { };
struct uint32x4 : int32x4 { };

vec128

inline int16x8 operator<<(int16x8 a, const int shift_imm8) {
	return { vshlq_n_s16( vreinterpretq_s16_s8(a.i128), shift_imm8) };
}
inline int32x4 operator<<(int32x4 a, const int shift_imm8) {
	return { vshlq_n_s32( vreinterpretq_s32_s8(a.i128), shift_imm8) };
}
inline int16x8 operator>>(int16x8 a, const int shift_imm8) {
	return { vshrq_n_s16( vreinterpretq_s16_s8(a.i128), shift_imm8) };
}
inline int32x4 operator>>(int32x4 a, const int shift_imm8) {
	return { vshrq_n_s32( vreinterpretq_s32_s8(a.i128), shift_imm8) };
}
inline uint32x4 operator>>(uint32x4 a, const int shift_imm8) {    return { vshrq_n_u32( vreinterpretq_u32_s8(a.i128), shift_imm8) };
}
inline int32x4 operator>>(int32x4 a, int32x4 shift) {
	return { vshlq_s32( vreinterpretq_s16_s8(a.i128), vnegq_s32( vreinterpretq_s32_s8(shift.i128) )) }; // neg shift and make sihned shift left then
}
inline int32x4 operator+(int32x4 a, int32x4 b) {
	return { vaddq_s32(a.i128, b.i128) }; // neg shift and make sihned shift left then
}


inline void SaveWithStep(int32x4 vect, int32_t * mem, int step) {
    mem[0]      = _mm_extract_epi32(vect.i128, 0);
    mem[step]   = _mm_extract_epi32(vect.i128, 1);
    mem[step*2] = _mm_extract_epi32(vect.i128, 2);
    mem[step*3] = _mm_extract_epi32(vect.i128, 3);
}

LoadUnaligned(){

}

#endif