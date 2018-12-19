/**

*This version of malloc uses an explicit free list.
 *Each block of memory has a header and footer, that holds the size and allocation bit of that specific block
 *A free block containers a pointer to the previous and next free block in the free list.
 *When performing the malloc or realloc fucntions, a global pointer searches through the free list via the block pointers
 *and returns the first block that size is equal to or greater then the amount of bytes needed
*for the purpose of this lab, some of the basic fuctions of malloc and some guideline ideas were inspired from your class book, "Computer Systems: Pearson New International Edition" (Bryant & Hallaron) 
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
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//############################################################### CONSTANTS AND MACROS AND HELPER FUNCTION DECLARATION

#define WSIZE 4 //size of a word
#define DSIZE 8 // Double word size (bytes)
#define CHUNKSIZE 16 // Size of the amount heap is extended by (bytes)
#define OVERHEAD 24  // Overhead padding

//determine the maximum
#define MAX(x ,y)  ((x) > (y) ? (x) : (y))

// pack a size and allocated bit into a word
#define PACK(size, alloc)  ((size) | (alloc))

// read and write a word at address p
#define GET(p)  (*(size_t *)(p))
#define PUT(p, value)  (*(size_t *)(p) = (value))

// read the size and allocation bit from address p
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

// address of block's header and footer
#define HDRP(ptr)  ((void *)(ptr) - WSIZE)
#define FTRP(ptr)  ((void *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

// address of next and previous free blocks
#define NEXT_FP(ptr)  (*(void **)(ptr + DSIZE))
#define PREV_FP(ptr)  (*(void **)(ptr))

// address of next and previous blocks
#define NEXT_BLKP(ptr)  ((void *)(ptr) + GET_SIZE(HDRP(ptr)))
#define PREV_BLKP(ptr)  ((void *)(ptr) - GET_SIZE(HDRP(ptr) - WSIZE))




//Helper fuctions
static void frontInsert(void *ptr);
static void removeBlock(void *ptr);
static int blockChecker(void *ptr);
static void place(void *ptr, size_t size);
static void *findFit(size_t size);
static void *coalesce(void *ptr);
static void *extendedHeap(size_t words);


//global variables
static char *heap_ptr = 0;  //pointer to the first block
static char *free_ptr = 0;   // pointer to the first free block

//####################################################################### END OF MACROS AND DECLARATION OF HELPER FUNCTIONS

//#######################################################################  HELPER FUNCTIONS



// coalesce - combines consecutive free blocks into one large free block. Return ptr to coalesced block

static void *coalesce(void *ptr){
    size_t previous_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr))) || PREV_BLKP(ptr) == ptr;
    size_t next__alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));


    //If the prev is not allocated and the next is allocated, then join ptr with the prev and remove
    if(!previous_alloc && next__alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        removeBlock(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

    //If the next is not allocated and the prev is allocated, then join ptr with the next and remove
    else if(previous_alloc && !next__alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        removeBlock(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }

   //If they are both (prev/next) not allocated, they join all and remove blocks
    else if(!previous_alloc && !next__alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        removeBlock(PREV_BLKP(ptr));
        removeBlock(NEXT_BLKP(ptr));
        ptr = PREV_BLKP(ptr);
        PUT(FTRP(ptr), PACK(size, 0));
    }

     //  If they are both allocated (prev/next) then returns ptr and insert at front
    frontInsert(ptr);
    return ptr;
}


// remove block from free list

static void removeBlock(void *ptr){
    if(PREV_FP(ptr)){
        NEXT_FP(PREV_FP(ptr)) = NEXT_FP(ptr);
    }

    else{
        free_ptr = NEXT_FP(ptr);
    }

    PREV_FP(NEXT_FP(ptr)) = PREV_FP(ptr);
}

//insert in free list

static void frontInsert(void *ptr){
    NEXT_FP(ptr) = free_ptr;
    PREV_FP(free_ptr) = ptr;
    PREV_FP(ptr) = NULL;
    free_ptr = ptr;
}

//extendHeap - Add free block and return its block pointer

static void* extendedHeap(size_t words){
    char *ptr;
    size_t size;

    //allocate set number of bytes for alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if(size < OVERHEAD){
        size = OVERHEAD;
    }

    if((long)(ptr = mem_sbrk(size)) == -1){
        return NULL;
    }

   //intializes the header and footer with zeros (free block)
    PUT(HDRP(ptr), PACK(size, 0)); //block header
    PUT(FTRP(ptr), PACK(size, 0)); //block footer
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); //epilogue block

    //merge if the prev block was free
    return coalesce(ptr);
}



//findFit - Find a fit for a block with of needed amout or greater bytes

static void *findFit(size_t size){
    void *ptr;

    //first fit search
    for(ptr = free_ptr; GET_ALLOC(HDRP(ptr)) == 0; ptr = NEXT_FP(ptr)){
    //test if the block is free and greater than or equal to needed amount
        if(size <= GET_SIZE(HDRP(ptr))){
            return ptr;
        }
    }
   //return null for no fit
    return NULL;
}


static int blockChecker(void *ptr){
    if(NEXT_FP(ptr) < mem_heap_lo() || NEXT_FP(ptr) > mem_heap_hi()){
        printf("Fatal: Next free pointer %p is out of bounds\n", NEXT_FP(ptr));
        return -1;
    }

    if(PREV_FP(ptr) < mem_heap_lo() || PREV_FP(ptr) > mem_heap_hi()){
        printf("Fatal: Previous free pointer %p is oiut of bounds", PREV_FP(ptr));
        return -1;
    }

    if((size_t)ptr % 8){
        printf("Fatal: %p is not aligned", ptr);
        return -1;
    }

    if(GET(HDRP(ptr)) != GET(FTRP(ptr))){
        printf("Fatal: Header and footer mismatch");
        return -1;
    }

    return 0;
}



//place - Place block of certain amount of bytes at start of free block ptr and split
//if remainder would be at least minimum block size

static void place(void *ptr, size_t size){
    size_t asize = GET_SIZE(HDRP(ptr));  //size of the free block


    //checks if the size of the free block is greater than the needed space
    if((asize - size) >= OVERHEAD){
        PUT(HDRP(ptr), PACK(size, 1)); //if yes, packs the block with adjust amount bytes and change allocated bit
        PUT(FTRP(ptr), PACK(size, 1));
        removeBlock(ptr);
        ptr = NEXT_BLKP(ptr); // places the remainding bits in the next block
        PUT(HDRP(ptr), PACK(asize - size, 0));
        PUT(FTRP(ptr), PACK(asize - size, 0));
        coalesce(ptr);
    }
//if not, packs the block with size amount bytes nd change allocated bit and remove block
    else{
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        removeBlock(ptr);
    }
}


int mm_check(void){
    void *ptr = heap_ptr;
    printf("Heap (%p): \n", heap_ptr);

    //checks if the prologue block is aligned and allocated
    //if inconsistent return -1
    if((GET_SIZE(HDRP(heap_ptr)) != OVERHEAD) || !GET_ALLOC(HDRP(heap_ptr))){
        printf("Fatal: Bad prologue header\n");
        return -1;
    }
    //if inconsistent return -1
    if(blockChecker(heap_ptr) == -1){
        return -1;
    }

    for(ptr = free_ptr; GET_ALLOC(HDRP(ptr)) == 0; ptr = NEXT_FP(ptr)){
    //if inconsistent return -1
         if(blockChecker(ptr) == -1){
                return -1;
         }
    }
    //if consistent return 0
    return 0;
}




//####################################################################### END OF HELPER FUNCTIONS

//####################################################################### MODIFIED GIVEN FUNCTIONS



//mm_init - Initialize the memory manager

int mm_init(void)
{
   // creates the initial empty heap
    if((heap_ptr = mem_sbrk(2 * OVERHEAD)) == NULL){
        return -1;
    }

    PUT(heap_ptr, 0); //alignment padding
    PUT(heap_ptr + WSIZE, PACK(OVERHEAD, 1)); //prologue header
    PUT(heap_ptr + DSIZE, 0); //prev pointer
    PUT(heap_ptr + DSIZE + WSIZE, 0); //next pointer
    PUT(heap_ptr + OVERHEAD, PACK(OVERHEAD, 1)); //prologue footer
    PUT(heap_ptr + WSIZE + OVERHEAD, PACK(0, 1)); //epilogue header
    free_ptr = heap_ptr + DSIZE;  //initilize free block pointer

   // extend the empty heap with a free block of CHUNKSIZE bytes
    if(extendedHeap(CHUNKSIZE / WSIZE) == NULL){
        return -1;
    }

    return 0;
}


//mm_malloc - Allocate a block with at least size bytes of payload

void *mm_malloc(size_t size)
{
    size_t asize; //adjusted size
    size_t esize; //by how much you  want to extend the heap
    char *ptr;

 //checks if size is equal to zro and ignores it
    if(size <= 0){
        return NULL;
    }

 //align the block size
    asize = MAX(ALIGN(size) + DSIZE, OVERHEAD);

    //search in free list for a free block
    if((ptr = findFit(asize))){
        place(ptr, asize);
        return ptr;
    }

 //extends heap, if there is no fit
    esize = MAX(asize, CHUNKSIZE);

    if((ptr = extendedHeap(esize / WSIZE)) == NULL)
        return NULL;

  //place/divide block
    place(ptr, asize);
    //returns the final pointer to the new allocated block
    return ptr;
}

// mm_free - Free a block

void mm_free(void *ptr)
{
    if(!ptr) return;

    //gets the size of the block to be freed
    size_t size = GET_SIZE(HDRP(ptr));

   //packs the heade and footer of the block with zeros
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

//mm_realloc - Allocates and already allocated block to a new space of a different size

void *mm_realloc(void *ptr, size_t size)
{
    size_t osize;
    void *newptr;
    size_t asize = MAX(ALIGN(size) + DSIZE, OVERHEAD);

    //checks if size is zero and just frees the block
    if(size <= 0){
        mm_free(ptr);
        return 0;
    }

 //checks if ptr exists, if not then malloc size
    if(ptr == NULL){
        return mm_malloc(size);
    }

//##################################
    osize = GET_SIZE(HDRP(ptr));

    if(osize == asize){
        return ptr;
    }

    if(asize <= osize){
        size = asize;

        if(osize - size <= OVERHEAD){
            return ptr;
        }

        PUT(HDRP(ptr), PACK(size, 1));
        PUT(FTRP(ptr), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(osize - size, 1));
        mm_free(NEXT_BLKP(ptr));
        return ptr;
    }

//#######################################
    //allocates new memory block of size bytes
    newptr = mm_malloc(size);

    //checks if realloc had fail, if yes, then the block is not touched
    if(!newptr){
        return 0;
    }

   //copy old data
    if(size < osize){
        osize = size;
    }

    memcpy(newptr, ptr, osize);
    //free the old block
    mm_free(ptr);
    return newptr;
}


//####################################################################### END OF MODIFIED GIVEN FUNCTIONS
