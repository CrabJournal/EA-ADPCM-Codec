#include "limits.h"

#include "vector_SIMD.h"

#include "EA ADPCM codec.h"
#include "EA_ADPCM_DLL.h"

#include <cassert>

#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

/*
	bias_compens
	EA-XA R2: not present in ffmpeg and SX, but present in NFS_abk_decode
	EA-XAS: presents in all known decoders (ffmpeg, NFS_abk_decoder(Carbon+))
*/
const int def_rounding = (fixp_exponent >> 1);

inline int16_t decode_XA_sample(const int16_t prev_samples[2], const table_type coef[2], char int4, byte shift) {
	int correction = (int)int4 << shift;
	int prediction = prev_samples[1] * coef[0] + prev_samples[0] * coef[1];
	return Clip_int16((prediction + correction + def_rounding) >> fixed_point_offset);
}

struct EncodedSample {
	int16_t decoded;
	char encoded;
};

inline EncodedSample encode_XA_sample(const int16_t prev_samples[2], const table_type coef[2], int sample, byte shift) {
	int prediction = prev_samples[1] * coef[0] + prev_samples[0] * coef[1];

	int correction = (sample << fixed_point_offset) - prediction;

#ifdef _DEBUG__
    int shifted = correction >> shift;
				const int tr = 8;
				if (shifted > tr || shifted < -(tr + 1)) {
					shift = out_chunk->headers[j].exp_shift - 1;
#ifdef _DEBUG
					printf("patch used for sample %d\n", pInSamples - in_PCM);
#endif
					// goto patch;
				}
#endif

    int res;
    int rounding = 1 << (shift - 1);
    res = Clip_int4((correction + rounding) >> shift);

	int predecoded = ((res << shift) + prediction + def_rounding) >> fixed_point_offset;
	int decoded = Clip_int16(predecoded);

	// ---- for better precision on clipping or near-clipping, this can be removed
	int term = 1 << (shift - fixed_point_offset); // it's like +-1 to res until >> fixed_point_offset
	int decoded2;
    decoded2 = Clip_int16(predecoded + term);
	if (res != 7 && abs(decoded - sample) > abs(decoded2 - sample)) {
		res += 1;
		decoded = decoded2;
	}
	else {
        decoded2 = Clip_int16(predecoded - term);
        if (res != -8 && abs(decoded - sample) > abs(decoded2 - sample)) {
            res -= 1;
            decoded = decoded2;
        }
    }
    // ----
	
	return { (int16_t)decoded, (char)res};
}

#define _GetNumXASChunks(N_SAMPLES) ((N_SAMPLES + 127) / 128)
// mb export?
uint32_t GetNumXASTotalChunks(uint32_t n_samples_per_channel, uint32_t n_channels) {
	return n_channels * _GetNumXASChunks(n_samples_per_channel);
}

EAADPCMCODEC_API
uint32_t GetXASEncodedSize(uint32_t n_samples_per_channel, uint32_t n_channels) {
	return GetNumXASTotalChunks(n_samples_per_channel, n_channels)*sizeof(XAS_Chunk);
}

void decode_XAS_Chunk(const XAS_Chunk* in_chunk, int16_t* out_PCM) {
	for (int j = 0; j < subchunks_in_XAS_chunk; j++) {
		int16_t *pSamples = out_PCM + j * 32;

		pSamples[0] = (in_chunk->headers[j].sample_0 << 4);
		int coef_index = in_chunk->headers[j].coef_index;

		pSamples[1] = (in_chunk->headers[j].sample_1 << 4);
		byte shift = 12 + fixed_point_offset - in_chunk->headers[j].exp_shift;

		const table_type* coef = ea_adpcm_table_v2[coef_index];

		for (int i = 0; i < 15; i++, pSamples += 2) {
			SamplesByte data = *(SamplesByte*)&(in_chunk->XAS_data[i][j]);

			pSamples[2] = decode_XA_sample(pSamples, coef, data.sample0, shift);
			pSamples[3] = decode_XA_sample(pSamples + 1, coef, data.sample1, shift);

#ifdef _DEBUG
			EncodedSample enc0 = encode_XA_sample(pSamples, coef, pSamples[2], shift);
			EncodedSample enc1 = encode_XA_sample(pSamples + 1, coef, pSamples[3], shift);
			if (enc0.encoded != data.sample0 || enc1.encoded != data.sample1) {
				printf(__FUNCTION__ " subchunk %d, byte %d: ", j, i);
				if (enc0.decoded != pSamples[2] || enc0.decoded != pSamples[3]) {
					printf("reencode error \n");
				}
				else {
					printf("loseless reencoding unequality \n");
				}
			}
#endif // _DEBUG
		}
	}
}

