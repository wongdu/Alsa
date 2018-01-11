
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "UtilsPoisx.h"


struct EventStructure
{
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	volatile bool   signalled;
	bool            manual;
};

struct AtomicBoolStruct
{
	pthread_mutex_t	 mutex;
	bool   value;
};

int  EventInit(EventStru **event, enum EventType type)
{
	int code = 0;
	struct EventStructure *data = (struct EventStructure *)malloc(sizeof(struct EventStructure));
	if (!data)
	{
		raise(SIGTRAP);
	}

	if ((code = pthread_mutex_init(&data->mutex, NULL)) < 0)
	{
		free(data);
		return code;
	}

	if ((code = pthread_cond_init(&data->cond, NULL)) < 0) 
	{
		pthread_mutex_destroy(&data->mutex);
		free(data);
		return code;
	}

	data->manual = (type == EVENT_TYPE_MANUAL);
	data->signalled = false;
	*event = data;

	return 0;
}

void EventDestroy(EventStru *event)
{
	if (event) 
	{
		pthread_mutex_destroy(&event->mutex);
		pthread_cond_destroy(&event->cond);
		free(event);
	}
}

int  EventWait(EventStru *event)
{
	int code = 0;
	pthread_mutex_lock(&event->mutex);
	if (!event->signalled)
	{
		code = pthread_cond_wait(&event->cond, &event->mutex);
	}

	if (code == 0)
	{
		if (!event->manual)
		{
			event->signalled = false;
		}			
		pthread_mutex_unlock(&event->mutex);
	}

	return code;
}

static inline void add_ms_to_ts(struct timespec *ts,unsigned long milliseconds)
{
	ts->tv_sec += milliseconds / 1000;
	ts->tv_nsec += (milliseconds % 1000) * 1000000;
	if (ts->tv_nsec > 1000000000)
	{
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000;
	}
}

int  EventTimedWait(EventStru *event, unsigned long milliseconds)
{
	int code = 0;
	pthread_mutex_lock(&event->mutex);

	if (!event->signalled) 
	{
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		add_ms_to_ts(&ts, milliseconds);
		code = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
	}
	if (code == 0)
	{
		if (!event->manual)
		{
			event->signalled = false;
		}			
	}

	pthread_mutex_unlock(&event->mutex);
	return code;
}

int  EventTry(EventStru *event)
{
	int ret = EAGAIN;

	pthread_mutex_lock(&event->mutex);
	if (event->signalled) 
	{
		if (!event->manual)
		{
			event->signalled = false;
		}			
		ret = 0;
	}
	pthread_mutex_unlock(&event->mutex);

	return ret;
}

int  EventSignal(EventStru *event)
{
	int code = 0;

	pthread_mutex_lock(&event->mutex);
	code = pthread_cond_signal(&event->cond);
	event->signalled = true;
	pthread_mutex_unlock(&event->mutex);

	return code;
}

void EventReset(EventStru *event)
{
	pthread_mutex_lock(&event->mutex);
	event->signalled = false;
	pthread_mutex_unlock(&event->mutex);
}


uint64_t GetTimeNaSec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

AtomicBoolStruct* AtomicBoolInit()
{
	struct AtomicBoolStruct *pData = (struct AtomicBoolStruct *)malloc(sizeof(struct AtomicBoolStruct));
	if (!pData)
	{
		raise(SIGTRAP);
	}

	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
	{
		goto cleanup;
	}
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
	{
		goto cleanup;
	}

	if (pthread_mutex_init(&pData->mutex, &attr) != 0)
	{
		goto cleanup;
	}
	if (pthread_mutexattr_destroy(&attr) != 0)
	{
		pthread_mutex_destroy(&pData->mutex);
		return NULL;
	}
	return pData;
cleanup:

	return NULL;
}
void AtomicBoolDestroy(AtomicBoolStruct *event)
{
	if (event)
	{
		pthread_mutex_destroy(&event->mutex);
	}
}

void SetAtomicBool(AtomicBoolStruct *ptr, bool val)
{
	if (ptr)
	{
		pthread_mutex_lock(&ptr->mutex);
		ptr->value = val;
		pthread_mutex_unlock(&ptr->mutex);
	}
}

bool GetAtomicBool(AtomicBoolStruct *ptr)
{
	bool bOldValue;
	if (ptr)
	{
		pthread_mutex_lock(&ptr->mutex);
		bOldValue = ptr->value;
		pthread_mutex_unlock(&ptr->mutex);
	}
	return bOldValue;
}
