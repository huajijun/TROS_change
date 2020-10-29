#include "heap2.h"
#include "queue2.h"
#include "common.h"
void vSyscallInit(void)
{
	int a =1,b = 2;
	int *ptr;
	a +=b;
	ptr = (int*)pvPortMalloc(50);
	BaseType_t xData;
	TickType_t xTimeToBlock = 0;
	xTestQueue = xQueueCreate( bktQUEUE_LENGTH, sizeof( BaseType_t ) );

	if( xQueueReceive( xTestQueue, &xData, xTimeToBlock ) != errQUEUE_EMPTY )
	{
		//xErrorOccurred = pdTRUE;
	}



}


