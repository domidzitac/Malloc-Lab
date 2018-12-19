/*
	 * mm-naive.c - The fastest, least memory-efficient malloc package.
	 *
	 *
	 * NOTE TO STUDENTS: Replace this header comment with your own header
	 * comment that gives a high level description of your solution.
	 */
	#include <stdio.h>
	#include <stdlib.h>
	#include <assert.h>
	#include <unistd.h>
	#include <string.h>
	

	#include "mm.h"
	#include "memlib.h"
	

	/*********************************************************
	 * NOTE TO STUDENTS: Before you do anything else, please
	 * provide your team information in the following struct.
	 ********************************************************/
	team_t team = {
	    /* Team name */
	    "TiDe",
	    /* First member's full name */
	    "Domnica Dzitac",
	    /* First member's email address */
	    "did233",
	    /* Second member's full name (leave blank if none) */
	    "Tyeece Hensley",
	    /* Second member's email address (leave blank if none) */
	    "tkh265"
	};
	

	/* single word (4) or double word (8) alignment */
	#define ALIGNMENT 8
	

	/* rounds up to the nearest multiple of ALIGNMENT */
	#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
	

	#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
	

	#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))
	

	//####################################################################### Constants and Macros
	

	#define WSIZE       4       // Size of header and footer
	#define DSIZE       8       // Double word size (bytes)
	#define CHUNKSIZE   16  // Size of the amount heap is extended by (bytes)
	#define OVERHEAD    24       // Overhead padding for header and footers
	

	//determine the maximum and the minimum
	#define MAX(x, y) ((x) > (y)? (x) : (y))
	#define MIN(x, y) ((x) < (y) ? (x) : (y))
	

	// pack a size and allocated bit into a word
	#define PACK(size, alloc)  ((size) | (alloc))
	

	// read and write a word at address p
	#define GET(p)       (*(unsigned int *)(p))
	#define PUT(p, val)  (*(unsigned int *)(p) = (val))
	

	// read the size and allocation bit from address p
	#define GET_SIZE(p)  (GET(p) & ~0x7)
	#define GET_ALLOC(p) (GET(p) & 0x1)
	

	// address of block's header and footer
	#define HDRP(ptr)       ((char *)(ptr) - WSIZE)
	#define FTRP(ptr)       ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)
	

	// address of (physically) next and previous blocks
	#define NEXT_BLKP(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
	#define PREV_BLKP(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))

// address of the previous and next free block in the free list
#define NEXT_FREEP(ptr)  (*(void **)(ptr + DSIZE)) //Get the address of the next free block
#define PREV_FREEP(ptr)  (*(void **)(ptr))       //Get the address of the previous free block
	

	

	//global variable
	static char *heap_ptr = 0; //a pointer to 1st block
static char *free_ptr = 0; //a pointer to 1st free block
	

	//helper functions prototypes
	static void *extendHeap(size_t wsize);
	static void place(void *ptr, size_t adjust);
	static void *findFit(size_t adjust);
	static void *coalesce(void *ptr);
	static void removeBlock(void *ptr);
	static void checkBlock(void *ptr);
static void frontInsert (void *ptr);
	

	

	//####################################################################### END OF MACROS
	

	

	// mm_checkheap - Check the heap for consistency

void mm_checkheap(int ver)
{
    char *ptr = heap_ptr;

    if (ver)
        printf("Heap (%p):\n", heap_ptr);

    if ((GET_SIZE(HDRP(heap_ptr)) != DSIZE) || !GET_ALLOC(HDRP(heap_ptr)))//checks if the prologue block is aligned and allocated
        printf("Bad prologue header\n");
    checkblock(heap_ptr);

    for (ptr = heap_ptr; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {//checks if the header and footer for each block in heap matches
        if (ver)
            printblock(ptr);
        checkblock(ptr);
    }

    if (ver)
        printblock(ptr);
    if ((GET_SIZE(HDRP(ptr)) != 0) || !(GET_ALLOC(HDRP(ptr))))//checks if the epilogue block is of si
        printf("Bad epilogue header\n");
}

//Internal functions

//extendHeap - Add free block and return its block pointer

static void *extendHeap(size_t wsize)
{
    void *ptr;
    size_t size;


    //allocate set number of bytes for alignment
    size = (wsize % 2) ? (wsize+1) * WSIZE : wsize * WSIZE;

    if(size < OVERHEAD){
        size = OVERHEAD;
    }

    if ((ptr = mem_sbrk(size)) == (void *) -1)
        return NULL;

    //intializes the header and footer with zeros (free block)
    PUT(HDRP(ptr), PACK(size, 0));         //block header
    PUT(FTRP(ptr), PACK(size, 0));         //block footer
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); //epilogue block

    //merge if the prev block was free
    return coalesce(ptr);
}

