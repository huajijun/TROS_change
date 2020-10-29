/* length */
#include "list2.h"
#include "common.h"
#ifndef QUEUE
#define QUEUE
typedef void * QueueHandle_t;

typedef struct QUEUE_REGISTRY_ITEM
{
	const char *pcQueueName; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
	QueueHandle_t xHandle;
} xQueueRegistryItem;

#define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()

#define portYIELD_WITHIN_API portYIELD
#define portYIELD()					vPortYield()


#define taskENTER_CRITICAL()		portENTER_CRITICAL()
#define taskEXIT_CRITICAL()			portEXIT_CRITICAL()

#define portENTER_CRITICAL()					vTaskEnterCritical()


#define portEXIT_CRITICAL()						vTaskExitCritical()


#define queueQUEUE_TYPE_BASE				( ( uint8_t ) 0U )

#define xQueueReceive( xQueue, pvBuffer, xTicksToWait ) xQueueGenericReceive( ( xQueue ), ( pvBuffer ), ( xTicksToWait ), pdFALSE )

#define xQueueCreate( uxQueueLength, uxItemSize ) xQueueGenericCreate( uxQueueLength, uxItemSize, queueQUEUE_TYPE_BASE )



typedef long BaseType_t;     

typedef unsigned long UBaseType_t;

#define configUSE_QUEUE_SETS 0

#define tskIDLE_PRIORITY            ( ( UBaseType_t ) 0U )

#define queueQUEUE_TYPE_BASE                ( ( uint8_t ) 0U )

#define	queueSEND_TO_BACK		( ( BaseType_t ) 0 )
#define	queueSEND_TO_FRONT		( ( BaseType_t ) 1 )
#define queueOVERWRITE			( ( BaseType_t ) 2 )

#define xQueueCreate( uxQueueLength, uxItemSize ) xQueueGenericCreate( uxQueueLength, uxItemSize, queueQUEUE_TYPE_BASE )

#define xQueueSend( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )
#define xQueueSendToBack( xQueue, pvItemToQueue, xTicksToWait ) xQueueGenericSend( ( xQueue ), ( pvItemToQueue ), ( xTicksToWait ), queueSEND_TO_BACK )

#define bktQUEUE_LENGTH             ( 5 )

#define configQUEUE_REGISTRY_SIZE 8
typedef xQueueRegistryItem QueueRegistryItem_t;




/*
 * Definition of the type of queue used by the scheduler.
 */

typedef struct QueueDefinition
{
	int8_t *pcHead;					/*< Points to the beginning of the queue storage area. */
	int8_t *pcTail;					/*< Points to the byte at the end of the queue storage area.  Once more byte is allocated than necessary to store the queue items, this is used as a marker. */
	int8_t *pcWriteTo;				/*< Points to the free next place in the storage area. */

	union							/* Use of a union is an exception to the coding standard to ensure two mutually exclusive structure members don't appear simultaneously (wasting RAM). */
	{
		int8_t *pcReadFrom;			/*< Points to the last place that a queued item was read from when the structure is used as a queue. */
		UBaseType_t uxRecursiveCallCount;/*< Maintains a count of the number of times a recursive mutex has been recursively 'taken' when the structure is used as a mutex. */
	} u;

	List_t xTasksWaitingToSend;		/*< List of tasks that are blocked waiting to post onto this queue.  Stored in priority order. */
	List_t xTasksWaitingToReceive;	/*< List of tasks that are blocked waiting to read from this queue.  Stored in priority order. */

	volatile UBaseType_t uxMessagesWaiting;/*< The number of items currently in the queue. */
	UBaseType_t uxLength;			/*< The length of the queue defined as the number of items it will hold, not the number of bytes. */
	UBaseType_t uxItemSize;			/*< The size of each items that the queue will hold. */

	volatile BaseType_t xRxLock;	/*< Stores the number of items received from the queue (removed from the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
	volatile BaseType_t xTxLock;	/*< Stores the number of items transmitted to the queue (added to the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */

	#if ( configUSE_TRACE_FACILITY == 1 )
		UBaseType_t uxQueueNumber;
		uint8_t ucQueueType;
	#endif

	#if ( configUSE_QUEUE_SETS == 1 )
		struct QueueDefinition *pxQueueSetContainer;
	#endif

} xQUEUE;
typedef xQUEUE Queue_t;

static QueueHandle_t xTestQueue;

QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];

#endif