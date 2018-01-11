#ifndef _Alsa_Play_H__
#define _Alsa_Play_H__

#include <alsa/asoundlib.h>
#include <pthread.h>
#include "RecvAudioJitterBuffer.h"
#include "UtilsPoisx.h"

static const int kMaxPlayDeviceNameSize = 128;

class AlsaPlay
{
public:
	AlsaPlay(const char* pDevName, unsigned int iRate);
	virtual ~AlsaPlay();
public:
	void Start();
	void Stop();
public:
	void InputVoiceData(unsigned char *szData, unsigned int nSize);
private:
	void Init();
	bool InitJitterBufferMutex();
private:
	bool TryOpenDev();
	bool OpenDev();
	void Close();
	bool SetParams();
	void StartReOpenDev();
	void StopReOpenDev();
private:
	static void* PlayAudioLoopThread(void *pParam);
	static void* ReOpenDevThread(void *pParam);
	void RunPlayAudioLoop();
	void RunReOpenDev();
	int  LockJitterBufferMutex();
	int  UnLockJitterBufferMutex();
private:
	snd_pcm_t *handle;
	snd_pcm_format_t format;
	snd_pcm_uframes_t period_size;
private:
	unsigned int channels;
	unsigned int rate;
	unsigned int sample_size;
	//uint8_t *buffer;
	char *buffer;
	//char   *pBuffer;
	RecvAudioJitterBuffer* m_pJitterBuffer;
	volatile bool active;
	char devName[kMaxPlayDeviceNameSize];
private:
	EventStru *abort_event;
	pthread_mutex_t bufMutex;
	pthread_t playThread;
	pthread_t reOpenThread;	
	AtomicBoolStruct *bPlay;
	AtomicBoolStruct *bReOpen;
};

#endif