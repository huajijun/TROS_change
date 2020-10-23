#define size_t int
#define uint8_t  char
#define NULL 0
#define portPOINTER_SIZE_TYPE	int
#define uint16_t short
#define bool char
#define false (char)0
#define true (char)1
#define heapMINIMUM_BLOCK_SIZE ( ( size_t ) ( heapSTRUCT_SIZE * 2 ) )
void *pvPortMalloc( size_t xWantedSize );
void vPortFree( void *pv );
