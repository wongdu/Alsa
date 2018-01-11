//#include <unistd.h>
#include "AlsaCollect.h"


#define NSEC_PER_SEC  1000000000LL
#define NSEC_PER_MSEC 1000000L
#define STARTUP_TIMEOUT_NS (500 * NSEC_PER_MSEC)
#define REOPEN_TIMEOUT 1000UL
#define SafeReturn(X) if (X){ \
	return false;\
}


AlsaCollect::AlsaCollect(const char* pDevName, unsigned int iRate) :devName(), m_cbFun(NULL)
{
	active = false;
	rate = iRate;
	handle = NULL;
	buffer = NULL;
	first_ts = 0;
	memset(devName, 0, kMaxDeviceNameSize);
	memcpy(devName,pDevName,strlen(pDevName));
	capThread = 0;
	reOpenThread = 0;
	m_Caller = 0;
	
	Init();
}

AlsaCollect::~AlsaCollect()
{
	if (handle)
	{
		Close();
	}

	AtomicBoolDestroy(bReOpen);
	AtomicBoolDestroy(bCap);
	EventDestroy(abort_event);
}

void AlsaCollect::Start()
{
	active = true;
	TryOpenDev();
}

void AlsaCollect::Stop()
{
	StopReOpenDev();
	Close();
	active = false;
}

void AlsaCollect::SetCapAudioCB(cbFun cbfun, unsigned long pCaller)
{
	m_cbFun = cbfun;
	m_Caller = pCaller;
}

void AlsaCollect::Init()
{
	if (EventInit(&abort_event, EVENT_TYPE_MANUAL) != 0)
	{
		goto cleanup;
	}

	bReOpen =AtomicBoolInit();
	bCap = AtomicBoolInit();
	
cleanup:
	
	return;
}

bool AlsaCollect::TryOpenDev()
{
	StopReOpenDev();
	if (OpenDev())
		return true;

	StartReOpenDev();
	return false;
}

