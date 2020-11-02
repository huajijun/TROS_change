#include "task.h"
#include "list2.h"
#include "queue2.h"
#include "common.h"
#include "other.h"
#define portBYTE_ALIGNMENT_MASK (0x001f)
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
static void prvInitialiseTaskLists( void );


static List_t xSuspendedTaskList;

static volatile UBaseType_t uxPendedTicks           = ( UBaseType_t ) 0U;
static volatile BaseType_t xYieldPending            = pdFALSE;           
static volatile BaseType_t xNumOfOverflows          = ( BaseType_t ) 0;  
static UBaseType_t uxTaskNumber                     = ( UBaseType_t ) 0U;
static volatile TickType_t xNextTaskUnblockTime     = ( TickType_t ) 0U; 
TCB_t * volatile pxCurrentTCB = NULL;  


TickType_t xTaskGetTickCount( void );
#define prvAddTaskToReadyList( pxTCB ) vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xGenericListItem ))
BaseType_t xTaskIncrementTick( void );

#define taskSWITCH_DELAYED_LISTS()                                                                  \
{                                                                                                   \
    List_t *pxTemp;                                                                                 \
    pxTemp = pxDelayedTaskList;                                                                     \
    pxDelayedTaskList = pxOverflowDelayedTaskList;                                                  \
    pxOverflowDelayedTaskList = pxTemp;                                                             \
    xNumOfOverflows++;                                                                              \
    prvResetNextTaskUnblockTime();                                                                  \
}       

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
	for(;;)
	{
		if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 ) 
		{                                                                                               
		    taskYIELD();                                                                                
		}                                                                                               
	}
}


TickType_t xTaskGetTickCount( void )
{
	TickType_t xTicks;

	//portTICK_TYPE_ENTER_CRITICAL();
	{
		xTicks = xTickCount;
	}
	//portTICK_TYPE_EXIT_CRITICAL();

	return xTicks;
}


static void prvResetNextTaskUnblockTime( void )
{                                                                                          
TCB_t *pxTCB;
   
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        /* The new current delayed list is empty.  Set xNextTaskUnblockTime to
        the maximum possible value so it is extremely unlikely that the
        if( xTickCount >= xNextTaskUnblockTime ) test will pass until
        there is an item in the delayed list. */
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        /* The new current delayed list is not empty, get the value of
        the item at the head of the delayed list.  This is the time at
        which the task at the head of the delayed list should be removed
        from the Blocked state. */
        ( pxTCB ) = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( ( pxTCB )->xGenericListItem ) );
    }
}  
                                                                                             

BaseType_t xTaskResumeAll( void )          
{                                          
	TCB_t *pxTCB;                              
	BaseType_t xAlreadyYielded = pdFALSE;
	taskENTER_CRITICAL();                                                                     
	{                                                                                         
	    --uxSchedulerSuspended;
	                                                                                          
	    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	    {
	        if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
	        {
	            /* Move any readied tasks from the pending list into the
	            appropriate ready list. */
	            while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
	            {
	                pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
	                ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
	                ( void ) uxListRemove( &( pxTCB->xGenericListItem ) );
	                prvAddTaskToReadyList( pxTCB );
	                                                                                          
	                /* If the moved task has a priority higher than the current
	                task then a yield must be performed. */
	                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
	                {
	                    xYieldPending = pdTRUE;
	                }
	            }
	                                                                                          
	            if( uxPendedTicks > ( UBaseType_t ) 0U )
	            {
	                while( uxPendedTicks > ( UBaseType_t ) 0U )
	                {
	                    if( xTaskIncrementTick() != pdFALSE )
	                    {
	                        xYieldPending = pdTRUE;
	                    }
	                    --uxPendedTicks;
	                }
	            }                                              
	            if( xYieldPending == pdTRUE )                        
	            {   
	                #if( configUSE_PREEMPTION != 0 )                 
	                {                                                
	                    xAlreadyYielded = pdTRUE;                     
	                }                                                
	                #endif                                           
	                taskYIELD_IF_USING_PREEMPTION();                 
	            }   
 
	        }       
	    }           
        
	}               
	taskEXIT_CRITICAL();                                             
	                
	return xAlreadyYielded;                                          
                                             
}
void prvTaskExitError(void)
{
    for(;;);
}
static void prvInitialiseTCBVariables( TCB_t * const pxTCB, const char * const pcName, UBaseType_t uxPriority, const MemoryRegion_t * const xRegions, const uint16_t usStackDepth ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
	UBaseType_t x;

	for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
	{
		pxTCB->pcTaskName[ x ] = pcName[ x ];

		if( pcName[ x ] == 0x00 )
		{
			break;
		}
	}

	pxTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

	if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
	{
		uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
	}

	pxTCB->uxPriority = uxPriority;
	#if ( configUSE_MUTEXES == 1 )
	{
		pxTCB->uxBasePriority = uxPriority;
		pxTCB->uxMutexesHeld = 0;
	}
	#endif

	vListInitialiseItem( &( pxTCB->xGenericListItem ) );
	vListInitialiseItem( &( pxTCB->xEventListItem ) );

	listSET_LIST_ITEM_OWNER( &( pxTCB->xGenericListItem ), pxTCB );

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
    while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopReadyPriority ] ) ) )                      \
    {                                                                                               \
        --uxTopReadyPriority;                                                                       \
    }                                                                                               \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopReadyPriority ] ) );      \
} /* taskSELECT_HIGHEST_PRIORITY_TASK */                                                                   
                                                                                                           


