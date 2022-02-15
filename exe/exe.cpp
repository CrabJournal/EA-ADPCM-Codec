#include "pch.h"
#include <iostream>

#include "..//EA ADPCM codec/EA_ADPCM_DLL.h"

const char about[] = {
	"EA ADPCM codec \n"
	"Current version supports only XAS encoding/decoding \n"
	"Usage:\n" 
	"encoding: encode [input wav file] [output raw file] \n" // only wav encoding supports now
	"decoding: decode [input raw file] [output wav file] [sample rate] \n"
	"decoded sounds assumed to be single channel, 128 samples aligned \n"
};

#define PCM_WAV 1

struct WAVHEADER {
	char chunkId[4];				// 0x0		"RIFF"
	unsigned long chunkSize;		// 0x4		sizeof file - 8
	char format[4];					// 0x8		"WAVE"
	char subchunk1Id[4];			// 0xC		"fmt "
	unsigned long subchunk1Size;	// 0x10		16
	unsigned short audioFormat;		// 0x14		1 = PCM
	unsigned short numChannels;		// 0x16		
	unsigned long sampleRate;		// 0x18		
	unsigned long byteRate;			// 0x1C		
	unsigned short blockAlign;		// 0x20		
	unsigned short bitsPerSample;	// 0x22		
	char subchunk2Id[4];			// 0x24		"data" or "smpl"
	unsigned long subchunk2Size;	// 0x28
	// wav data						// 0x2C
};

const WAVHEADER defaul_wav_header{
	{'R', 'I', 'F', 'F'},
	0,						// !
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	PCM_WAV,
	1,						// .
	0,						// !
	0,						// !
	2,						// .
	16,
	{'d', 'a', 't', 'a'},
	0						// !
};

void MakeWavHeader(WAVHEADER *wav_header, int sample_rate, int num_samples, int num_channels) {
	memcpy(wav_header, &defaul_wav_header, sizeof(WAVHEADER));
	int block_align = num_channels * 2;
	size_t data_size = num_samples * 2;
	wav_header->chunkSize = data_size + sizeof(WAVHEADER) - 8;
	wav_header->numChannels = num_channels;
	wav_header->sampleRate = sample_rate;
	wav_header->byteRate = sample_rate* block_align;
	wav_header->blockAlign = block_align;
	wav_header->subchunk2Size = data_size;
}

void main(int argc, char* argv[]) {
	if (argc < 4) {
		printf(about);
		return;
	}
	WAVHEADER wav_header;
	if (strcmp(argv[1], "encode") == 0) {
		FILE* in_wav_file = fopen(argv[2], "r+b");
		fread(&wav_header, sizeof(wav_header), 1, in_wav_file);
		if (*(uint32_t*)wav_header.subchunk2Id != *(uint32_t*)defaul_wav_header.subchunk2Id) {
			printf("Can not find data signature in wav \n");
			fclose(in_wav_file);
			return;
		}
		if (wav_header.bitsPerSample != 16 || wav_header.audioFormat != PCM_WAV) {
			printf("Unknown audio format, use 16 bit PCM wav \n");
			fclose(in_wav_file);
			return;
		}
		uint32_t n_samples_per_channel = wav_header.subchunk2Size / wav_header.numChannels / 2;
		printf("wav info: \n sample rate = %d Hz \n number of channels = %d \n number of samples per channel = %d \n\n",
			wav_header.sampleRate, wav_header.numChannels, n_samples_per_channel);
		int16_t* PCM_data = (int16_t*)malloc(wav_header.subchunk2Size);
		fread(PCM_data, wav_header.subchunk2Size, 1, in_wav_file);
		fclose(in_wav_file);

		size_t encoded_size = GetXASEncodedSize(n_samples_per_channel, wav_header.numChannels);
		void* encoded_data = malloc(encoded_size);
		encode_XAS(encoded_data, PCM_data, n_samples_per_channel, wav_header.numChannels);
		free(PCM_data);

		FILE* out_raw_file = fopen(argv[3], "w+b");
		fwrite(encoded_data, encoded_size, 1, out_raw_file);
		free(encoded_data);
		fclose(out_raw_file);
		printf("encoding complited \n");
	}
	else if (strcmp(argv[1], "decode") == 0) {
		if (argc < 5) {
			printf(about);
			return;
		}
		FILE* in_raw_file = fopen(argv[2], "r+b");

		fseek(in_raw_file, 0, SEEK_END);
		size_t raw_size = ftell(in_raw_file);
		fseek(in_raw_file, 0, SEEK_SET);

		size_t n_chunks = raw_size / 76;
		if (raw_size % 76) {
			printf("uncorrect raw file \n");
			fclose(in_raw_file);
			return;
		}

		void* XAS_data = malloc(raw_size);
		fread(XAS_data, raw_size, 1, in_raw_file);
		fclose(in_raw_file);

		size_t n_total_samples = n_chunks * 128;
		const int channels = 1;
		int16_t* PCM_data = (int16_t*)malloc(sizeof(int16_t) * n_total_samples);

		decode_XAS(XAS_data, PCM_data, n_total_samples / channels, channels);
		free(XAS_data);

		MakeWavHeader(&wav_header, atoi(argv[4]), n_total_samples, channels);

		FILE* out_wav = fopen(argv[3], "w+b");

		fwrite(&wav_header, sizeof(wav_header), 1, out_wav);
		fwrite(PCM_data, sizeof(int16_t) * n_total_samples, 1, out_wav);
		fclose(out_wav);
		free(PCM_data);

		printf("decoding complited \n");
	}
	else {
		printf("Unknown command \n");
		printf(about);
	}
}

/*
#define countof(ARR) (sizeof(ARR) / sizeof(ARR[0]))
void main() {
	static const char *argv1[] = { 
		"", 
		"encode", 
		"D:/games/Need for Speed Carbon/SOUND/1/fx_misc_mb_01.wav",
		"XAS.raw"
	};
	static const char *argv2[] = {
		"",
		"decode",
		"XAS.raw",
		"44100",
		"decoded.wav"
	};

	_main(countof(argv2), (char**)argv2);
}
*/