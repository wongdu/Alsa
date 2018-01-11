#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "RecvAudioJitterBuffer.h"

static void* _realloc(void* ptr, unsigned int size);
RecvAudioJitterBuffer::RecvAudioJitterBuffer(uint32_t iframe_duration, uint32_t irate, uint32_t ichannels)
{
	frame_duration = iframe_duration;
	rate = irate;
	channels = ichannels;
	x_data_size = ((frame_duration * rate) / 500) << (channels == 2 ? 1 : 0);
	//x_data_size = 340;
	memset(&buff, 0, sizeof(structBuffer));
	num_pkt_in = 0;
	num_pkt_miss = 0;
	num_pkt_miss_max = (1000 / frame_duration) * 2;
}

RecvAudioJitterBuffer::~RecvAudioJitterBuffer()
{
	Close();
}

int  RecvAudioJitterBuffer::Open()
{
	int tmp = 0;
	state = jitter_buffer_init((int)frame_duration);
	if (!state)
	{
		return -2;
	}

	tmp = 1;
	jitter_buffer_ctl(state, JITTER_BUFFER_SET_MAX_LATE_RATE, &tmp);
	return 0;
}

void RecvAudioJitterBuffer::Close()
{
	if (state)
	{
		jitter_buffer_destroy(state);
		state = NULL;
	}
}

int RecvAudioJitterBuffer::JitterBufferPut(unsigned char *data, unsigned int data_size)
{
	JitterBufferPacket jb_packet;
	static uint16_t seq_num = 0;
	if (!data || !data_size)
	{
		return -1;
	}
	if (!state)
	{
		return -2;
	}

	jb_packet.user_data = 0;
	jb_packet.span = frame_duration;
	jb_packet.len = x_data_size;
	if (x_data_size == data_size)
	{
		jb_packet.data = reinterpret_cast<char*>(data);
		jb_packet.sequence = seq_num;
		jb_packet.timestamp = (seq_num * jb_packet.span);
		seq_num++;
		jitter_buffer_put(state, &jb_packet);
	}
	else
	{
		int i;
		jb_packet.sequence = 0; // Ignore
		if ((buff.index + data_size) > buff.size)
		{
			if (!(buff.ptr = reinterpret_cast<uint8_t*>(_realloc(buff.ptr, (buff.index + data_size)))))
			{
				buff.size = 0;
				buff.index = 0;
				return 0;
			}
			buff.size = (buff.index + data_size);
		}

		memcpy(&buff.ptr[buff.index], data, data_size);
		buff.index += data_size;
		if (buff.index >= x_data_size)
		{
			unsigned int copied = 0;
			for (i = 0; (i + x_data_size) <= buff.index; i += x_data_size)
			{
				jb_packet.data = (char*)&buff.ptr[i];
				jb_packet.timestamp = (++fake_seqnum * jb_packet.span);// reassembled pkt will have fake seqnum
				jitter_buffer_put(state, &jb_packet);
				copied += x_data_size;
			}
			if (copied == buff.index)
			{
				buff.index = 0;
			}
			else
			{
				memmove(&buff.ptr[0], &buff.ptr[copied], (buff.index - copied));
				buff.index -= copied;
			}
		}
	}
	++num_pkt_in;
	return 0;
}

uint32_t RecvAudioJitterBuffer::JitterBufferGet(char *out_data, unsigned int out_size)
{
	JitterBufferPacket jb_packet;
	int ret = 0;
	unsigned int ret_size = 0;


	if (!out_data || !out_size)
	{
		return 0;
	}
	if (!state)
	{
		return 0;
	}
	if (x_data_size != out_size)
	{
		return 0;
	}

	jb_packet.data = out_data;
	jb_packet.len = (spx_uint32_t)out_size;
	//fprintf(stderr, "start read data\n");
	ret = jitter_buffer_get(state, &jb_packet, frame_duration, NULL);
	if (JITTER_BUFFER_OK != ret)
	{
		++num_pkt_miss;
		switch (ret)
		{
		case JITTER_BUFFER_MISSING:
		{
			if (num_pkt_miss > num_pkt_miss_max &&num_pkt_in > num_pkt_miss_max)
			{
				JitterBufferReset();
			}
			break;
		}
		default:
			break;
		}
	}
	else
	{
		num_pkt_miss = 0;
		ret_size = jb_packet.len;
	}
	return ret_size;
}

int RecvAudioJitterBuffer::JitterBufferTick()
{
	if (!state)
	{
		return -1;
	}
	jitter_buffer_tick(state);
	return 0;
}

void RecvAudioJitterBuffer::JitterBufferReset()
{
	if (state)
	{
		jitter_buffer_reset(state);
	}
	num_pkt_in = 0;
	num_pkt_miss = 0;
}

void* _realloc(void* ptr, unsigned int size)
{
	void *ret = NULL;
	if (size)
	{
		if (ptr)
		{
			if (!(ret = realloc(ptr, size)))
			{

			}
		}
		else
		{
			if (!(ret = calloc(1, size)))
			{

			}
		}
	}
	else if (ptr)
	{
		free(ptr);
	}
	return ret;
}