bool AlsaCollect::OpenDev()
{
	pthread_attr_t attr;
	int err = snd_pcm_open(&handle, devName, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
	if (err<0)
	{
		return false;
	}
	if (!SetParams())
	{
		goto cleanup;
	}
	if (snd_pcm_state(handle) != SND_PCM_STATE_PREPARED)
	{		
		goto cleanup;
	}
	err = snd_pcm_start(handle);
	if (err < 0) 
	{
		goto cleanup;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	err = pthread_create(&capThread, &attr, CapAudioLoopThread, this);
	if (err)
	{
		pthread_attr_destroy(&attr);
		goto cleanup;
	}
	pthread_attr_destroy(&attr);
	return true;
cleanup:
	Close();
	return false;
}

void AlsaCollect::Close()
{
	if (capThread)
	{
		SetAtomicBool(bCap, false);
		pthread_join(capThread, NULL);
		capThread = 0;
	}
	if (handle) 
	{
		snd_pcm_drain(handle);
		snd_pcm_drop(handle);
		snd_pcm_close(handle);
		handle = NULL;
	}
	if (buffer)
	{
		free(buffer);
		buffer = NULL;
	}
}

bool AlsaCollect::SetParams()
{
	snd_pcm_hw_params_t *hwparams;
	int err = 0;
	int dir = 0;
	unsigned int buffer_time = 0;
	unsigned int period_time = 0;

	snd_pcm_hw_params_alloca(&hwparams);
	err = snd_pcm_hw_params_any(handle, hwparams);
	SafeReturn(err);
	
	err = snd_pcm_hw_params_set_access(handle, hwparams,SND_PCM_ACCESS_RW_INTERLEAVED);
	SafeReturn(err);

	format = SND_PCM_FORMAT_S16_LE;
	err = snd_pcm_hw_params_set_format(handle, hwparams,format);
	SafeReturn(err);

	err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, &dir);
	SafeReturn(err);
	
	err = snd_pcm_hw_params_get_channels(hwparams, &channels);
	if (err < 0)
		channels = 1;

	err = snd_pcm_hw_params_set_channels_near(handle, hwparams,&channels);	
	SafeReturn(err);

	err = snd_pcm_hw_params_get_buffer_time_max(hwparams, &buffer_time, 0);
	SafeReturn(err);
	if (buffer_time > 320000)
		buffer_time = 320000;
	period_time = buffer_time / 4;
	err = snd_pcm_hw_params_set_buffer_time_near(handle, hwparams, &buffer_time, 0);
	SafeReturn(err);
	err = snd_pcm_hw_params_set_period_time_near(handle, hwparams, &period_time, 0);
	SafeReturn(err);

	err = snd_pcm_hw_params(handle, hwparams);
	SafeReturn(err);

	err = snd_pcm_hw_params_get_period_size(hwparams, &period_size,&dir);
	SafeReturn(err);

	sample_size = (channels * snd_pcm_format_physical_width(format)) / 8;

	if (buffer)
		free(buffer);
	buffer = (uint8_t *)malloc(period_size * sample_size);
	if (!buffer)
	{
		raise(SIGTRAP);
	}
	return true;
}

void AlsaCollect::StartReOpenDev()
{
	pthread_attr_t attr;
	int err;

	if (GetAtomicBool(bReOpen))
		return;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	err = pthread_create(&reOpenThread, &attr, ReOpenDevThread, this);
	if (err)
	{
		//log error
	}

	pthread_attr_destroy(&attr);
}

void AlsaCollect::StopReOpenDev()
{
	if (GetAtomicBool(bReOpen))
	{
		EventSignal(abort_event);
	}
	if (reOpenThread)
	{
		pthread_join(reOpenThread, NULL);
		reOpenThread = 0;
	}
	EventReset(abort_event);
}

void* AlsaCollect::CapAudioLoopThread(void *pParam)
{
	AlsaCollect* pCollect = reinterpret_cast<AlsaCollect*>(pParam);
	pCollect->RunCapAudioLoop();
	return NULL;
}

void* AlsaCollect::ReOpenDevThread(void *pParam)
{
	AlsaCollect* pCollect = reinterpret_cast<AlsaCollect*>(pParam);
	pCollect->RunReOpenDev();
	return NULL;
}

void AlsaCollect::RunCapAudioLoop()
{
	AlsaCaptureOut out;
	out.data = buffer;
	//out.samples_per_sec = rate;
	out.sample_size = sample_size;
	SetAtomicBool(bCap, true);

	do
	{
		snd_pcm_sframes_t frames = snd_pcm_readi(handle, buffer, period_size);
		if (!GetAtomicBool(bCap))
		{
			break;
		}

		if (frames <= 0)
		{
			frames = snd_pcm_recover(handle, frames, 0);
			if (frames <= 0)
			{
				snd_pcm_wait(handle, 100);
				continue;
			}
		}

		out.frames = frames;
		out.timestamp = GetTimeNaSec() - ((frames * NSEC_PER_SEC) / rate);
		if (!first_ts)
		{
			first_ts = out.timestamp + STARTUP_TIMEOUT_NS;
		}
		if (out.timestamp > first_ts &&m_cbFun)
		{
			//OutputAudio(&out);
			m_cbFun(&out, m_Caller);
		}
	} while (GetAtomicBool(bCap));

	pthread_exit(NULL);
}

void AlsaCollect::RunReOpenDev()
{
	unsigned long timeout = REOPEN_TIMEOUT;
	SetAtomicBool(bReOpen, true);

	while (EventTimedWait(abort_event, timeout) == ETIMEDOUT)
	{
		if (OpenDev())
			break;

		if (timeout < (REOPEN_TIMEOUT * 5))
		{
			timeout += REOPEN_TIMEOUT;
		}
	}

	SetAtomicBool(bReOpen, false);
	pthread_exit(NULL);
}