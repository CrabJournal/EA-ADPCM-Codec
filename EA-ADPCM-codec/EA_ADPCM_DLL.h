#pragma once
#include <stdint.h>

// #define _DEBUG

#ifdef _MSC_VER

#ifdef EAADPCMCODEC_EXPORTS
#define EAADPCMCODEC_API extern "C" __declspec(dllexport)
#else
#define EAADPCMCODEC_API extern "C" __declspec(dllimport)
#endif
#define CODEC_ABI __cdecl

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