//place - Place block of certain amount of bytes at start of free block ptr and split if remainder would be at least minimum block size

static void place(void *ptr, size_t adjust)
{
    size_t size = GET_SIZE(HDRP(ptr)); //size of the free block

    //checks if the size of the free block is greater than the needed space
    if ((size - adjust) >= (DSIZE + OVERHEAD)) {
        PUT(HDRP(ptr), PACK(adjust, 1)); //if yes, packs the block with adjust amount bytes and change allocated bit
        PUT(FTRP(ptr), PACK(adjust, 1));
        ptr = NEXT_BLKP(ptr); // places the remainding bits in the next block
        PUT(HDRP(ptr), PACK(size-adjust, 0));
        PUT(FTRP(ptr), PACK(size-adjust, 0));
        coalesce(ptr);
    }
    //if not, packs the block with size amount bytes nd change allocated bit
    else {
        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        removeBlock(ptr);
    }
}

//findFit - Find a fit for a block with of needed amout or greater bytes

static void *findFit(size_t adjust)
{
  void *ptr;

    for(ptr = free_ptr; GET_ALLOC(HDRP(ptr)) == 0; ptr = NEXT_FREEP(ptr)){                    //Traverse the entire free list
        if(adjust <= GET_SIZE(HDRP(ptr))){                                                     //If size fits in the available free block
            return ptr;                                                                      //Return the block pointer
        }
    }

    return NULL;
}

// coalesce - combines consecutive free blocks into one large free block. Return ptr to coalesced block

static void *coalesce(void *ptr)
{
    size_t prevAl = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t nextAl = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    // 1. If they are both allocated (prev/next) then returns ptr
    if (prevAl && nextAl) {
        insertFront(ptr);
        return ptr;
    }

    //2. If the prev is not allocated and the next is allocated, then join ptr with the prev
    else if (!prevAl && nextAl) {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        removeBlock(ptr);
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        return(PREV_BLKP(ptr));
    }

    //3. If the next is not allocated and the prev is allocated, then join ptr with the next
    else if (prevAl&& !nextAl) {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        ptr = NEXT_BLKP(ptr)
        removeBlock(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size,0));
        return(ptr);
    }

    //4. If they are both (prev/next) not allocated, they join all
    else {
        size += (GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr))));
        removeBlock(PREV_BLKP(ptr));                                                        //Remove the block previous to the current block
        removeBlock(NEXT_BLKP(ptr));                                                        //Remove the block next to the current block
        ptr = PREV_BLKP(ptr);
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        return(PREV_BLKP(ptr));
    }
}

/*
static void printBlock(void *ptr)
{
    size_t hdsize, hdalloc, ftsize, ftalloc;

    hdsize = GET_SIZE(HDRP(ptr));//
    hdalloc = GET_ALLOC(HDRP(ptr));//
    ftsize = GET_SIZE(FTRP(ptr));//
    ftalloc = GET_ALLOC(FTRP(ptr));//

    if (hdsize == 0) {
        printf("%p: EOL\n", ptr);
        return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", ptr,
           (int)hsize, (hdalloc ? 'a' : 'f'),
           (int)fsize, (ftalloc ? 'a' : 'f'));
}
*/

static void checkBlock(void *ptr)
{
    if ((size_t)ptr % 8)
        printf("Error: %p is not doubleword aligned\n", ptr);
    if (GET(HDRP(ptr)) != GET(FTRP(ptr)))
printf("Error: header does not match footer\n");
}


static void insertFront(void *ptr){
    NEXT_FREEP(ptr) = free_ptr;                                                            //Sets the next pointer to the start of the free list
    PREV_FREEP(free_ptr) = ptr;                                                            //Sets the current's previous to the new block
    PREV_FREEP(ptr) = NULL;                                                                  //Set the previosu free pointer to NULL
    free_ptr = ptr;                                                                        //Sets the start of the free list as the new block
}


