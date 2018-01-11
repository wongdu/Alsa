#include <unistd.h>
#include "AlsaCollect.h"

void  ProcessCapAudio(AlsaCaptureOut* pOutputAudio, unsigned long)
{
	unsigned int rc;
	rc = write(1, pOutputAudio->data, pOutputAudio->frames*pOutputAudio->sample_size);
	if (rc != pOutputAudio->frames*pOutputAudio->sample_size)
	{
		fprintf(stderr,"short write: wrote %d bytes\n", rc);
	}
}

int main()
{
	int loops = 10;
	AlsaCollect audioColl("default",8000);
	
	audioColl.SetCapAudioCB(&ProcessCapAudio,0);
	audioColl.Start();
	while (loops-- > 0)
	{
		usleep(1000000);
	}
	audioColl.Stop();
	return 0;
}