static TCB_t *prvAllocateTCBAndStack( const uint16_t usStackDepth, StackType_t * const puxStackBuffer )
{
TCB_t *pxNewTCB;

	#if( portSTACK_GROWTH > 0 )
	{
		pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

		if( pxNewTCB != NULL )
		{
			pxNewTCB->pxStack = ( StackType_t * ) pvPortMallocAligned( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ), puxStackBuffer ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

			if( pxNewTCB->pxStack == NULL )
			{
				vPortFree( pxNewTCB );
				pxNewTCB = NULL;
			}
		}
	}
	#else /* portSTACK_GROWTH */
	{
		StackType_t *pxStack;

		pxStack = ( StackType_t * ) pvPortMallocAligned( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ), puxStackBuffer ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */

		if( pxStack != NULL )
		{
			pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

			if( pxNewTCB != NULL )
			{
				pxNewTCB->pxStack = pxStack;
			}
			else
			{
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
		#if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
		{
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

	

	pxNewTCB = prvAllocateTCBAndStack( usStackDepth, puxStackBuffer );
	if( pxNewTCB != NULL )
	{
		#if( portUSING_MPU_WRAPPERS == 1 )
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
				pxNewTCB->xUsingStaticallyAllocatedStack = pdTRUE;
			}
			else
			{
				pxNewTCB->xUsingStaticallyAllocatedStack = pdFALSE;
			}
		#endif /* portUSING_MPU_WRAPPERS == 1 */

		#if( portSTACK_GROWTH < 0 )
		{
			pxTopOfStack = pxNewTCB->pxStack + ( usStackDepth - ( uint16_t ) 1 );
			pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA exception.  Avoiding casts between pointers and integers is not practical.  Size differences accounted for using portPOINTER_SIZE_TYPE type. */

		}
		#else /* portSTACK_GROWTH */
		{
			pxTopOfStack = pxNewTCB->pxStack;

			pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( usStackDepth - 1 );
		}
		#endif /* portSTACK_GROWTH */

		prvInitialiseTCBVariables( pxNewTCB, pcName, uxPriority, xRegions, usStackDepth );

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
			*pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
		}

		taskENTER_CRITICAL();
		{
			uxCurrentNumberOfTasks++;
			if( pxCurrentTCB == NULL )
			{
				pxCurrentTCB =  pxNewTCB;

				if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
				{
					prvInitialiseTaskLists();
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
				}
			}

			uxTaskNumber++;

			#if ( configUSE_TRACE_FACILITY == 1 )
			{
				/* Add a counter into the TCB for tracing only. */
				pxNewTCB->uxTCBNumber = uxTaskNumber;
			}
			#endif /* configUSE_TRACE_FACILITY */

			prvAddTaskToReadyList(pxNewTCB);

			xReturn = pdPASS;
		}
		taskEXIT_CRITICAL();
	}
	else
	{
		xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
	}

	if( xReturn == pdPASS )
	{
		if( xSchedulerRunning != pdFALSE )
		{
			/* If the created task is of a higher priority than the current task
			then it should run now. */
		//	if( pxCurrentTCB->uxPriority < uxPriority )
			{
				taskYIELD_IF_USING_PREEMPTION();
			}
		}
	}

	return xReturn;
}



void vTaskSwitchContext( void )
{
	if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
	{
		xYieldPending = pdTRUE;
	}
	else
	{
		xYieldPending = pdFALSE;

		#if ( configGENERATE_RUN_TIME_STATS == 1 )
		{
				#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
					portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
				#else
					ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
				#endif

				if( ulTotalRunTime > ulTaskSwitchedInTime )
				{
					pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
				}
				ulTaskSwitchedInTime = ulTotalRunTime;
		}
		#endif /* configGENERATE_RUN_TIME_STATS */

		/* Check for stack overflow, if configured. */
		taskCHECK_FOR_STACK_OVERFLOW();

		/* Select a new task to run using either the generic C or port
		optimised asm code. */
		taskSELECT_HIGHEST_PRIORITY_TASK();
		//traceTASK_SWITCHED_IN();

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
				//portASSERT_IF_IN_ISR();
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
			}
		}
			
	}
void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut ) 
{                                                                                 
    pxTimeOut->xOverflowCount = xNumOfOverflows;         
    pxTimeOut->xTimeOnEntering = xTickCount;             
}                                                        

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )   
{
	BaseType_t xReturn;
	const TickType_t xConstTickCount = xTickCount; 
	if( *pxTicksToWait == portMAX_DELAY )
	{                                    
	    xReturn = pdFALSE;               
	}                                    
	if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) )
	{                                                                                                          
		    xReturn = pdTRUE;                                                       
	}                                                                           
	else if( ( xConstTickCount - pxTimeOut->xTimeOnEntering ) < *pxTicksToWait )
	{                                                                           
		      
		*pxTicksToWait -= ( xConstTickCount -  pxTimeOut->xTimeOnEntering );    
		vTaskSetTimeOutState( pxTimeOut );                                      
		xReturn = pdFALSE;                                                      
	}                                                                           
	else                                                                        
	{                                                                           
		xReturn = pdTRUE;                                                       
	}                                                                           
	return xReturn;                                                        
}
static void prvAddCurrentTaskToDelayedList( const TickType_t xTimeToWake )                 
{                                                                                          
    /* The list item will be inserted in wake time order. */                               
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xGenericListItem ), xTimeToWake );           
                                                                                           
    if( xTimeToWake < xTickCount )                                                         
    {                                                                                      
        /* Wake time has overflowed.  Place this item in the overflow list. */             
        vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xGenericListItem ) );     
    }                                                                                      
    else                                                                                   
    {                                                                                      
        /* The wake time has not overflowed, so the current block list is used. */         
        vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xGenericListItem ) );             
                                                                                           
        /* If the task entering the blocked state was placed at the head of the            
        list of blocked tasks then xNextTaskUnblockTime needs to be updated                
        too. */                                                                            
        if( xTimeToWake < xNextTaskUnblockTime )                                           
        {                                                                                  
            xNextTaskUnblockTime = xTimeToWake;                                            
        }                                                                                                                                                                
    }                                                                                      
}                                                                                          

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait ) 
{                                                                                       
	TickType_t xTimeToWake;                                                                 
	vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );
	if( uxListRemove( &( pxCurrentTCB->xGenericListItem ) ) == ( UBaseType_t ) 0 ) 
	{                                                                              
	    /* The current task must be in a ready list, so there is no need to        
	    check, and the port reset macro can be called directly. */                   
	}
	if( xTicksToWait == portMAX_DELAY )                                               
	{                                                                                 
	    /* Add the task to the suspended task list instead of a delayed task          
	    list to ensure the task is not woken by a timing event.  It will              
	    block indefinitely. */                                                        
	    vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xGenericListItem ) );   
	}                                                                                 
	else                                                                              
	{                                                                                 
	    /* Calculate the time at which the task should be woken if the event          
	    does not occur.  This may overflow but this doesn't matter, the               
	    scheduler will handle it. */                                                  
	    xTimeToWake = xTickCount + xTicksToWait;                                      
	    prvAddCurrentTaskToDelayedList( xTimeToWake );                                
	}                                                                                 
                                                                              
}                                                                     


