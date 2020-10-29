#include "common.h"
#include "list2.h"
#define CSR_MIE             (0x304)
#define CSR_MTVEC           (0x305)
#define mtimecmp	    (0x2004000)
#define mtime               (0x200bff8)

#define configTICK_CLOCK_HZ         ( ( unsigned long ) 1000000 ) 
#define configTICK_RATE_HZ          ( ( TickType_t ) 1000 )

#define tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR    ( ( BaseType_t ) -2 )       
#define tmrCOMMAND_EXECUTE_CALLBACK             ( ( BaseType_t ) -1 )       
#define tmrCOMMAND_START_DONT_TRACE             ( ( BaseType_t ) 0 )        
#define tmrCOMMAND_START                        ( ( BaseType_t ) 1 )        
#define tmrCOMMAND_RESET                        ( ( BaseType_t ) 2 )        
#define tmrCOMMAND_STOP                         ( ( BaseType_t ) 3 )        
#define tmrCOMMAND_CHANGE_PERIOD                ( ( BaseType_t ) 4 )        
#define tmrCOMMAND_DELETE                       ( ( BaseType_t ) 5 )        
                                                                            
#define tmrFIRST_FROM_ISR_COMMAND               ( ( BaseType_t ) 6 )        
#define tmrCOMMAND_START_FROM_ISR               ( ( BaseType_t ) 6 )        
#define tmrCOMMAND_RESET_FROM_ISR               ( ( BaseType_t ) 7 )        
#define tmrCOMMAND_STOP_FROM_ISR                ( ( BaseType_t ) 8 )        
#define tmrCOMMAND_CHANGE_PERIOD_FROM_ISR       ( ( BaseType_t ) 9 )        
                                                                           
typedef void * TimerHandle_t; 
typedef void (*TimerCallbackFunction_t)( TimerHandle_t xTimer );       
typedef struct tmrTimerControl                  
{                                               
    const char              *pcTimerName;       
    ListItem_t              xTimerListItem;     
    TickType_t              xTimerPeriodInTicks;
    UBaseType_t             uxAutoReload;       
    void                    *pvTimerID;         
    TimerCallbackFunction_t pxCallbackFunction; 
    #if( configUSE_TRACE_FACILITY == 1 )        
        UBaseType_t         uxTimerNumber;      
    #endif                                      
} xTIMER;                                       
typedef xTIMER Timer_t; 

#define xTimerStart( xTimer, xTicksToWait ) xTimerGenericCommand( ( xTimer ), tmrCOMMAND_START, ( xTaskGetTickCount() ), NULL, ( xTicksToWait ) )
