#include "heap2.h"
#include "queue2.h"
#include "common.h"

BaseType_t xData = 0;
void vSyscallInit(void)
{
	BaseType_t a =1,b = 2;
	int *ptr;
	a +=b;
	QueueHandle_t testqueuet;
	ptr = (int*)pvPortMalloc(50);
	TickType_t xTimeToBlock = 0;
	testqueuet = xQueueCreate( bktQUEUE_LENGTH, sizeof( BaseType_t ) );

	xQueueSend(testqueuet,&a,0);
	if( xQueueReceive( testqueuet, &xData, xTimeToBlock ) != errQUEUE_EMPTY )
	{
		//xErrorOccurred = pdTRUE;
	}



}