BaseType_t xTaskIncrementTick( void ) 
{                                     
	TCB_t * pxTCB;                        
	TickType_t xItemValue;                
	BaseType_t xSwitchRequired = pdFALSE; 
	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
	{                                                    
		++xTickCount;
		             
		{            
			const TickType_t xConstTickCount = xTickCount;
			                                              
			if( xConstTickCount == ( TickType_t ) 0U )    
			{                                             
			    taskSWITCH_DELAYED_LISTS();               
			}                                             
			if( xConstTickCount >= xNextTaskUnblockTime )                      
			{                                                                  
			    for( ;; )                                                      
			    {                                                              
			        if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )    
			        {                                                                                          
			            xNextTaskUnblockTime = portMAX_DELAY;                  
			            break;                                                 
			        } 
			        else                                                                     
			        {                                                                        
                               
			            pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
			            xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xGenericListItem ) );
			                                                                                 
			            if( xConstTickCount < xItemValue )                                   
			            {                                                                                                          
			                xNextTaskUnblockTime = xItemValue;                               
			                break;                                                           
			            }                                                                    
			            ( void ) uxListRemove( &( pxTCB->xGenericListItem ) );             
			                                                                               
			            /* Is the task waiting on an event also?  If so remove             
			            it from the event list. */                                         
			            if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
			            {                                                                  
			                ( void ) uxListRemove( &( pxTCB->xEventListItem ) );           
			            }                                                                  
			            else                                                               
			            {                                                                  
			                //mtCOVERAGE_TEST_MARKER();                                      
			            }                                                                  
			                                                                               
			            /* Place the unblocked task into the appropriate ready             
			            list. */                                                           
			            prvAddTaskToReadyList( pxTCB );                                    
			                                                                               
			            /* A task being unblocked cannot cause an immediate                
			            context switch if preemption is turned off. */                     
			            {                                                       
			                /* Preemption is on, but a context switch should    
			                only be performed if the unblocked task has a       
			                priority that is equal to or higher than the        
			                currently executing task. */                        
			                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority ) 
			                {                                                   
			                    xSwitchRequired = pdTRUE;                       
			                }                                                   
			                else                                                
			                {                                                   
			                    //mtCOVERAGE_TEST_MARKER();                       
			                }                                                   
			            }
			        }    
			    }
			}                                                   
        }


        if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
        {                                                                                                      
            xSwitchRequired = pdTRUE;                                                                          
        }                                                                                                      
                                                  
    }
    else
    {
    	++uxPendedTicks;
    	if( xYieldPending != pdFALSE )
    	{                             
    	    xSwitchRequired = pdTRUE; 
    	}                             

    } 
    return xSwitchRequired;                                             
}
static void prvInitialiseTaskLists( void )                                                                 
{                                                                                                          
	UBaseType_t uxPriority;                                                                                    
                                                                                                           
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {                                                                                                      
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );                                           
    }                                                                                                      
                                                                                                           
    vListInitialise( &xDelayedTaskList1 );                                                                 
    vListInitialise( &xDelayedTaskList2 );                                                                 
    vListInitialise( &xPendingReadyList );                                                                 
                                                                                                           
    #if ( INCLUDE_vTaskDelete == 1 )                                                                       
    {                                                                                                      
        vListInitialise( &xTasksWaitingTermination );                                                      
    }                                                                                                      
    #endif /* INCLUDE_vTaskDelete */                                                                       
                                                                                                           
    #if ( INCLUDE_vTaskSuspend == 1 )                                                                      
    {                                                                                                      
        vListInitialise( &xSuspendedTaskList );                                                            
    }                                                                                                      
    #endif /* INCLUDE_vTaskSuspend */                                                                      
                                                                                                           
    /* Start with pxDelayedTaskList using list1 and the pxOverflowDelayedTaskList                          
    using list2. */                                                                                        
    pxDelayedTaskList = &xDelayedTaskList1;                                                                
    pxOverflowDelayedTaskList = &xDelayedTaskList2;                                                        
}                                                                                                          
void SetRunning(void)
{
    xSchedulerRunning++;

}


void vTaskStartScheduler( void )   
{                                  
	BaseType_t xReturn;      
	xReturn = xTaskCreate( prvIdleTask, "IDLE", tskIDLE_STACK_SIZE, ( void * ) NULL, ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ), NULL );     
	if( xReturn == pdPASS )
	{                                     
	    xReturn = xTimerCreateTimerTask();
	}                                     
		if( xReturn == pdPASS )
		{                      
			portDISABLE_INTERRUPTS(); 
			xNextTaskUnblockTime = portMAX_DELAY; 
			xSchedulerRunning = pdTRUE;           
			xTickCount = ( TickType_t ) 0U;       
			if( xPortStartScheduler() != pdFALSE )                                 
			{                                                                      
			    /* Should not reach here as if the scheduler is running the        
			    function will not return. */                                       
			}                                                                      
			else                                                                   
			{                                                                      
			    /* Should only reach here if a task calls xTaskEndScheduler(). */  
			}       
		}                                                               
}                        