#ifdef __GNUC__
#define ALIGN(ALGN) __attribute__((aligned (ALGN)))
#else
#define ALIGN(ALGN)
#endif

void decode_XAS_Chunk_SIMD(const XAS_Chunk* in_chunk, int16_t* out_PCM) {
	
    vec128 head = LoadUnaligned(in_chunk->headers);
	static const table_type ea_adpcm_table_v3[][2] ALIGN(16) = {
		{(table_type)(0.000000*fixp_exponent), (table_type)(0.000000*fixp_exponent)},
		{(table_type)(0.000000*fixp_exponent), (table_type)(0.937500*fixp_exponent)},
		{(table_type)(-0.812500*fixp_exponent), (table_type)(1.796875*fixp_exponent)},
		{(table_type)(-0.859375*fixp_exponent), (table_type)(1.531250*fixp_exponent)},
	};
	static const int32_t const_shift[4] ALIGN(16) = {16 - fixed_point_offset, 16 - fixed_point_offset , 16 - fixed_point_offset , 16 - fixed_point_offset };
	static const uint8_t shuffle[16] ALIGN(16) = {12, 8, 4, 0,   13, 9, 5, 1,   14, 10, 6, 2,    15, 11, 7, 3};

	uint32x4 rounding = { GetOnes128() };

	uint32x4 coef_mask = rounding >> 30;
	int32x4 nibble_mask = rounding << 28;

	rounding = (rounding >> 31 << (fixed_point_offset - 1)).SIMD_reinterpret_cast<uint32x4>();

	int16x8 samples = head.SIMD_reinterpret_cast<int16x8>();
	samples = samples >> 4 << 4;

	int32x4 shift = { head };
	shift = *(int32x4*)const_shift + ((shift << 12).SIMD_reinterpret_cast<uint32x4>() >> 28);
	int32x4 coef_index = { head & coef_mask };
	int16x8 coefs = LoadByIndex(coef_index, (int*) ea_adpcm_table_v3).SIMD_reinterpret_cast<int16x8>();

    SaveWithStep(samples.SIMD_reinterpret_cast<int32x4>(), (int32_t*)out_PCM, 16);

	vec128 _shuffle = *(vec128*)shuffle;

	for (int i = 0; i < 4; i++) {

        int32x4 data = LoadUnaligned(&in_chunk->XAS_data[0][i*16]).SIMD_reinterpret_cast<int32x4>();

		data = PermuteByIndex(data, _shuffle).SIMD_reinterpret_cast<int32x4>();

		int itrs = 4 - ((i + 1) >> 2); // i != 3 ? 4 : 3;

		for (int j = 0; j < itrs; j++) {
			for (int k = 0; k < 2; k++) {
				int32x4 prediction = mul16_add32(samples, coefs);
				int32x4 correction = (data & nibble_mask).SIMD_reinterpret_cast<int32x4>() >> shift;

				int32x4 predecode = (prediction + correction + rounding) >> fixed_point_offset;

				int16x8 decoded = Clip_int16(predecode);

				samples = { (samples.SIMD_reinterpret_cast<uint32x4>() >> 16) | (((int32x4)(decoded.SIMD_reinterpret_cast<uint16x8>())) << 16) };

				data = data << 4;
			}
			SaveWithStep(samples.SIMD_reinterpret_cast<int32x4>(), (int*)(out_PCM + i*8 + j*2 + 2), 16);
		}
	}
#ifdef _DEBUG
	int16_t PCM2[128];
	decode_XAS_Chunk(in_chunk, PCM2);
	if (memcmp(PCM2, out_PCM, 128 * 2) == 0) {
		printf("ok \n");
	}
	else {
		printf("not ok \n");
	}
#endif // _DEBUG

}

