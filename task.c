#include "task.h"
#include "list2.h"
#include "queue2.h"
#include "common.h"
static volatile BaseType_t xSchedulerRunning        = pdFALSE;
static volatile UBaseType_t uxTopReadyPriority      = tskIDLE_PRIORITY;
static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];
static List_t xDelayedTaskList1;                    
static List_t xDelayedTaskList2;                    
static List_t * volatile pxDelayedTaskList;         
static List_t * volatile pxOverflowDelayedTaskList; 
static List_t xPendingReadyList;  
static volatile UBaseType_t uxCurrentNumberOfTasks  = ( UBaseType_t ) 0U;
static volatile TickType_t xTickCount               = ( TickType_t ) 0U;  


static volatile UBaseType_t uxPendedTicks           = ( UBaseType_t ) 0U;
static volatile BaseType_t xYieldPending            = pdFALSE;           
static volatile BaseType_t xNumOfOverflows          = ( BaseType_t ) 0;  
static UBaseType_t uxTaskNumber                     = ( UBaseType_t ) 0U;
static volatile TickType_t xNextTaskUnblockTime     = ( TickType_t ) 0U; 
TCB_t * volatile pxCurrentTCB = NULL;
static void prvInitialiseTCBVariables( TCB_t * const pxTCB, const char * const pcName, UBaseType_t uxPriority, const MemoryRegion_t * const xRegions, const uint16_t usStackDepth ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
UBaseType_t x;

	/* Store the task name in the TCB. */
	for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
	{
		pxTCB->pcTaskName[ x ] = pcName[ x ];

		/* Don't copy all configMAX_TASK_NAME_LEN if the string is shorter than
		configMAX_TASK_NAME_LEN characters just in case the memory after the
		string is not accessible (extremely unlikely). */
		if( pcName[ x ] == 0x00 )
		{
			break;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}

	/* Ensure the name string is terminated in the case that the string length
	was greater or equal to configMAX_TASK_NAME_LEN. */
	pxTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

	/* This is used as an array index so must ensure it's not too large.  First
	remove the privilege bit if one is present. */
	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	pxTCB->uxPriority = uxPriority;
	#if ( configUSE_MUTEXES == 1 )
	{
		pxTCB->uxBasePriority = uxPriority;
		pxTCB->uxMutexesHeld = 0;
	}
	#endif /* configUSE_MUTEXES */

	vListInitialiseItem( &( pxTCB->xGenericListItem ) );
	vListInitialiseItem( &( pxTCB->xEventListItem ) );

	/* Set the pxTCB as a link back from the ListItem_t.  This is so we can get
	back to	the containing TCB from a generic item in a list. */
	listSET_LIST_ITEM_OWNER( &( pxTCB->xGenericListItem ), pxTCB );

	/* Event lists are always in priority order. */
	listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
	listSET_LIST_ITEM_OWNER( &( pxTCB->xEventListItem ), pxTCB );

	#if ( portCRITICAL_NESTING_IN_TCB == 1 )
	{
		pxTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
	}
	#endif /* portCRITICAL_NESTING_IN_TCB */

	#if ( configUSE_APPLICATION_TASK_TAG == 1 )
	{
		pxTCB->pxTaskTag = NULL;
	}
	#endif /* configUSE_APPLICATION_TASK_TAG */

	#if ( configGENERATE_RUN_TIME_STATS == 1 )
	{
		pxTCB->ulRunTimeCounter = 0UL;
	}
	#endif /* configGENERATE_RUN_TIME_STATS */

	#if ( portUSING_MPU_WRAPPERS == 1 )
	{
		vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ), xRegions, pxTCB->pxStack, usStackDepth );
	}
	#else /* portUSING_MPU_WRAPPERS */
	{
		( void ) xRegions;
		( void ) usStackDepth;
	}
	#endif /* portUSING_MPU_WRAPPERS */

	#if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
	{
		for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
		{
			pxTCB->pvThreadLocalStoragePointers[ x ] = NULL;
		}
	}
	#endif

	#if ( configUSE_TASK_NOTIFICATIONS == 1 )
	{
		pxTCB->ulNotifiedValue = 0;
		pxTCB->eNotifyState = eNotWaitingNotification;
	}
	#endif

	#if ( configUSE_NEWLIB_REENTRANT == 1 )
	{
		/* Initialise this task's Newlib reent structure. */
		_REENT_INIT_PTR( ( &( pxTCB->xNewLib_reent ) ) );
	}
	#endif /* configUSE_NEWLIB_REENTRANT */
}
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* Simulate the stack frame as it would be created by a context switch
	interrupt. */
	register int *tp asm("x3");
	pxTopOfStack--;
	*pxTopOfStack = (portSTACK_TYPE)pxCode;			/* Start address */
	pxTopOfStack -= 22;
	*pxTopOfStack = (portSTACK_TYPE)pvParameters;	/* Register a0 */
	pxTopOfStack -= 6;
	*pxTopOfStack = (portSTACK_TYPE)tp; /* Register thread pointer */
	pxTopOfStack -= 3;
	*pxTopOfStack = (portSTACK_TYPE)prvTaskExitError; /* Register ra */
	
	return pxTopOfStack;
}

