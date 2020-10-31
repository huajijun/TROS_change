#include "task.h"
#define taskCHECK_FOR_STACK_OVERFLOW()                                                              \
	{                                                                                                   \
	    const uint32_t * const pulStack = ( uint32_t * ) pxCurrentTCB->pxStack;                         \
	    const uint32_t ulCheckValue = ( uint32_t ) 0xa5a5a5a5;                                          \
	                                                                                                    \
	    if( ( pulStack[ 0 ] != ulCheckValue ) ||                                                \        
	        ( pulStack[ 1 ] != ulCheckValue ) ||                                                \        
	        ( pulStack[ 2 ] != ulCheckValue ) ||                                                \        
	        ( pulStack[ 3 ] != ulCheckValue ) )                                             \
	    {                                                                                               \
	        vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pxCurrentTCB->pcTaskName );   \
	    }                                                                                               \
	}                                                                                                    
	void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
	{                                                                          
	                                                                           
	    /* Run time stack overflow checking is performed if                    
	    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook        
	    function is called if a stack overflow is detected. */                 
	    //taskDISABLE_INTERRUPTS();                                              
	    return 0;
	}   
