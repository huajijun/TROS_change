include "queue2.h"
BaseType_t xQueueGenericReset( QueueHandle_t xQueue, BaseType_t xNewQueue )
{
Queue_t * const pxQueue = ( Queue_t * ) xQueue;

	//configASSERT( pxQueue );

	{
		pxQueue->pcTail = pxQueue->pcHead + ( pxQueue->uxLength * pxQueue->uxItemSize );
		pxQueue->uxMessagesWaiting = ( UBaseType_t ) 0U;
		pxQueue->pcWriteTo = pxQueue->pcHead;
		pxQueue->u.pcReadFrom = pxQueue->pcHead + ( ( pxQueue->uxLength - ( UBaseType_t ) 1U ) * pxQueue->uxItemSize );
		pxQueue->xRxLock = queueUNLOCKED;
		pxQueue->xTxLock = queueUNLOCKED;

		if( xNewQueue == false )
		{
			/* If there are tasks blocked waiting to read from the queue, then
			the tasks will remain blocked as after this function exits the queue
			will still be empty.  If there are tasks blocked waiting to write to
			the queue, then one should be unblocked as after this function exits
			it will be possible to write to it. */
			if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
			{
				if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) == pdTRUE )
				{
					queueYIELD_IF_USING_PREEMPTION();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			/* Ensure the event queues start in the correct state. */
			vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
			vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );
		}
	}
	//taskEXIT_CRITICAL();

	/* A value is returned for calling semantic consistency with previous
	versions. */
	return pdPASS;
}

/*
 * Input Parament  uxQueueLength : the lenght of queue
 * Input Parament  uxItemSize    : the size of data
 *
 *
 *
 *
*/
QueueHandle_t xQueueGenericCreate( const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, const uint8_t ucQueueType )
{
	Queue_t *pxNewQueue;
	size_t xQueueSizeInBytes;
	QueueHandle_t xReturn = NULL;

	/* Remove compiler warnings about unused parameters should
	configUSE_TRACE_FACILITY not be set to 1. */
	( void ) ucQueueType;

	configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

	if( uxItemSize == ( UBaseType_t ) 0 )
	{
		/* There is not going to be a queue storage area. */
		xQueueSizeInBytes = ( size_t ) 0;
	}
	else
	{
		/* The queue is one byte longer than asked for to make wrap checking
		easier/faster. */
		xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ) + ( size_t ) 1; /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
	}
	/* Allocate the new queue structure and storage area. */
	pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );


	if( pxNewQueue != NULL )
	{
		if( uxItemSize == ( UBaseType_t ) 0 )
		{
			/* No RAM was allocated for the queue storage area, but PC head
			cannot be set to NULL because NULL is used as a key to say the queue
			is used as a mutex.  Therefore just set pcHead to point to the queue
			as a benign value that is known to be within the memory map. */
			pxNewQueue->pcHead = ( int8_t * ) pxNewQueue;
		}
		else
		{
			/* Jump past the queue structure to find the location of the queue
			storage area. */
			pxNewQueue->pcHead = ( ( int8_t * ) pxNewQueue ) + sizeof( Queue_t );
		}

		/* Initialise the queue members as described above where the queue type
		is defined. */
		pxNewQueue->uxLength = uxQueueLength;
		pxNewQueue->uxItemSize = uxItemSize;
		( void ) xQueueGenericReset( pxNewQueue, true);

		#if ( configUSE_TRACE_FACILITY == 1 )
		{
			pxNewQueue->ucQueueType = ucQueueType;
		}
		#endif /* configUSE_TRACE_FACILITY */

		#if( configUSE_QUEUE_SETS == 1 )
		{
			pxNewQueue->pxQueueSetContainer = NULL;
		}
		#endif /* configUSE_QUEUE_SETS */

		//traceQUEUE_CREATE( pxNewQueue );
		xReturn = pxNewQueue;
	}

	return xReturn;
}		
void vQueueAddToRegistry( QueueHandle_t xQueue, const char *pcQueueName ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
UBaseType_t ux;

	/* See if there is an empty space in the registry.  A NULL name denotes
	a free slot. */
	for( ux = ( UBaseType_t ) 0U; ux < ( UBaseType_t ) configQUEUE_REGISTRY_SIZE; ux++ )
	{
		if( xQueueRegistry[ ux ].pcQueueName == NULL )
		{
			/* Store the information on this queue. */
			xQueueRegistry[ ux ].pcQueueName = pcQueueName;
			xQueueRegistry[ ux ].xHandle = xQueue;

			traceQUEUE_REGISTRY_ADD( xQueue, pcQueueName );
			break;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}