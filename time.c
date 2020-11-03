#include "time.h"
#include "common.h"
#include "task.h"
#include "encoding.h"
static List_t xActiveTimerList1;   
static List_t xActiveTimerList2;   
static List_t *pxCurrentTimerList; 
static List_t *pxOverflowTimerList;

static QueueHandle_t xTimerQueue = NULL;
#define tmrNO_DELAY		( TickType_t ) 0U

typedef struct tmrTimerParameters      
{                                      
    TickType_t          xMessageValue; 
    Timer_t *           pxTimer;       
} TimerParameter_t;                    

typedef struct tmrTimerQueueMessage                                                                   
{                                                                                                     
    BaseType_t          xMessageID;         /*<< The command being sent to the timer service task. */ 
    union                                                                                             
    {                                                                                                 
        TimerParameter_t xTimerParameters;                                                            
                                                                                                      
        /* Don't include xCallbackParameters if it is not going to be used as                         
        it makes the structure (and therefore the timer queue) larger. */                             
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )                                                   
            CallbackParameters_t xCallbackParameters;                                                 
        #endif /* INCLUDE_xTimerPendFunctionCall */                                                   
    } u;                                                                                              
} DaemonTaskMessage_t;             

static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime );                                                                   

static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow );

static void prvSwitchTimerLists( void );
static void prvProcessReceivedCommands( void );
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ); 
static void prvSetNextTimerInterrupt(void)
{
	__asm volatile("ld t0,0(%0)"::"r"mtimecmp);
	__asm volatile("add t0,t0,%0" :: "r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));
	__asm volatile("sd t0,0(%0)"::"r"mtimecmp);
}

/* Sets and enable the timer interrupt */                                           
void vPortSetupTimer(void)                                                          
{                                                                                
  __asm volatile("ld t0,0(%0)"::"r"mtime);
  __asm volatile("add t0,t0,%0"::"r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));  
  __asm volatile("sd t0,0(%0)"::"r"mtimecmp);
                                                                                   
   /* Enable timer interupt */                                                     
   __asm volatile("csrs mie,%0"::"r"(0x80));                                       
}

void vPortSysTickHandler( void )              
{                                             
    prvSetNextTimerInterrupt();               
                                              
    /* Increment the RTOS tick. */            
    if( xTaskIncrementTick() != pdFALSE )     
    {                                         
        vTaskSwitchContext();                 
    }                                         
}  
static void prvCheckForValidListAndQueue( void )
{
  if( xTimerQueue == NULL )                                                                                 
  {                                                                                                         
      vListInitialise( &xActiveTimerList1 );                                                                
      vListInitialise( &xActiveTimerList2 );                                                                
      pxCurrentTimerList = &xActiveTimerList1;                                                              
      pxOverflowTimerList = &xActiveTimerList2;                                                             
      xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );                                                                                                                                                                                                                                       
      {                                                                                                     
          if( xTimerQueue != NULL )                                                                         
          {                                                                                                 
              vQueueAddToRegistry( xTimerQueue, "TmrQ" );                                                   
          }                                                                                                                                                                                            
      }                                                                                                                                                                   
  }                                                                                                                                                                                                                
}                                           
TimerHandle_t xTimerCreate( const char * const pcTimerName, const TickType_t xTimerPeriodInTicks, const UBaseType_t uxAutoReload, void * const pvTimerID, TimerCallbackFunction_t pxCallbackFunction )
{
  Timer_t *pxNewTimer;
  if( xTimerPeriodInTicks == ( TickType_t ) 0U )
  {                                             
      pxNewTimer = NULL;                        
  }                                             
  else                                                                                
  {                                                                                   
      pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );                   
      if( pxNewTimer != NULL )                                                        
      {                                                                               
          /* Ensure the infrastructure used by the timer service task has been        
          created/initialised. */                                                     
          prvCheckForValidListAndQueue();                                             
                                                                                      
          /* Initialise the timer structure members using the function parameters. */ 
          pxNewTimer->pcTimerName = pcTimerName;                                      
          pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks;                      
          pxNewTimer->uxAutoReload = uxAutoReload;                                    
          pxNewTimer->pvTimerID = pvTimerID;                                          
          pxNewTimer->pxCallbackFunction = pxCallbackFunction;                        
          vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );                     
                                                                                      
          //traceTIMER_CREATE( pxNewTimer );                                            
      }                                                                               
      else                                                                            
      {                                                                               
          //traceTIMER_CREATE_FAILED();                                                 
      }                                                                               
  }
  return ( TimerHandle_t ) pxNewTimer;                                                                                   

}

