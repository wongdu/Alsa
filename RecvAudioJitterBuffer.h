#ifndef _Recv_Audio_Jitter_Buffer_H__
#define _Recv_Audio_Jitter_Buffer_H__

#include "config.h"
#include "speex_jitter.h"

struct structBuffer{
	uint8_t* ptr;
	unsigned int size;
	unsigned int index;
} ;

class RecvAudioJitterBuffer
{
public:
	RecvAudioJitterBuffer(uint32_t frame_duration, uint32_t rate, uint32_t channels);
	virtual ~RecvAudioJitterBuffer();

public:
	int  Open();
	void Close();
	int  JitterBufferPut(unsigned char *data, unsigned int  data_size);
	uint32_t  JitterBufferGet(char *out_data, unsigned int out_size);
	int  JitterBufferTick();
	void JitterBufferReset();
private:
	JitterBuffer* state;

private:
	uint32_t rate;
	uint32_t frame_duration;
	uint32_t channels;
	uint32_t x_data_size; // expected data size
	uint16_t fake_seqnum; // if ptime mismatch then, reassembled pkt will have invalid seqnum
	structBuffer buff;
private:
	uint64_t num_pkt_in; // Number of incoming pkts since the last reset
	uint64_t num_pkt_miss; // Number of times we got consecutive "JITTER_BUFFER_MISSING" results
	uint64_t num_pkt_miss_max; // Max value for "num_pkt_miss" before reset()ing the jitter buffer
};

#endif