#ifdef BENCH
void PrintRes(const char* mes, uint64_t time, uint64_t reps) {
	printf("%s: total = %llu, per chunk = %f \n", mes, time, (double)time / reps);
}
void Bench(uint32_t reps) {
	XAS_Chunk in_chunk;
	int16_t PCM[128];
	uint64_t start = __rdtsc();
	for (uint64_t i = 0; i < reps; i++) {
		decode_XAS_Chunk(&in_chunk, PCM);
	}
	uint64_t SISD_time = __rdtsc() - start;
	start = __rdtsc();
	for (uint64_t i = 0; i < reps; i++) {
		decode_XAS_Chunk_SIMD(&in_chunk, PCM);
	}
	uint64_t SIMD_time = __rdtsc() - start;

	PrintRes("SISD", SISD_time, reps);
	PrintRes("SIMD", SIMD_time, reps);
}
#endif // BENCH

#define decode_XAS_Chunk decode_XAS_Chunk_SIMD

void decode_XAS(const void* in_data, int16_t* out_PCM, uint32_t n_samples_per_channel, uint32_t n_channels) {
	if (n_samples_per_channel == 0)
		return;
	const XAS_Chunk* _in_data = (XAS_Chunk*)in_data;
	int16_t PCM[128];
	uint32_t n_chunks_per_channel = _GetNumXASChunks(n_samples_per_channel);
	for (int chunk_ind = 0; chunk_ind < n_chunks_per_channel - 1; chunk_ind++) {
		for (int channel_ind = 0; channel_ind < n_channels; channel_ind++) {
			decode_XAS_Chunk(_in_data++, PCM);
			for (int sample_ind = 0; sample_ind < 128; sample_ind++) {
				out_PCM[channel_ind + sample_ind * n_channels] = PCM[sample_ind];
			}
		}
		out_PCM += 128* n_channels;
	}
	uint32_t samples_remain_per_channel = n_samples_per_channel - (n_chunks_per_channel-1)*128;
	for (int channel_ind = 0; channel_ind < n_channels; channel_ind++) {
		decode_XAS_Chunk(_in_data++, PCM);
		for (int sample_ind = 0; sample_ind < samples_remain_per_channel; sample_ind++) {
			out_PCM[channel_ind + sample_ind * n_channels] = PCM[sample_ind];
		}
	}
}

// ~same method as in SX but with fixed point
int simple_CalcCoefShift(const int16_t* pSamples, const int16_t in_prevSamples[2], int num_samples, int *out_coef_index, byte* out_shift) {
	// SX using clip here

	const int num_coefs = 4;

	int min_max_error = INT_MAX;
	int s_min_max_error = INT_MAX; // don't need I think
	int best_coef_ind = 0;
	for (int coef_ind = 0; coef_ind < num_coefs; coef_ind++) {
		int16_t prevSamples[2] = { in_prevSamples[0], in_prevSamples[1] };
		// fixed point 24.8
		// for coef_ind = 0 max_error = max abs sample
		int max_error = 0;
		int s_max_error = 0;
		for (int i = 0; i < num_samples; i++) {
			int prediction = ea_adpcm_table_v2[coef_ind][0] * prevSamples[1] + ea_adpcm_table_v2[coef_ind][1] * prevSamples[0];
			int sample = pSamples[i];
			sample <<= fixed_point_offset;
			int s_error = sample - prediction;
			int error = abs(s_error);
			if (error > max_error) {
				max_error = error;
				s_max_error = s_error;
			}
			prevSamples[0] = prevSamples[1];
			prevSamples[1] = pSamples[i];
		}
		if (max_error < min_max_error) {
			min_max_error = max_error;
			best_coef_ind = coef_ind;
			s_min_max_error = s_max_error;
		}
	}
	int max_min_error_i16 = Clip_int16(min_max_error >> fixed_point_offset);

	int mask = 0x4000;
	int exp_shift;
	for (exp_shift = 0; exp_shift < 12; exp_shift++) {
		if ((((mask >> 3) + max_min_error_i16) & mask) != 0) {
			break;
		}
		mask >>= 1;
	}
	*out_coef_index = best_coef_ind;
	*out_shift = exp_shift;
	return max_min_error_i16;
}