static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
TickType_t xNextExpireTime;

	/* Timers are listed in expiry time order, with the head of the list
	referencing the task that will expire first.  Obtain the time at which
	the timer with the nearest expiry time will expire.  If there are no
	active timers then just set the next expire time to 0.  That will cause
	this task to unblock when the tick count overflows, at which point the
	timer lists will be switched and the next expiry time can be
	re-assessed.  */
	*pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
	if( *pxListWasEmpty == pdFALSE )
	{
		xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
	}
	else
	{
		/* Ensure the task unblocks when the tick count rolls over. */
		xNextExpireTime = ( TickType_t ) 0U;
	}

	return xNextExpireTime;
}

static void prvTimerTask( void *pvParameters )
{
	TickType_t xNextExpireTime;
	BaseType_t xListWasEmpty;
	for( ;; )
	{
		/* Query the timers list to see if it contains any timers, and if so,
		obtain the time at which the next timer will expire. */
		xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

		/* If a timer has expired, process it.  Otherwise, block this task
		until either a timer does expire, or a command is received. */
		prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

		/* Empty the command queue. */
		prvProcessReceivedCommands();
	}
}


static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )                                           
{                                                                                                                           
TickType_t xTimeNow;                                                                                                        
 static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 Variable is only accessible to one task. */   
                                                                                                                            
    xTimeNow = xTaskGetTickCount();                                                                                         
                                                                                                                            
    if( xTimeNow < xLastTime )                                                                                              
    {                                                                                                                       
        prvSwitchTimerLists();                                                                                              
        *pxTimerListsWereSwitched = pdTRUE;                                                                                 
    }                                                                                                                       
    else                                                                                                                    
    {                                                                                                                       
        *pxTimerListsWereSwitched = pdFALSE;                                                                                
    }                                                                                                                       
                                                                                                                            
    xLastTime = xTimeNow;                                                                                                   
                                                                                                                            
    return xTimeNow;                                                                                                        
}                                                                                                                           


static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) 
{                                                                                                    
	TickType_t xTimeNow;                                                                                 
  	BaseType_t xTimerListsWereSwitched;                                                                  
	xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
	if( xTimerListsWereSwitched == pdFALSE )
	{
	  /* The tick count has not overflowed, has the timer expired? */                                   
  		if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )                             
  		{                                                                                                 
      			( void ) xTaskResumeAll();                                                                    
      			prvProcessExpiredTimer( xNextExpireTime, xTimeNow );                                          
  		}                                                                                                 
  		else                                                                                              
  		{                                                                                                 
      			if( xListWasEmpty != pdFALSE )                                                                
      			{                                                                                             
          			xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );                                 
      			}                                                                                             
                                                                                                    
      			vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty ); 
                                                                                                    
      			if( xTaskResumeAll() == pdFALSE )                                                             
      			{	                                                                                             
          			portYIELD_WITHIN_API();                                                                   
      			}                                                                                             
		}
	}
}




BaseType_t xTimerGenericCommand( TimerHandle_t xTimer, const BaseType_t xCommandID, const TickType_t xOptionalValue, BaseType_t * const pxHigherPriorityTaskWoken, const TickType_t xTicksToWait ) 
{ 
  BaseType_t xReturn = pdFAIL;                                                                                                                                                                                                 
  DaemonTaskMessage_t xMessage;
  if( xTimerQueue != NULL )                                                                      
  {                                                                                              
      /* Send a command to the timer service task to start the xTimer timer. */                  
      xMessage.xMessageID = xCommandID;                                                          
      xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;                                
      xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;                                
                                                                                                 
      if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )                                               
      {                                                                                          
          if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )                                
          {                                                                                      
              xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );                
          }                                                                                      
          else                                                                                   
          {                                                                                      
              xReturn = xQueueSendToBack( xTimerQueue, &xMessage, 0 );                 
          }                                                                                      
      }                                                                                          
      else                                                                                       
      {                                                                                          
          xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
      }                                                                                                                                                                                                        
  }    
  return      xReturn;                                                                                                                                                             
}
BaseType_t xTimerCreateTimerTask( void )
{ 

    BaseType_t xReturn = pdFAIL;            
    prvCheckForValidListAndQueue();
    if( xTimerQueue != NULL )
    {
      xReturn = xTaskCreate( prvTimerTask, "Tmr Svc", ( uint16_t ) configTIMER_TASK_STACK_DEPTH, NULL, ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, NULL);
    }
    return xReturn;
}




