#pragma once

#ifndef __SSE2__

#if (_M_IX86_FP >= 2) || defined(_M_X64) || defined(_M_AMD64)
#define __SSE2__
#endif

#endif // !__SSE2__


#ifdef __SSE2__

#define __OPTIMIZE__

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
// #include <xmmintrin.h>

struct vec128;
inline vec128 GetOnes128();

struct vec128 {
    static vec128 GetZeros128() {
        return { _mm_setzero_si128() };
    }

    template <typename T>
    inline T SIMD_reinterpret_cast(){
        return { i128 };
    }
    inline vec128 operator&(vec128 b) {
        return { _mm_and_si128(i128, b.i128) };
    }
    inline vec128 operator|(vec128 b) {
        return { _mm_or_si128(i128, b.i128) };
    }
    inline vec128 operator^(vec128 b) {
        return { _mm_xor_si128(i128, b.i128) };
    }
    inline vec128 operator~() {
        return { (*this)^GetOnes128() };
    }

public:
    __m128i i128;
};

inline vec128 GetOnes128() {
    __m128i undef = _mm_undefined_si128();
    return { _mm_cmpeq_epi32(undef, undef) };
}

inline vec128 GetZeros128(){
    return { _mm_setzero_si128() };
}

struct uint32x4_t;

struct int32x4_t : vec128 {};
struct uint32x4_t : int32x4_t {};
struct int16x8_t : vec128 {};
struct uint16x8_t : vec128 {
    inline operator int32x4_t() const { return {_mm_cvtepu16_epi32(i128)}; }
};
struct uint8x16_t : vec128 {
    inline operator int32x4_t() const { return {_mm_cvtepu8_epi32(i128)}; }
};

inline int16x8_t operator<<(int16x8_t a, const int shift_imm8) {
	return { _mm_slli_epi16(a.i128, shift_imm8) };
}
inline int16x8_t operator<<(int16x8_t a, int16x8_t shift) {
	return { _mm_sll_epi16(a.i128, shift.i128) };
}
inline int32x4_t operator<<(int32x4_t a, const int shift_imm8) {
	return { _mm_slli_epi32(a.i128, shift_imm8) };
}
inline int32x4_t operator<<(int32x4_t a, int32x4_t shift) {
	return { _mm_sll_epi32(a.i128, shift.i128) };
}
inline int16x8_t operator>>(int16x8_t a, const int shift_imm8) {
	return { _mm_srli_epi16(a.i128, shift_imm8) };
}
inline int16x8_t operator>>(int16x8_t a, int16x8_t shift) {
	return { _mm_sra_epi16(a.i128, shift.i128) };
}
inline int32x4_t operator>>(int32x4_t a, const int shift_imm8) {
	return { _mm_srai_epi32(a.i128, shift_imm8) };
}
inline uint32x4_t operator>>(uint32x4_t a, const int shift_imm8) {
    return { _mm_srli_epi32(a.i128, shift_imm8) };
}
inline int32x4_t operator>>(int32x4_t a, int32x4_t shift) {
	return { _mm_sra_epi32(a.i128, shift.i128) };
}


inline int32x4_t operator+(int32x4_t a, int32x4_t b) {
	return { _mm_add_epi32(a.i128, b.i128) };
}
inline int16x8_t operator+(int16x8_t a, int16x8_t b) {
	return { _mm_add_epi16(a.i128, b.i128) };
}
inline int32x4_t operator-(int32x4_t a, int32x4_t b) {
	return { _mm_sub_epi32(a.i128, b.i128) };
}
inline int16x8_t operator-(int16x8_t a, int16x8_t b) {
	return { _mm_sub_epi16(a.i128, b.i128) };
}

inline int32x4_t mul16_add32(int16x8_t a, int16x8_t b) {
	return { _mm_madd_epi16(a.i128, b.i128) };
}

inline int16x8_t Clip_int16(int32x4_t a) {
	return {  _mm_packs_epi32(a.i128, a.i128)  };
}

#ifdef __AVX__

inline int32x4_t LoadByIndex(int32x4_t indexes, const int* mem) {
    return { _mm_castps_si128(_mm_permutevar_ps(*(__m128*)mem, indexes.i128)) };
}

#else
inline int32x4_t LoadByIndex(int32x4_t indexes, const int* mem) {
    __m128i tmp = _mm_undefined_si128();
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 0)], 0);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 1)], 1);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 2)], 2);
    tmp = _mm_insert_epi32(tmp, mem[_mm_extract_epi32(indexes.i128, 3)], 3);
    return { tmp };
}
#endif // __AVX__

inline void SaveWithStep(int32x4_t vect, int32_t * mem, int step) {
    mem[0]      = _mm_extract_epi32(vect.i128, 0);
    mem[step]   = _mm_extract_epi32(vect.i128, 1);
    mem[step*2] = _mm_extract_epi32(vect.i128, 2);
    mem[step*3] = _mm_extract_epi32(vect.i128, 3);
}

inline void SaveWithStep_low_4(int16x8_t vect, int16_t* mem, int step) {
    mem[0]      = _mm_extract_epi16(vect.i128, 0);
    mem[step]   = _mm_extract_epi16(vect.i128, 1);
    mem[step*2] = _mm_extract_epi16(vect.i128, 2);
    mem[step*3] = _mm_extract_epi16(vect.i128, 3);
}

#endif // __SSE__