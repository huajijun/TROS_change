#include <common.h>
#ifndef LIST
#define LIST
struct xLIST_ITEM
{
//	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	 TickType_t xItemValue;			/*< The value being listed.  In most cases this is used to sort the list in descending order. */
	struct xLIST_ITEM *  pxNext;		/*< Pointer to the next ListItem_t in the list. */
	struct xLIST_ITEM *  pxPrevious;	/*< Pointer to the previous ListItem_t in the list. */
	void * pvOwner;										/*< Pointer to the object (normally a TCB) that contains the list item.  There is therefore a two way link between the object containing the list item and the list item itself. */
	void *  pvContainer;				/*< Pointer to the list in which this list item is placed (if any). */
				/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
};
typedef struct xLIST_ITEM ListItem_t;					/* For some reason lint wants this as two separate definitions. */

struct xMINI_LIST_ITEM
{
//	listFIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE			/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	 TickType_t xItemValue;
	struct xLIST_ITEM *  pxNext;
	struct xLIST_ITEM *  pxPrevious;
};
typedef struct xMINI_LIST_ITEM MiniListItem_t;

/*
 * Definition of the type of queue used by the scheduler.
 */
typedef struct xLIST
{
//	listFIRST_LIST_INTEGRITY_CHECK_VALUE				/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
	 UBaseType_t uxNumberOfItems;
	ListItem_t *  pxIndex;			/*< Used to walk through the list.  Points to the last item returned by a call to listGET_OWNER_OF_NEXT_ENTRY (). */
	MiniListItem_t xListEnd;							/*< List item that contains the maximum possible item value meaning it is always at the end of the list and is therefore used as a marker. */
					/*< Set to a known value if configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES is set to 1. */
} List_t;
#define listSET_LIST_ITEM_OWNER( pxListItem, pxOwner )      ( ( pxListItem )->pvOwner = ( void * ) ( pxOwner ) )
#define listLIST_IS_EMPTY( pxList ) ( ( BaseType_t ) ( ( pxList )->uxNumberOfItems == ( UBaseType_t ) 0 ) )
#define listGET_OWNER_OF_HEAD_ENTRY( pxList )  ( (&( ( pxList )->xListEnd ))->pxNext->pvOwner )
#define listSET_LIST_ITEM_VALUE( pxListItem, xValue )   ( ( pxListItem )->xItemValue = ( xValue ) )
#define listGET_LIST_ITEM_VALUE( pxListItem )   ( ( pxListItem )->xItemValue )
#define listCURRENT_LIST_LENGTH( pxList )   ( ( pxList )->uxNumberOfItems )
#define listIS_CONTAINED_WITHIN( pxList, pxListItem ) ( ( BaseType_t ) ( ( pxListItem )->pvContainer == ( void * ) ( pxList ) ) )
#define listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxList )  ( ( ( pxList )->xListEnd ).pxNext->xItemValue )
#define listLIST_ITEM_CONTAINER( pxListItem ) ( ( pxListItem )->pvContainer )
#define listGET_OWNER_OF_NEXT_ENTRY( pxTCB, pxList )                                        \
{                                                                                           \
List_t * const pxConstList = ( pxList );                                                    \
    /* Increment the index to the next item and return the item, ensuring */                \
    /* we don't return the marker used at the end of the list.  */                          \
    ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                            \
    if( ( void * ) ( pxConstList )->pxIndex == ( void * ) &( ( pxConstList )->xListEnd ) )  \
    {                                                                                       \
        ( pxConstList )->pxIndex = ( pxConstList )->pxIndex->pxNext;                        \
    }                                                                                       \
    ( pxTCB ) = ( pxConstList )->pxIndex->pvOwner;                                          \
}                                                                                            
#endif