#include <unistd.h>
#include "AlsaPlay.h"

enum ThreadPriority
{
	kLowPriority = 1,
	kNormalPriority = 2,
	kHighPriority = 3,
	kHighestPriority = 4,
	kRealtimePriority = 5
};

#define REOPEN_TIMEOUT 1000UL
#define SafeReturn(X) if (X){ \
	return false;\
}

static const uint32_t frameDuration = 20;
int ConvertToSystemPriority(ThreadPriority priority, int min_prio, int max_prio);
static int _thread_set_priority(pthread_t thread, int prio);


AlsaPlay::AlsaPlay(const char* pDevName, unsigned int iRate) :m_pJitterBuffer(NULL), devName()
{
	active = false;
	rate = iRate;
	handle = NULL;
	buffer = NULL;
	memset(devName, 0, kMaxPlayDeviceNameSize);
	memcpy(devName, pDevName, strlen(pDevName));
	abort_event = NULL;	
	playThread = 0;
	reOpenThread = 0;
	Init();	
}

AlsaPlay::~AlsaPlay()
{
	if (handle)
	{
		Close();
	}

	AtomicBoolDestroy(bReOpen);
	AtomicBoolDestroy(bPlay);
	pthread_mutex_destroy(&bufMutex);
	EventDestroy(abort_event);
}

void AlsaPlay::Start()
{	
	if (TryOpenDev())
	{
		active = true;
	}
}

void AlsaPlay::Stop()
{
	StopReOpenDev();
	Close();
	active = false;
}

void AlsaPlay::InputVoiceData(unsigned char *szData, unsigned int  nSize)
{		
	int result = 0;
	if (!active)
	{
		return;
	}
	//return;
	result = snd_pcm_writei(handle, szData, nSize/2);
	if (result == -EPIPE)
	{
		//snd_pcm_prepare(handle);
		printf("InputVoiceData  error %d\n", nSize);
		fflush(stdout);
		result = snd_pcm_recover(handle, result, 0);
		if (0 == result)
		{
			//goto next;
		}
		
	}

	return;
	if (m_pJitterBuffer)
	{
		LockJitterBufferMutex();
		m_pJitterBuffer->JitterBufferPut(szData,nSize);
		UnLockJitterBufferMutex();
	}
}

void AlsaPlay::Init()
{
	if (EventInit(&abort_event, EVENT_TYPE_MANUAL) != 0)
	{
		goto cleanup;
	}

	bReOpen = AtomicBoolInit();
	bPlay = AtomicBoolInit();
	
cleanup:
	
	return;
}

bool AlsaPlay::InitJitterBufferMutex()
{
	int rc;
	pthread_mutexattr_t attr;
	
	rc = pthread_mutexattr_init(&attr);
	SafeReturn(rc);
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	SafeReturn(rc);

	rc = pthread_mutex_init(&bufMutex, &attr);
	SafeReturn(rc);
	rc = pthread_mutexattr_destroy(&attr);
	if (0!=rc)
	{
		pthread_mutex_destroy(&bufMutex);
		return false;
	}
	return true;
}

bool AlsaPlay::TryOpenDev()
{
	StopReOpenDev();
	if (OpenDev())
		return true;

	StartReOpenDev();
	return false;
}

bool AlsaPlay::OpenDev()
{
	pthread_attr_t attr;
	//int err = snd_pcm_open(&handle, devName, SND_PCM_STREAM_PLAYBACK, 0);
	int err = snd_pcm_open(&handle, devName, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0)
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
	/*err = snd_pcm_start(handle);
	if (err < 0)
	{
		goto cleanup;
	}*/

	if (m_pJitterBuffer)
	{
		delete m_pJitterBuffer;
	}
	m_pJitterBuffer = new RecvAudioJitterBuffer(frameDuration, rate, channels);
	if (!m_pJitterBuffer || m_pJitterBuffer->Open())
	{
		goto cleanup;
	}
	return true;

	if (!InitJitterBufferMutex())
	{
		goto cleanup;
	}	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	//err = pthread_create(&playThread, &attr, PlayAudioLoopThread, this);
	if (err)
	{
		pthread_attr_destroy(&attr);
		goto cleanup;
	}
	fprintf(stderr, "PlayAudioLoopThread\n");
	pthread_attr_destroy(&attr);
	return true;
cleanup:
	Close();
	return false;
}