const int shift4_rounding = 0x8 - 1;

void encode_XAS_Chunk(XAS_Chunk* out_chunk, const int16_t in_PCM[128] /*, size_t nSamples = 128*/) {
	//assert(nSamples <= 128);
	for (int j = 0; j < subchunks_in_XAS_chunk; j++) {

		const int16_t *pInSamples = in_PCM + j * 32;

        out_chunk->headers[j].unused = 0;
		out_chunk->headers[j].sample_0 = (pInSamples[0] + shift4_rounding) >> 4;
		out_chunk->headers[j].sample_1 = (pInSamples[1] + shift4_rounding) >> 4;

		int16_t decoded_PCM[32];
		decoded_PCM[0] = out_chunk->headers[j].sample_0 << 4;
		decoded_PCM[1] = out_chunk->headers[j].sample_1 << 4;

		int coef_index;
		byte shift;
		simple_CalcCoefShift(pInSamples + 2, decoded_PCM, 30, &coef_index, &shift);
patch:
		out_chunk->headers[j].coef_index = coef_index;
		out_chunk->headers[j].exp_shift = shift;

		const table_type *coef = ea_adpcm_table_v2[coef_index];
		shift = 12 + fixed_point_offset - shift;

		int16_t *pDecodedSamples = decoded_PCM;

		for (int i = 0; i < 15; i++) {
			byte data = 0;

			for (int n = 0; n < 2; n++) {
				EncodedSample enc = encode_XA_sample(pDecodedSamples, coef, pInSamples[2], shift);

				pDecodedSamples[2] = enc.decoded; // think as decoder will for better precision
				data <<= 4;
				data |= enc.encoded & 0xF;
				pInSamples++, pDecodedSamples++;
			}
			out_chunk->XAS_data[i][j] = data;
		}
	}
}


void encode_XAS(void* out_data, const int16_t* in_PCM, uint32_t n_samples_per_channel, uint32_t n_channels) {
	if (n_samples_per_channel == 0)
		return;
	XAS_Chunk* _out_data = (XAS_Chunk*)out_data;
	uint32_t n_chunks_per_channel = _GetNumXASChunks(n_samples_per_channel);
	int16_t PCM[128];
#pragma nounroll
	for (int chunk_ind = 0; chunk_ind < n_chunks_per_channel - 1; chunk_ind++) {
		for (int channel_ind = 0; channel_ind < n_channels; channel_ind++) {
			const int16_t* t = in_PCM + channel_ind;
			for (int sample_ind = 0; sample_ind < 128; sample_ind++, t += n_channels) {
				PCM[sample_ind] = *t;
			}
			encode_XAS_Chunk(_out_data++, PCM);
			
		}
		in_PCM += 128 * n_channels;
	}
	uint32_t samples_remain_per_channel = n_samples_per_channel - (n_chunks_per_channel - 1) * 128;
	for (int channel_ind = 0; channel_ind < n_channels; channel_ind++) {
		for (int sample_ind = 0; sample_ind < samples_remain_per_channel; sample_ind++) {
			PCM[sample_ind] = in_PCM[channel_ind + sample_ind * n_channels];
		}
		_memset(PCM + samples_remain_per_channel, 0, (128 - samples_remain_per_channel)*sizeof(int16_t));
		encode_XAS_Chunk(_out_data++, PCM);
	}
}