static void removeBlock(void *ptr){
    if(PREV_FREEP(ptr)){                                                                     //If there is a previous block
        NEXT_FREEP(PREV_FREEP(ptr)) = NEXT_FREEP(ptr);                                        //Set the next pointer of the previous block to next block
    }

    else{                                                                                   //If there is no previous block
        free_ptr = NEXT_FREEP(ptr);                                                        //Set the free list to the next block
    }

    PREV_FREEP(NEXT_FREEP(ptr)) = PREV_FREEP(ptr);                                            //Set the previous block's pointer of the next block to the previous block
}






	//#######################################################################
	//The functions you provided modified
	

	

	//mm_init - Initialize the memory manager
	

	int mm_init(void)
	{
	    // creates the initial empty heap
	    if ((heap_ptr = mem_sbrk(2 * OVERHEAD)) == NULL)
	        return -1;
	    PUT(heap_ptr, 0);                        /* alignment padding */
	    PUT(heap_ptr+WSIZE, PACK(OVERHEAD, 1));  /* prologue header */
    PUT(heap_ptr+DSIZE, 0);                  /*previous pointer*/
    PUT(heap_ptr+DSIZE+WSIZE, 0);            /*next pointer*/
	    PUT(heap_ptr+OVERHEAD, PACK(OVERHEAD, 1));  /* prologue footer */
	    PUT(heap_ptr+WSIZE+OVERHEAD, PACK(0, 1));   /* epilogue header */

   //initializes the free list pointer at the end of the heap
	    heap_ptr += DSIZE;
	

	    // extend the empty heap with a free block of CHUNKSIZE bytes
	    if (extendHeap(CHUNKSIZE/WSIZE) == NULL)
	        return -1;

	    return 0;
	}
	

	/*
	 * mm_malloc - Allocate a block with at least size bytes of payload
	 */
	void *mm_malloc(size_t size)
	{
	    size_t adjust;      //adjusted size
	    size_t extend_size; //by how much you  want to extend the heap
	    char *ptr;
	

	    //checks if size is equal to zro and ignores it
	    if (size <= 0)
	        return NULL;
	

	    //align the block size with minimum amount of bytes allowed
	    adjust = MAX(ALIGN(size) + DSIZE, OVERHEAD);
	

	    //search in free list for a free block
	    if ((ptr = findFit(adjust)) != NULL) {
	        place(ptr, adjust);
	        return ptr;
	    }
	

	    //extends heap, if there is no fit
	    extend_size = MAX(adjust,CHUNKSIZE);
	    if ((ptr = extendHeap(extend_size/WSIZE)) == NULL)
	        return NULL;

	    //place/divide block
	    place(ptr, adjust);

	    //returns the final pointer to the new allocated block
	    return ptr;
	}
	

	/*
	 * mm_free - Free a block
	 */
	void mm_free(void *ptr)
	{
	    if(!ptr) return;
	    size_t size = GET_SIZE(HDRP(ptr)); //gets the size of the block to be freed
	

	    //packs the heade and footer of the block with zeros
	    PUT(HDRP(ptr), PACK(size, 0));
	    PUT(FTRP(ptr), PACK(size, 0));
	    coalesce(ptr);
	}
	

	

	/*
	 * mm_realloc - Allocates and already allocated block to a new space of a different size
	 */
	void *mm_realloc(void *ptr, size_t size)
	{
	

	    void *oldptr = ptr;
	    size_t oldsize;
	    void *newptr;
    size_t adjust;

   //align the block with minimum amount of bytes allowed

	    adjust = MAX(ALIGN(size) + DSIZE, OVERHEAD);


	  //checks if size is zero and just frees the block
	    if(size == 0) {
	        mm_free(oldptr);
	        return 0;
	    }
	

	    //checks if ptr exists, if not then malloc size
	    if(oldptr == NULL) {
	        return mm_malloc(size);
	    }
	

	    //allocates new memory block of size bytes
	    oldsize = GET_SIZE(HDRP(oldptr);

    //checks if the old and new size are the same, returns old ptr
    if (oldsize==adjust){
        return oldptr;}
	
     if (adjust <= oldsize){ //checks if the adjusted size is less than the old size
        size = adjust;//shrinks block size
        
        if (oldsize-size <= OVERHEAD){//checks if the new block can’t be formed
            return oldptr;}

        PUT(HDRP(oldptr), PACK(size,1)); //updates header size and allocated bit
        PUT(FTRP(oldptr), PACK(size, 1)); //updates footer size and allocated bit
        PUT(HDRP(NEXT_BLKP(oldptr)), PACK(oldsize-size,1));
        mm_free(NEXT_BLKP(oldptr)); //frees next block
        return oldptr;
    }
 
    newptr = mm_malloc(size); //allocates new block


	    //checks if realloc had fail, if yes, then the block is not touched
	    if(!newptr) {
	        return 0;
	    }
	

	    //copy old data
	    if(size < oldsize){
		    oldsize = size;}
	    memcpy(newptr, oldptr, oldsize);
	

	   //free the old block
	    mm_free(oldptr);
	

	    return newptr;
	}
	