void AlsaPlay::Close()
{
	if (playThread)
	{
		SetAtomicBool(bPlay, false);
		pthread_join(playThread, NULL);
		playThread = 0;
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

bool AlsaPlay::SetParams()
{
	snd_pcm_hw_params_t *hwparams;
	int err = 0;
	int dir = 0;
	unsigned int buffer_time = 0;
	unsigned int period_time = 0;

	snd_pcm_hw_params_alloca(&hwparams);
	err = snd_pcm_hw_params_any(handle, hwparams);
	SafeReturn(err);

	err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	SafeReturn(err);

	format = SND_PCM_FORMAT_S16_LE;
	err = snd_pcm_hw_params_set_format(handle, hwparams, format);
	SafeReturn(err);

	err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rate, &dir);
	SafeReturn(err);

	err = snd_pcm_hw_params_get_channels(hwparams, &channels);
	if (err < 0)
		channels = 1;

	err = snd_pcm_hw_params_set_channels_near(handle, hwparams, &channels);
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

	err = snd_pcm_hw_params_get_period_size(hwparams, &period_size, &dir);
	SafeReturn(err);

	sample_size = (channels * snd_pcm_format_physical_width(format)) / 8;	
	period_size = period_size*sample_size;
	if (buffer)
		free(buffer);
	buffer = (char *)malloc(period_size);
	if (!buffer)
	{
		raise(SIGTRAP);
	}
	return true;
}

void AlsaPlay::StartReOpenDev()
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

void AlsaPlay::StopReOpenDev()
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

void* AlsaPlay::PlayAudioLoopThread(void *pParam)
{
	AlsaPlay* pPlay = reinterpret_cast<AlsaPlay*>(pParam);
	pPlay->RunPlayAudioLoop();
	return NULL;
}

void* AlsaPlay::ReOpenDevThread(void *pParam)
{
	AlsaPlay* pPlay = reinterpret_cast<AlsaPlay*>(pParam);
	pPlay->RunReOpenDev();
	return NULL;
}

void AlsaPlay::RunPlayAudioLoop()
{
	int result = 0;
	unsigned int ret_size = 0;
	//uint8_t *buffer = NULL;
	//char *buffer = NULL;
	//unsigned int readSize = period_size*sample_size;
	unsigned int readSize = period_size;
	SetAtomicBool(bPlay, true);
	_thread_set_priority(pthread_self(), kHighestPriority);

	do
	{
		LockJitterBufferMutex();
		//get audio data from jitbuffer
		if (m_pJitterBuffer)
		{
			ret_size = m_pJitterBuffer->JitterBufferGet(buffer, readSize);
		}
		if (!ret_size)
		{
			//fprintf(stderr, "no data\n");
			//usleep(10000);
			//continue;
			memset(buffer, 0, readSize);
		}
		if (!GetAtomicBool(bPlay))
		{
			UnLockJitterBufferMutex();
			break;
		}
		//fprintf(stderr, "start play audio\n");
		result = snd_pcm_writei(handle, buffer, period_size);
		if (result == -EPIPE)
		{
			//snd_pcm_prepare(handle);
			result = snd_pcm_recover(handle,result,0);
			if (0==result)
			{
				goto next;
			}
			UnLockJitterBufferMutex();
			break; //end play loop
		}
next:
		UnLockJitterBufferMutex();
	} while (GetAtomicBool(bPlay));
}

void AlsaPlay::RunReOpenDev()
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

int AlsaPlay::LockJitterBufferMutex()
{
	int ret = EINVAL;
	ret = pthread_mutex_lock(&bufMutex);
	return ret;
}

int AlsaPlay::UnLockJitterBufferMutex()
{
	int ret = EINVAL;
	ret = pthread_mutex_unlock(&bufMutex);
	return ret;
}

int ConvertToSystemPriority(ThreadPriority priority, int min_prio,int max_prio) 
{
	assert(max_prio - min_prio > 2);
	const int top_prio = max_prio - 1;
	const int low_prio = min_prio + 1;

	switch (priority) 
	{
	case kLowPriority:
		return low_prio;
	case kNormalPriority:
		// The -1 ensures that the kHighPriority is always greater or equal to
		// kNormalPriority.
		return (low_prio + top_prio - 1) / 2;
	case kHighPriority:
		//return std::max(top_prio - 2, low_prio);
		return (top_prio - 2) > low_prio ? (top_prio - 2) : low_prio;
	case kHighestPriority:
		//return std::max(top_prio - 1, low_prio);
		return (top_prio - 1) >low_prio ? (top_prio - 1) : low_prio;
	case kRealtimePriority:
		return top_prio;
	}
	assert(false);
	return low_prio;
}

int _thread_set_priority(pthread_t thread, int prio)
{
	struct sched_param param;
	int policy;
	int rc;

	rc = pthread_getschedparam(thread, &policy, &param);
	if (rc != 0)
	{
		return -1;
	}

	const int min_prio = sched_get_priority_min(policy);
	const int max_prio = sched_get_priority_max(policy);
	if ((min_prio == EINVAL) || (max_prio == EINVAL))
	{
		return -1;
	}
	if (max_prio - min_prio <= 2)
	{
		// There is no room for setting priorities with any granularity.
		return -1;
	}

	//param.sched_priority = ConvertToSystemPriority(prio, min_prio, max_prio);
	param.sched_priority = ConvertToSystemPriority(ThreadPriority(prio), min_prio, max_prio);
	rc = pthread_setschedparam(thread, policy, &param);
	if (rc != 0)
	{
		return -1;
	}
	return 0;
}