// processing 28 samples, returns number of bytes red (61 or 15)
size_t decode_EA_XA_R2_Chunk(const byte* XA_Chunk, int16_t out_PCM[28], int16_t prev_samples[3]) {
	const byte* p_curr_byte = XA_Chunk;
	int16_t *pSample = out_PCM;
	byte _byte = *(p_curr_byte++);
	int16_t* p_prev_samples = prev_samples;
	if (_byte == 0xEE) {
		prev_samples[1] = Get_s16be(p_curr_byte), p_curr_byte += 2;
		prev_samples[0] = Get_s16be(p_curr_byte), p_curr_byte += 2;
		for (int i = 0; i < samples_in_EA_XA_R_chunk; i++)
			*(pSample++) = Get_s16be(p_curr_byte), p_curr_byte += 2;
	}
	else {
		int coef_index = _byte >> 4;
		const table_type *coef = ea_adpcm_table_v2[coef_index];
		byte shift = 12 + fixed_point_offset - (_byte & 0xF);
		for (int j = 0; j < samples_in_EA_XA_R_chunk / 2; j++) {
			SamplesByte data = *(SamplesByte*)(p_curr_byte++);

			pSample[0] = decode_XA_sample(p_prev_samples, coef, data.sample0, shift);
			prev_samples[2] = pSample[0]; // in case of p_prev_samples == prev_samples
			pSample[1] = decode_XA_sample(p_prev_samples + 1, coef, data.sample1, shift);

#ifdef _DEBUG
			EncodedSample enc = encode_XA_sample(p_prev_samples + 1, coef, pSample[1], shift);
			if (enc.encoded != data.sample1) {
				printf(
					"Reencoding issue:\n"
					"   source sample = %d (0x%X)\n"
					"reencoded sample = %d (0x%X)\n"
					"   source nibble = %d (0x%X)"
					"reencoded nibble = %d (0x%X)\n\n",
					(int)pSample[1], (int)pSample[1], (int)enc.decoded, (int)enc.decoded,
					(int)data.sample1, (int)data.sample1, (int)enc.encoded, (int)enc.encoded);
			}
#endif // _DEBUG


			p_prev_samples = pSample;
			pSample += 2;
		}
		prev_samples[1] = pSample[-1];
		prev_samples[0] = pSample[-2];
	}

	return p_curr_byte - XA_Chunk;
}

void decode_EA_XA_R2(const void* data, int16_t *out_PCM, uint32_t n_samples_per_channel, uint32_t n_channels) {
    // TODO: multi channel
    byte *_data = (byte*)data;
	int16_t prev_samples[3] = { 0 };
    int num_chunks = (n_samples_per_channel + 27) / 28;
	for (int i = 0; i < num_chunks; i++) {
		size_t data_decoded_size = decode_EA_XA_R2_Chunk(_data, out_PCM, prev_samples);
#ifdef _DEBUG
		if (data_decoded_size != sizeof_uncompr_EA_XA_R23_block
			&& data_decoded_size != sizeof_compr_EA_XA_R23_block) {
			printf("Warning: decoded %d bytes\n", data_decoded_size);
			system("pause");
		}
#endif // _DEBUG
        _data += data_decoded_size;
		out_PCM += samples_in_EA_XA_R_chunk;
	}
}

