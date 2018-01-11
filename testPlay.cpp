#include <unistd.h>
#include "AlsaPlay.h"

static const unsigned int bufSize = 320;
static const unsigned int frameRate = 8000;

int main(int argc, char *argv[])
{
	int rc;
	FILE *fp = NULL;
	char *buffer;
	int size;
	if (argc != 2) 
	{
		fprintf(stderr, "Usage: ./lplay <FILENAME>/n");
		return -1;
	}
	fp = fopen(argv[1], "rb");
	if (fp == NULL)
	{
		fprintf(stderr, "Error open [%s]/n", argv[1]);
		return -1;
	}
	size = bufSize;
	buffer = (char *)malloc(size);
	AlsaPlay audioplay("default", frameRate);
	

	audioplay.Start();
	while (1)
	{
		rc = fread(buffer, 1, size, fp);		
		if (rc == 0) 
		{
			fprintf(stderr, "end of file on input\n");
			break;
		}
		else if (rc != size)
		{
			fprintf(stderr, "short read: read %d bytes\n", rc);
		}
		audioplay.InputVoiceData(reinterpret_cast<unsigned char*>(buffer), size);
		//usleep(25000);
	}
	sleep(5);
	audioplay.Stop();
	return 0;
}
