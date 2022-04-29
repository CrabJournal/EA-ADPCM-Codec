#include <iostream>
#include <string.h>

#include "..//EA-ADPCM-codec/EA_ADPCM_DLL.h"

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
    uint32_t chunkSize;		        // 0x4		sizeof file - 8
	char format[4];					// 0x8		"WAVE"
	char subchunk1Id[4];			// 0xC		"fmt "
	uint32_t subchunk1Size;	        // 0x10		16
    uint16_t audioFormat;		    // 0x14		1 = PCM
    uint16_t numChannels;		    // 0x16
    uint32_t sampleRate;		    // 0x18
    uint32_t byteRate;			    // 0x1C
    uint16_t blockAlign;		    // 0x20
    uint16_t bitsPerSample;	        // 0x22
	char subchunk2Id[4];			// 0x24		"data" or "smpl"
    uint32_t subchunk2Size;	        // 0x28
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

struct WAV_meta {
    int16_t* PCM;
    size_t n_samples_per_channel;
};

WAV_meta ReadWAV(const char* wav_file, WAVHEADER &wav_header) {
    WAV_meta wavMeta = {nullptr, 0};

    FILE* in_wav_file = fopen(wav_file, "r+b");
    fread(&wav_header, sizeof(wav_header), 1, in_wav_file);
    if (*(uint32_t*)wav_header.subchunk2Id != *(uint32_t*)defaul_wav_header.subchunk2Id) {
        printf("Can not find data signature in wav \n");
        fclose(in_wav_file);
        return wavMeta;
    }
    if (wav_header.bitsPerSample != 16 || wav_header.audioFormat != PCM_WAV) {
        printf("Unknown audio format, use 16 bit PCM wav \n");
        fclose(in_wav_file);
        return wavMeta;
    }
    wavMeta.n_samples_per_channel = wav_header.subchunk2Size / wav_header.numChannels / 2;
    printf("wav info: \n file name: %s \n sample rate = %d Hz \n number of channels = %d \n number of samples per channel = %d \n\n",
           wav_file, wav_header.sampleRate, wav_header.numChannels, wavMeta.n_samples_per_channel);
    wavMeta.PCM = (int16_t*)malloc(wav_header.subchunk2Size);
    fread(wavMeta.PCM, wav_header.subchunk2Size, 1, in_wav_file);
    fclose(in_wav_file);
    return wavMeta;
}
void WriteWAV(const char* wav_file, const WAVHEADER &wav_header, const int16_t* PCM){
    FILE *f = fopen(wav_file, "w+b");
    fwrite(&wav_header, sizeof(wav_header), 1, f);
    fwrite(PCM, wav_header.subchunk2Size, 1, f);
    fclose(f);
}

size_t ReadWholeFile(const char* file_name, void* &data) {
    FILE* f = fopen(file_name, "r+b");
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc(file_size);
    fread(data, file_size, 1, f);
    fclose(f);
	return file_size;
}
void WriteFile(const char* file_name, const void* data, size_t data_size) {
    FILE* f = fopen(file_name, "w+b");
    fwrite(data, data_size, 1, f);
    fclose(f);
}


bool EncodeWav(const char* wav_file, const char *raw_file, WAVHEADER &wav_header) {
    WAV_meta wav_meta = ReadWAV(wav_file, wav_header);

    if (wav_meta.PCM == nullptr) {
        return false;
    }
    uint32_t n_samples_per_channel = wav_meta.n_samples_per_channel;

	size_t encoded_size = GetXASEncodedSize(n_samples_per_channel, wav_header.numChannels);
	void* encoded_data = malloc(encoded_size);
	encode_XAS(encoded_data, wav_meta.PCM, n_samples_per_channel, wav_header.numChannels);
	free(wav_meta.PCM);

    WriteFile(raw_file, encoded_data, encoded_size);

	free(encoded_data);
	return true;
}

bool DecodeRaw(const char* raw_file, const char* wav_file, WAVHEADER &wav_header) {

    void* XAS_data;

    size_t raw_size = ReadWholeFile(raw_file, XAS_data);

	size_t n_chunks = raw_size / 76;
	if (raw_size % 76) {
		printf("uncorrect raw file \n");
		free(XAS_data);
		return false;
	}

	size_t n_total_samples = wav_header.subchunk2Size == 0? n_chunks * 128 : wav_header.subchunk2Size / 2;
	int channels = wav_header.numChannels;
	int16_t* PCM_data = (int16_t*)malloc(sizeof(int16_t) * n_total_samples);

	decode_XAS(XAS_data, PCM_data, n_total_samples / channels, channels);
	free(XAS_data);

	MakeWavHeader(&wav_header, wav_header.sampleRate, n_total_samples, channels);

    WriteWAV(wav_file, wav_header, PCM_data);

	free(PCM_data);
	return true;
}

