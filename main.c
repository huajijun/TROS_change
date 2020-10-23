#include "heap2.h"
#include "queue2.h"
void vSyscallInit(void)
{
	int a =1,b = 2;
	int *ptr;
	a +=b;
	ptr = (int*)pvPortMalloc(50);
	xTestQueue = xQueueCreate( bktQUEUE_LENGTH, sizeof( BaseType_t ) );

}