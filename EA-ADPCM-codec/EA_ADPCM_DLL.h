#pragma once
#include <stdint.h>

// #define _DEBUG

#ifdef _MSC_VER

#ifdef EAADPCMCODEC_EXPORTS
#define EAADPCMCODEC_API extern "C" __declspec(dllexport)
#else
#define EAADPCMCODEC_API extern "C" __declspec(dllimport)
#endif
#define CODEC_ABI //__vectorcall

#else // _MSC_VER

#define EAADPCMCODEC_API
#define CODEC_ABI

#endif

EAADPCMCODEC_API
uint32_t CODEC_ABI GetXASEncodedSize(uint32_t n_samples_per_channel, uint32_t n_channels);

EAADPCMCODEC_API
void CODEC_ABI decode_XAS(const void* in_XAS, int16_t* out_PCM, uint32_t n_samples_per_channel, uint32_t n_channels);

EAADPCMCODEC_API
void CODEC_ABI encode_XAS(void* out_XAS, const int16_t* in_PCM, uint32_t n_samples_per_channel, uint32_t n_channels);

EAADPCMCODEC_API
void CODEC_ABI decode_EA_XA_R2(const void* data, int16_t *out_PCM, uint32_t n_samples_per_channel, uint32_t n_channels);

EAADPCMCODEC_API
size_t CODEC_ABI encode_EA_XA_R2(void* data, const int16_t PCM[], uint32_t n_samples_per_channel, uint32_t n_channels, int16_t max_error = 10);


// #define BENCH

#ifdef BENCH

EAADPCMCODEC_API
#ifdef _MSC_VER
void _cdecl Bench(uint32_t reps);
#endif
void CODEC_ABI Bench(uint32_t reps);

#endif // !BENCH