void encode_EA_XA_R2_chunk_nocompr(byte data[sizeof_uncompr_EA_XA_R23_block], const int16_t PCM[28], int16_t prev[2], int nCannels) {
	*data = 0xEE;
    *(int16_t*)(data + 1) = ToBigEndian16(PCM[26*nCannels]);
    *(int16_t*)(data + 3) = ToBigEndian16(PCM[27*nCannels]);
    prev[0] = PCM[26*nCannels];
    prev[1] = PCM[27*nCannels];
	int16_t* pOutData = (int16_t*)(data + 5);
	for (int i = 0; i < 28*nCannels; i+=nCannels) {
		pOutData[i] = ToBigEndian16(PCM[i]);
	}
}

void encode_EA_XA_block(byte data[], const int16_t PCM[], int16_t prev[2], int samples, int PCM_step, const table_type* coefs, byte shift, int data_step = 1){
    for (int i = 0; i < samples/2; i++){
        byte _data = 0;
        for (int j = 0; j < 2; j++){
            EncodedSample enc = encode_XA_sample(prev, coefs, PCM[(i*2 + j)*PCM_step], shift);
            prev[0] = prev[1];
            prev[1] = enc.decoded;
            _data <<= 4;
            _data |= enc.encoded;
        }
        *data = _data;
        data += data_step;
    }
}

size_t encode_EA_XA_R2_chunk(byte data[sizeof_uncompr_EA_XA_R23_block], const int16_t PCM[28], int16_t prev[2], int nCannels, int16_t max_error) {
    int coef_index;
    byte shift;
    int err = simple_CalcCoefShift(PCM, prev, 28, &coef_index, &shift);
    if (err > max_error){
        encode_EA_XA_R2_chunk_nocompr(data, PCM, prev, nCannels);
        return sizeof_uncompr_EA_XA_R23_block;
    }
    else {
        *data++ = coef_index << 4 | shift;
        shift = 12 + fixed_point_offset - shift;
        const table_type * coefs = ea_adpcm_table_v2[coef_index];
        encode_EA_XA_block(data, PCM, prev, 28, nCannels, coefs, shift);
        return sizeof_compr_EA_XA_R23_block;
    }
}

void encode_EA_XA_R1_chunk(byte data[sizeof_EA_XA_R1_chunk], const int16_t PCM[28], const int16_t prev[2],  int nCannels) {
    *(int16_t*)data = ToBigEndian16(prev[0]);
    *(int16_t*)(data + 2) = ToBigEndian16(prev[1]); // ?
    int coef_index;
    byte shift;
    simple_CalcCoefShift(PCM, prev, 28, &coef_index, &shift);
    data[4] = coef_index << 4 | shift;
    int16_t _prev[2]; memcpy(_prev, prev, 4);
    encode_EA_XA_block(data + 5, PCM, _prev, 28, nCannels, ea_adpcm_table_v2[coef_index], 12 + fixed_point_offset - shift);
}

size_t encode_EA_XA_R2_channel(void* data, const int16_t PCM[], uint32_t n_samples_per_channel, uint32_t n_channels, int16_t max_error) {
    int chunks_per_channel = (n_samples_per_channel + 27) / 28;
    int16_t prev[2];
    byte* curr_data = (byte*)data;
    encode_EA_XA_R2_chunk_nocompr(curr_data, PCM, prev, (int)n_channels);
    curr_data += sizeof_uncompr_EA_XA_R23_block;
    for (int chunk_ind = 1; chunk_ind < chunks_per_channel; chunk_ind++){
        curr_data += encode_EA_XA_R2_chunk(curr_data, PCM + 28*chunk_ind*n_channels, prev, n_channels, max_error);
    }
    return curr_data - (byte*)data;
}

size_t encode_EA_XA_R2(void* data, const int16_t PCM[], uint32_t n_samples_per_channel, uint32_t n_channels, int16_t max_error) {
    byte* curr_data = (byte*)data;
    for (int chan_ind = 0; chan_ind < n_channels; chan_ind++){
        curr_data += encode_EA_XA_R2_channel(curr_data, PCM + chan_ind, n_samples_per_channel, n_channels, max_error);
    }
    return curr_data - (byte*)data;
}