#include "time.h"
#include "common.h"
static List_t xActiveTimerList1;   
static List_t xActiveTimerList2;   
static List_t *pxCurrentTimerList; 
static List_t *pxOverflowTimerList;

static QueueHandle_t xTimerQueue = NULL;

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



static void prvSetNextTimerInterrupt(void)
{
	__asm volatile("csrr t0,mtimecmp");
	__asm volatile("add t0,t0,%0" :: "r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));
	__asm volatile("csrw mtimecmp,t0");
}

/* Sets and enable the timer interrupt */                                           
void vPortSetupTimer(void)                                                          
{                                                                                
  __asm volatile("csrr t0,mtime");                                                
  __asm volatile("add t0,t0,%0"::"r"(configTICK_CLOCK_HZ / configTICK_RATE_HZ));  
  __asm volatile("csrw mtimecmp,t0");                                             
                                                                                   
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
                                                                                      
          traceTIMER_CREATE( pxNewTimer );                                            
      }                                                                               
      else                                                                            
      {                                                                               
          traceTIMER_CREATE_FAILED();                                                 
      }                                                                               
  }
  return ( TimerHandle_t ) pxNewTimer;                                                                                   

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
              xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );                 
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