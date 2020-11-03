#ifndef zzy
#define zzy
	#define int8_t  char
	#define portCRITICAL_NESTING_IN_TCB                 1
	#define queueQUEUE_TYPE_BASE				( ( uint8_t ) 0U )  
	#define configUSE_QUEUE_SETS 0

	#define queueQUEUE_TYPE_BASE                ( ( uint8_t ) 0U )

	#define bktQUEUE_LENGTH             ( 5 )
	#define size_t int
	#define uint8_t  char
	#define NULL 0
	#define portPOINTER_SIZE_TYPE	int
	#define uint16_t short
	#define bool char
	#define false (char)0
	#define true (char)1
	#define configQUEUE_REGISTRY_SIZE 8
	#define uint32_t unsigned int
	#define uint64_t unsigned long
	typedef unsigned long UBaseType_t;
	typedef long BaseType_t;
	typedef void * QueueHandle_t;
	typedef uint32_t TickType_t;
	#define configTIMER_QUEUE_LENGTH        2 
	#define portMAX_DELAY ( TickType_t ) 0xffffffffUL
	#define pdFALSE false
	#define pdTRUE	true
	#define pdPASS          ( pdTRUE )
	#define queueSEND_TO_BACK       ( ( BaseType_t ) 0 ) 
	#define queueSEND_TO_FRONT      ( ( BaseType_t ) 1 ) 
	#define queueOVERWRITE          ( ( BaseType_t ) 2 ) 

	#define configMAX_PRIORITIES        ( 30 )
	#define portYIELD_WITHIN_API portYIELD
	#define portYIELD()                 vPortYield()
	#define StackType_t uint32_t
	#define configMAX_TASK_NAME_LEN         ( 16 )
	#define errQUEUE_EMPTY  ( ( BaseType_t ) 0 )
	#define errQUEUE_FULL   ( ( BaseType_t ) 0 )
	typedef void (*TaskFunction_t)( void * );
	#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY   ( -1 )
	#define errQUEUE_BLOCKED                        ( -4 )
	#define errQUEUE_YIELD                          ( -5 )
	#define portSTACK_TYPE  uint64_t
	#define pdFAIL          ( pdFALSE ) 
	#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 1024 )
	#define configTIMER_TASK_STACK_DEPTH	( configMINIMAL_STACK_SIZE )
	#define configTIMER_TASK_PRIORITY		( 2 )

	#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
	#define pvPortMallocAligned( x, puxStackBuffer ) ( ( ( puxStackBuffer ) == NULL ) ? ( pvPortMalloc( ( x ) ) ) : ( puxStackBuffer ) )
	
	#define portSTACK_GROWTH            ( -1 )                                               
	#define portDISABLE_INTERRUPTS()                __asm volatile  ( "csrc mstatus,1" )
	#define portENABLE_INTERRUPTS()                 __asm volatile  ( "csrs mstatus,1" )

	#define portPRIVILEGE_BIT ( ( UBaseType_t ) 0x00 )  
	#define tskIDLE_PRIORITY            ( ( UBaseType_t ) 0U )
	#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters ) 
	#define configTICK_RATE_HZ          ( ( TickType_t ) 1000 )
	#define portTICK_PERIOD_MS          ( ( TickType_t ) (1000 / configTICK_RATE_HZ) ) 
	#define portYIELD_WITHIN_API portYIELD
	#define tskIDLE_STACK_SIZE	configMINIMAL_STACK_SIZE

#endif