void Test(int16_t *in, int16_t *out, size_t nSamples) {
	printf("Test starts \n");
	const int max_error = 128;
	for (size_t i = 0; i < nSamples; i++) {
		int error = in[i] - out[i];
		if (abs(error) > max_error) {
			printf("%d - %d \n", i, error);
			out[i] = in[i];
		}
	}
	printf("Test ends \n");
}

// one channel only
void PCM_to_text(const char* txt_file, const int16_t* PCM, size_t nSamples, int nChannels) {
    FILE* stream = fopen(txt_file, "w");
	for (size_t i = 0; i < nSamples; i += nChannels) {
		fprintf(stream, "%d\n", PCM[i]);
	}
    fclose(stream);
}

void RunTheTest(const char* input_file, int repeat) {
	WAVHEADER wav_header;
	static const char raw_file_name[] = "tmp.raw";
	static const char wav_file_name[] = "tmp.wav";

    WAV_meta wav_meta = ReadWAV(input_file, wav_header);
    int16_t* buf2 = (int16_t*)malloc(wav_header.subchunk2Size);
    size_t encoded_size = GetXASEncodedSize(wav_meta.n_samples_per_channel, wav_header.numChannels);
    void* XAS = malloc(encoded_size);

    int nSamples = wav_header.subchunk2Size / 2;
    const int max_samples_to_text = 2'000'000;
    int nSamples_to_text = nSamples >  max_samples_to_text? max_samples_to_text : nSamples;
    PCM_to_text("orig.txt", wav_meta.PCM, nSamples_to_text, wav_header.numChannels);

    int time_rep = 0;
    do {
        encode_XAS(XAS, wav_meta.PCM, wav_meta.n_samples_per_channel, wav_header.numChannels);
        decode_XAS(XAS, buf2, wav_meta.n_samples_per_channel, wav_header.numChannels);
        std::swap(wav_meta.PCM, buf2);
    } while (++time_rep < repeat && memcmp(wav_meta.PCM, buf2, wav_header.subchunk2Size) != 0);
    PCM_to_text("prev.txt", buf2, nSamples_to_text, wav_header.numChannels);

    free(buf2);
    printf("recoded %d times\n", time_rep);

    WriteWAV(wav_file_name, wav_header, wav_meta.PCM);

	PCM_to_text("recoded.txt", wav_meta.PCM, nSamples_to_text, wav_header.numChannels);

}

int _main(int argc, char* argv[]) {

	if ((argc == 4 || argc == 3) && strcmp(argv[1], "test") == 0) {
		RunTheTest(argv[2], argc == 3? 1: atoi(argv[3]) );
		return 0;
	}
	WAVHEADER wav_header;
	if (argc < 4) {
		printf(about);
		return 1;
	}
	if (strcmp(argv[1], "encode") == 0) {
		if (EncodeWav(argv[2], argv[3], wav_header)) {
			printf("encoding completed \n");
		}
	}
	else if (strcmp(argv[1], "decode") == 0) {
		if (argc < 5) {
			printf(about);
			return 2;
		}
		wav_header.sampleRate = atoi(argv[4]);
        wav_header.numChannels = 1;

		if (DecodeRaw(argv[2], argv[3], wav_header)) {
			printf("decoding complited \n");
		}

	}
	else {
		printf("Unknown command \n");
		printf(about);
	}
    return 0;
}


#ifndef countof
#define countof(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#endif

#define _main_(args) _main(countof(args), (char**)args)

FILE* out_f;


int main() {
	static const char *argv1[] = { 
		"", 
		"encode", 
		"D:/games/Need for Speed Carbon/SOUND/1/fx_misc_mb_01.wav",
		"XAS.raw"
	};
	static const char *argv2[] = {
		"",
		"decode",
		"XAS_Chunk.dat",
		"44100",
		"decoded.wav"
	};

	static const char *argv3[] = {
		"",
		"test",
		"FE_MB_or_29.wav",
        "1"
	};

    // out_f = fopen("p-c-out.txt", "w");

    _main_(argv3);
    return 0;
}
