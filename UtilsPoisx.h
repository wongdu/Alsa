#ifndef _Utils_Poisx_H__
#define _Utils_Poisx_H__

#include <errno.h>
#include <signal.h>

#include <inttypes.h>
typedef unsigned char uint8_t;
typedef unsigned int  uint32_t;
//typedef unsigned long long	uint64_t;

#ifdef __cplusplus
extern "C" {
#endif

enum EventType 
{
	EVENT_TYPE_AUTO,
	EVENT_TYPE_MANUAL
};
struct EventStructure;
typedef struct EventStructure EventStru;

int  EventInit(EventStru **event, enum EventType type);
void EventDestroy(EventStru *event);
int  EventWait(EventStru *event);
int  EventTimedWait(EventStru *event, unsigned long milliseconds);
int  EventTry(EventStru *event);
int  EventSignal(EventStru *event);
void EventReset(EventStru *event);


uint64_t GetTimeNaSec(void);
struct AtomicBoolStruct;

AtomicBoolStruct* AtomicBoolInit();
void AtomicBoolDestroy(AtomicBoolStruct *event);
void SetAtomicBool(AtomicBoolStruct *ptr, bool val);
bool GetAtomicBool(AtomicBoolStruct *ptr);



#ifdef __cplusplus
}
#endif

#endif