#define taskSELECT_HIGHEST_PRIORITY_TASK()                                                          \
{                                                                                                   \
    /* Find the highest priority queue that contains ready tasks. */                                \
    while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopReadyPriority ] ) ) )                      \
    {                                                                                               \
        configASSERT( uxTopReadyPriority );                                                         \
        --uxTopReadyPriority;                                                                       \
    }                                                                                               \
                                                                                                    \
    /* listGET_OWNER_OF_NEXT_ENTRY indexes through the list, so the tasks of                        \      
    the same priority get an equal share of the processor time. */                                  \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopReadyPriority ] ) );      \
} /* taskSELECT_HIGHEST_PRIORITY_TASK */                                                                   
                                                                                                           

#define prvAddTaskToReadyList( pxTCB ) vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xGenericListItem )
static TCB_t *prvAllocateTCBAndStack( const uint16_t usStackDepth, StackType_t * const puxStackBuffer )
{
TCB_t *pxNewTCB;

	/* If the stack grows down then allocate the stack then the TCB so the stack
	does not grow into the TCB.  Likewise if the stack grows up then allocate
	the TCB then the stack. */
	#if( portSTACK_GROWTH > 0 )
	{
		/* Allocate space for the TCB.  Where the memory comes from depends on
		the implementation of the port malloc function. */
		pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

		if( pxNewTCB != NULL )
		{
			/* Allocate space for the stack used by the task being created.
			The base of the stack memory stored in the TCB so the task can
			be deleted later if required. */
			pxNewTCB->pxStack = ( StackType_t * ) pvPortMallocAligned( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ), puxStackBuffer ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

			if( pxNewTCB->pxStack == NULL )
			{
				/* Could not allocate the stack.  Delete the allocated TCB. */
				vPortFree( pxNewTCB );
				pxNewTCB = NULL;
			}
		}
	}
	#else /* portSTACK_GROWTH */
	{
	StackType_t *pxStack;

		/* Allocate space for the stack used by the task being created. */
		pxStack = ( StackType_t * ) pvPortMallocAligned( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ), puxStackBuffer ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

		if( pxStack != NULL )
		{
			/* Allocate space for the TCB.  Where the memory comes from depends
			on the implementation of the port malloc function. */
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				/* Store the stack location in the TCB. */
				pxNewTCB->pxStack = pxStack;
			}
			else
			{
				/* The stack cannot be used as the TCB was not created.  Free it
				again. */
				vPortFree( pxStack );
			}
		}
		else
		{
			pxNewTCB = NULL;
		}
	}
	#endif /* portSTACK_GROWTH */

	if( pxNewTCB != NULL )
	{
		/* Avoid dependency on memset() if it is not required. */
		#if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
		{
			/* Just to help debugging. */
			( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) usStackDepth * sizeof( StackType_t ) );
		}
		#endif /* ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) ) */
	}

	return pxNewTCB;
}




