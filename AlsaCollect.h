#ifndef _Alsa_Collect_H__
#define _Alsa_Collect_H__

#include <alsa/asoundlib.h>
#include <pthread.h>
#include "UtilsPoisx.h"
static const int kMaxDeviceNameSize = 128;

struct AlsaCaptureOut
{
	const uint8_t *data;
	uint32_t frames;
	uint32_t sample_size;
	uint64_t timestamp;
};

typedef void(*cbFun)(AlsaCaptureOut*,unsigned long);


class AlsaCollect
{
public:
	AlsaCollect(const char* pDevName, unsigned int iRate);
	virtual ~AlsaCollect();
public:
	void Start();
	void Stop();
	void SetCapAudioCB(cbFun cbfun, unsigned long pCaller);
private:
	void Init();
private:
	bool TryOpenDev();
	bool OpenDev();
	void Close();
	bool SetParams();
	void StartReOpenDev();
	void StopReOpenDev();
private:
	static void* CapAudioLoopThread(void *pParam);
	static void* ReOpenDevThread(void *pParam);
	void RunCapAudioLoop();
	void RunReOpenDev();
private:
	snd_pcm_t *handle;	
	snd_pcm_format_t format;
	snd_pcm_uframes_t period_size;
private:
	unsigned int channels;
	unsigned int rate;
	unsigned int sample_size;
	uint8_t *buffer;
	uint64_t first_ts;
	volatile bool active;
	char devName[kMaxDeviceNameSize];
private:
	EventStru *abort_event;
	pthread_t capThread;
	pthread_t reOpenThread;
	AtomicBoolStruct *bCap;
	AtomicBoolStruct *bReOpen;
	cbFun  m_cbFun;
	unsigned long m_Caller;
};

#endif