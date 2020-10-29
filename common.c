#include "common.h"
void memcpy(char * des , char * src, int number)
{
	for(int i = 0; i< number; i++)
	{
		*des++ = *src++;
	}
}