BaseType_t xTaskGenericCreate( TaskFunction_t pxTaskCode, const char * const pcName, const uint16_t usStackDepth, 
        void * const pvParameters, UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask, StackType_t * const puxStackBuffer, const MemoryRegion_t * const xRegions ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
BaseType_t xReturn;
TCB_t * pxNewTCB;
StackType_t *pxTopOfStack;

	

	/* Allocate the memory required by the TCB and stack for the new task,
	checking that the allocation was successful. */
	pxNewTCB = prvAllocateTCBAndStack( usStackDepth, puxStackBuffer );
	if( pxNewTCB != NULL )
	{
		#if( portUSING_MPU_WRAPPERS == 1 )
			/* Should the task be created in privileged mode? */
			BaseType_t xRunPrivileged;
			if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
			{
				xRunPrivileged = pdTRUE;
			}
			else
			{
				xRunPrivileged = pdFALSE;
			}
			uxPriority &= ~portPRIVILEGE_BIT;

			if( puxStackBuffer != NULL )
			{
				/* The application provided its own stack.  Note this so no
				attempt is made to delete the stack should that task be
				deleted. */
				pxNewTCB->xUsingStaticallyAllocatedStack = pdTRUE;
			}
			else
			{
				/* The stack was allocated dynamically.  Note this so it can be
				deleted again if the task is deleted. */
				pxNewTCB->xUsingStaticallyAllocatedStack = pdFALSE;
			}
		#endif /* portUSING_MPU_WRAPPERS == 1 */

		/* Calculate the top of stack address.  This depends on whether the
		stack grows from high memory to low (as per the 80x86) or vice versa.
		portSTACK_GROWTH is used to make the result positive or negative as
		required by the port. */
		#if( portSTACK_GROWTH < 0 )
		{
			pxTopOfStack = pxNewTCB->pxStack + ( usStackDepth - ( uint16_t ) 1 );
			pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA exception.  Avoiding casts between pointers and integers is not practical.  Size differences accounted for using portPOINTER_SIZE_TYPE type. */

			/* Check the alignment of the calculated top of stack is correct. */
		}
		#else /* portSTACK_GROWTH */
		{
			pxTopOfStack = pxNewTCB->pxStack;
			/* Check the alignment of the stack buffer is correct. */

			/* If we want to use stack checking on architectures that use
			a positive stack growth direction then we also need to store the
			other extreme of the stack space. */
			pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( usStackDepth - 1 );
		}
		#endif /* portSTACK_GROWTH */

		/* Setup the newly allocated TCB with the initial state of the task. */
		prvInitialiseTCBVariables( pxNewTCB, pcName, uxPriority, xRegions, usStackDepth );

		/* Initialize the TCB stack to look as if the task was already running,
		but had been interrupted by the scheduler.  The return address is set
		to the start of the task function. Once the stack has been initialised
		the	top of stack variable is updated. */
		#if( portUSING_MPU_WRAPPERS == 1 )
		{
			pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
		}
		#else /* portUSING_MPU_WRAPPERS */
		{
			pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
		}
		#endif /* portUSING_MPU_WRAPPERS */

		if( ( void * ) pxCreatedTask != NULL )
		{
			/* Pass the TCB out - in an anonymous way.  The calling function/
			task can use this as a handle to delete the task later if
			required.*/
			*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* Ensure interrupts don't access the task lists while they are being
		updated. */
		taskENTER_CRITICAL();
		{
			uxCurrentNumberOfTasks++;
			if( pxCurrentTCB == NULL )
			{
				/* There are no other tasks, or all the other tasks are in
				the suspended state - make this the current task. */
				pxCurrentTCB =  pxNewTCB;

				if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
				{
					/* This is the first task to be created so do the preliminary
					initialisation required.  We will not recover if this call
					fails, but we will report the failure. */
					prvInitialiseTaskLists();
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				/* If the scheduler is not already running, make this task the
				current task if it is the highest priority task to be created
				so far. */
				if( xSchedulerRunning == pdFALSE )
				{
					if( pxCurrentTCB->uxPriority <= uxPriority )
					{
						pxCurrentTCB = pxNewTCB;
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

			uxTaskNumber++;

			#if ( configUSE_TRACE_FACILITY == 1 )
			{
				/* Add a counter into the TCB for tracing only. */
				pxNewTCB->uxTCBNumber = uxTaskNumber;
			}
			#endif /* configUSE_TRACE_FACILITY */
			traceTASK_CREATE( pxNewTCB );

			prvAddTaskToReadyList(pxNewTCB);

			xReturn = pdPASS;
			portSETUP_TCB(pxNewTCB);
		}
		taskEXIT_CRITICAL();
	}
	else
	{
		xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
		traceTASK_CREATE_FAILED();
	}

	if( xReturn == pdPASS )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			/* If the created task is of a higher priority than the current task
			then it should run now. */
			if( pxCurrentTCB->uxPriority < uxPriority )
			{
				taskYIELD_IF_USING_PREEMPTION();
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

	return xReturn;
}



void vTaskSwitchContext( void )
{
	if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
	{
		/* The scheduler is currently suspended - do not allow a context
		switch. */
		xYieldPending = pdTRUE;
	}
	else
	{
		xYieldPending = pdFALSE;
		traceTASK_SWITCHED_OUT();

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
				#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
					portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
				#else
					ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
				#endif

				/* Add the amount of time the task has been running to the
				accumulated	time so far.  The time the task started running was
				stored in ulTaskSwitchedInTime.  Note that there is no overflow
				protection here	so count values are only valid until the timer
				overflows.  The guard against negative values is to protect
				against suspect run time stat counter implementations - which
				are provided by the application, not the kernel. */
				if( ulTotalRunTime > ulTaskSwitchedInTime )
				{
					pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				ulTaskSwitchedInTime = ulTotalRunTime;
		}
		#endif /* configGENERATE_RUN_TIME_STATS */

		/* Check for stack overflow, if configured. */
		taskCHECK_FOR_STACK_OVERFLOW();

		/* Select a new task to run using either the generic C or port
		optimised asm code. */
		taskSELECT_HIGHEST_PRIORITY_TASK();
		traceTASK_SWITCHED_IN();

		#if ( configUSE_NEWLIB_REENTRANT == 1 )
		{
			/* Switch Newlib's _impure_ptr variable to point to the _reent
			structure specific to this task. */
			_impure_ptr = &( pxCurrentTCB->xNewLib_reent );
		}
		#endif /* configUSE_NEWLIB_REENTRANT */
	}
}

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )    
{                                                                          
TCB_t *pxUnblockedTCB;                                                     
BaseType_t xReturn;                                                                                                                           
	pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
	( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	{
		( void ) uxListRemove( &( pxUnblockedTCB->xGenericListItem ) ); 
		prvAddTaskToReadyList( pxUnblockedTCB );                        

	}
	else                                                                                 
	{                                                                                    
	    /* The delayed and ready lists cannot be accessed, so hold this task             
	    pending until the scheduler is resumed. */                                       
	    vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );   
	}                                                                                    
	if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )                       
	{                                                                                 
	    /* Return true if the task removed from the event list has a higher           
	    priority than the calling task.  This allows the calling task to know if      
	    it should force a context switch now. */                                      
	    xReturn = pdTRUE;                                                             
	                                                                                  
	    /* Mark that a yield is pending in case the user is not using the             
	    "xHigherPriorityTaskWoken" parameter to an ISR safe FreeRTOS function. */     
	    xYieldPending = pdTRUE;                                                       
	}                                                                                 
	else                                                                              
	{                                                                                 
	    xReturn = pdFALSE;                                                            
	}                                                                                 
	return xReturn;
}

BaseType_t xTaskGetSchedulerState( void )                    
{                                                            
BaseType_t xReturn;                                          
                                                             
    if( xSchedulerRunning == pdFALSE )                       
    {                                                        
        xReturn = taskSCHEDULER_NOT_STARTED;                 
    }                                                        
    else                                                     
    {                                                        
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {                                                    
            xReturn = taskSCHEDULER_RUNNING;                 
        }                                                    
        else                                                 
        {                                                    
            xReturn = taskSCHEDULER_SUSPENDED;               
        }                                                    
    }                                                        
                                                             
    return xReturn;                                          
}                                                            
void vTaskStartScheduler( void ) 
{
	BaseType_t xReturn;
	if( xReturn == pdPASS )                
	{                                      
	    xReturn = xTimerCreateTimerTask(); 
	}                                      
	if( xPortStartScheduler() != pdFALSE )                           
	{                                                                
	    /* Should not reach here as if the scheduler is running the  
	    function will not return. */                                 
	}                                                                

}

void vTaskSuspendAll( void )
 {
 	++uxSchedulerSuspended;
 }
void vTaskEnterCritical( void )
	{
		portDISABLE_INTERRUPTS();

		if( xSchedulerRunning != pdFALSE )
		{
			( pxCurrentTCB->uxCriticalNesting )++;

			/* This is not the interrupt safe version of the enter critical
			function so	assert() if it is being called from an interrupt
			context.  Only API functions that end in "FromISR" can be used in an
			interrupt.  Only assert if the critical nesting count is 1 to
			protect against recursive calls if the assert function also uses a
			critical section. */
			if( pxCurrentTCB->uxCriticalNesting == 1 )
			{
				portASSERT_IF_IN_ISR();
			}
		}
		else
		{
			
		}
	}
	void vTaskExitCritical( void )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			if( pxCurrentTCB->uxCriticalNesting > 0U )
			{
				( pxCurrentTCB->uxCriticalNesting )--;

				if( pxCurrentTCB->uxCriticalNesting == 0U )
				{
					portENABLE_INTERRUPTS();
				}
				else
				{
					
				}
			}
			else
			{
				
			}
		}
		else
		{
			
		}
	}