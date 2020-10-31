#include "heap2.h"
#include "queue2.h"
#include "common.h"
#include "task.h"

BaseType_t xData = 0;
void TestProgram(void);
void vSyscallInit(void)
{
	xTaskCreate( TestProgram, "TestProgram", 4096, NULL, 20, NULL );
}

void TestProgram(void)
{

	BaseType_t a =1;
	QueueHandle_t testqueuet;
	testqueuet = xQueueCreate( bktQUEUE_LENGTH, sizeof( BaseType_t ) );
	xQueueSend(testqueuet,&a,0);
	if( xQueueReceive( testqueuet, &xData, 0 ) != errQUEUE_EMPTY )
	{
		//xErrorOccurred = pdTRUE;
	}
}