static void prvProcessReceivedCommands( void )
{
DaemonTaskMessage_t xMessage;
Timer_t *pxTimer;
BaseType_t xTimerListsWereSwitched, xResult;
TickType_t xTimeNow;

    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) 
    {
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
        }
		#endif
	if( xMessage.xMessageID >= ( BaseType_t ) 0 )
	{
		pxTimer = xMessage.u.xTimerParameters.pxTimer;

		if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
		{
			( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
		}
		xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

		switch( xMessage.xMessageID )
			{
				case tmrCOMMAND_START :
			    case tmrCOMMAND_START_FROM_ISR :
			    case tmrCOMMAND_RESET :
			    case tmrCOMMAND_RESET_FROM_ISR :
				case tmrCOMMAND_START_DONT_TRACE :
					if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) == pdTRUE )
					{
						pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

						if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
						{
							xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
							( void ) xResult;
						}

					}
					break;

				case tmrCOMMAND_STOP :
				case tmrCOMMAND_STOP_FROM_ISR :
					break;

				case tmrCOMMAND_CHANGE_PERIOD :
				case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
					pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;

					( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
					break;
					case tmrCOMMAND_DELETE :
					vPortFree( pxTimer );
					break;

				default	:
					break;
			}
		}
	}
}

static void prvSwitchTimerLists( void )
{
	TickType_t xNextExpireTime, xReloadTime;
	List_t *pxTemp;
	Timer_t *pxTimer;
	BaseType_t xResult;                      
	while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )                   
	{    
    	xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList ); 
     
    /* Remove the timer from the list. */                                     
    	pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
    	( void ) uxListRemove( &( pxTimer->xTimerListItem ) );                    
    	pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
    	if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )                                                              
		{                                                                                                                  
	                                                                           
		    xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );                                              
		    if( xReloadTime > xNextExpireTime )                                                                            
		    {                                                                                                              
		        listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );                                      
		        listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );                                          
		        vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );                                           
		    }                                                                                                              
		    else                                                                                                           
		    {                                                                                                              
		        xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );                                                                                   
		        ( void ) xResult;                                                                                          
		    }                                                                                                              
		} 
	}
	pxTemp = pxCurrentTimerList;   
	pxCurrentTimerList = pxOverflowTimerList;
	pxOverflowTimerList = pxTemp;       
}




static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )                                           
{                     
BaseType_t xResult;   
Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );                                                  
                      
    /* Remove the timer from the list of active timers.  A check has already                                                                
    been performed to ensure the list is not empty. */                                                                                      
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );                                                                                                                                                                                           
                      
    /* If the timer is an auto reload timer then calculate the next                                                                         
    expiry time and re-insert the timer in the list of active timers. */                                                                    
    if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )                                                                                   
    {                 
        /* The timer is inserted into a list using a time relative to anything                                                              
        other than the current time.  It will therefore be inserted into the                                                                
        correct list relative to the time this task thinks it is now. */                                                                    
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) == pdTRUE )
        {             
            /* The timer expired before it was added to the active timer                                                                    
            list.  Reload it now.  */                                                                                                       
            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );                     
                                                                                                               
        }                   
    }                             
    /* Call the timer callback. */                                                                                                          
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );                                                                               
}                     
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) 
{                                                                          
	BaseType_t xProcessTimerNow = pdFALSE;                                     
                                                                           
    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );      
                                                                           
    if( xNextExpiryTime <= xTimeNow )                                      
    {                                                                      
        /* Has the expiry time elapsed between the command to start/reset a
        timer was issued, and the time the command was processed? */       
        if( ( xTimeNow - xCommandTime ) >= pxTimer->xTimerPeriodInTicks )  
        {                                                                  
            /* The time between a command being issued and the command being
            processed actually exceeds the timers period.  */              
            xProcessTimerNow = pdTRUE;                                     
        }                                                                                                                                                           
        else                                                               
        {                                                                  
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }                                                                  
    }                                                                      
    else                                                                   
    {                                                                      
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) ) 
        {                                                                  
            /* If, since the command was issued, the tick count has overflowed
            but the expiry time has not, then the timer must have already passed
            its expiry time and should be processed immediately. */        
            xProcessTimerNow = pdTRUE;                                     
        }                                                                  
        else                                                               
        {                                                                  
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }                                                                  
    }                                                                      
                                                                           
    return xProcessTimerNow;                                               
}                                                                          
