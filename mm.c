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

//#######################################################################
//Constants and Macros

#define WSIZE       4       // Size of header and footer
#define DSIZE       8       // Double word size (bytes)
#define CHUNKSIZE  (1<<12)  // Size of the amount heap is extended by (bytes)
#define OVERHEAD    8       // Overhead padding for header and footers

//determine the maximum and the minimum
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc)  ((size) | (alloc))

// Read and write a word at address p
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

// Read the size and allocation bit from address p
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Address of block's header and footer
#define HDRP(ptr)       ((char *)(ptr) - WSIZE)
#define FTRP(ptr)       ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

// Address of (physically) next and previous blocks
#define NEXT_BLKP(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))

//#######################################################################

//global variables
static char *heap_listp; //a pointer to 1st block

//helper functions
static void *extend_heap(size_t words);
static void place(void *ptr, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *ptr);
static void printblock(void *ptr);
static void checkblock(void *ptr);


//#######################################################################
//definition of helper functions

// mm_checkheap - Check the heap for consistency

void mm_checkheap(int verbose)
{
    char *ptr = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (verbose)
            printblock(ptr);
        checkblock(ptr);
    }

    if (verbose)
        printblock(ptr);
    if ((GET_SIZE(HDRP(ptr)) != 0) || !(GET_ALLOC(HDRP(ptr))))
        printf("Bad epilogue header\n");
}

//Internal func

//extend_heap - Add free block and return its block pointer

static void *extend_heap(size_t words)
{
    void *ptr;
    size_t size;

    //allocate set number of words for alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((ptr = mem_sbrk(size)) == (void *) -1)
        return NULL;

    //intializes the header and footer with zeros (free block)
    PUT(HDRP(ptr), PACK(size, 0));         //block header
    PUT(FTRP(ptr), PACK(size, 0));         //block footer
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); //epilogue block

    //merge if the prev block was free
    return coalesce(ptr);
}

//place - Place block of asize bytes at start of free block ptr
 //  and split if remainder would be at least minimum block size

static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(ptr)); //size of the free block

    //checks if the size of the free block is greater than the needed space
    if ((csize - asize) >= (DSIZE + OVERHEAD)) {
        PUT(HDRP(ptr), PACK(asize, 1)); //if yes, packs the block with adjust amount bytes and change allocated bit
        PUT(FTRP(ptr), PACK(asize, 1));
        ptr = NEXT_BLKP(ptr); // places the remainding bits in the next block
        PUT(HDRP(ptr), PACK(csize-asize, 0));
        PUT(FTRP(ptr), PACK(csize-asize, 0));
    }
    //if not, packs the block with size amount bytes nd change allocated bit
    else {
        PUT(HDRP(ptr), PACK(csize, 1));
        PUT(FTRP(ptr), PACK(csize, 1));
    }
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    void *ptr;

    /* first fit search */
    for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) && (asize <= GET_SIZE(HDRP(ptr)))) {
            return ptr;
        }
    }
    return NULL; /* no fit */
}

// coalesce - boundary tag coalescing. Return ptr to coalesced block

static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    // 1. If they are both allocated (prev/next) then returns ptr
    if (prev_alloc && next_alloc) {
        return ptr;
    }

    //2. If the prev is not allocated and the next is allocated, then join ptr with the prev
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        return(PREV_BLKP(ptr));
    }

    //3. If the next is not allocated and the prev is allocated, then join ptr with the next
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size,0));
        return(ptr);
    }

    //4. If they are both (prev/next) not allocated, they join all
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) +
	    GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        return(PREV_BLKP(ptr));
    }
}

static void printblock(void *ptr)
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(ptr));
    halloc = GET_ALLOC(HDRP(ptr));
    fsize = GET_SIZE(FTRP(ptr));
    falloc = GET_ALLOC(FTRP(ptr));

    if (hsize == 0) {
        printf("%p: EOL\n", ptr);
        return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", ptr,
           (int)hsize, (halloc ? 'a' : 'f'),
           (int)fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *ptr)
{
    if ((size_t)ptr % 8)
        printf("Error: %p is not doubleword aligned\n", ptr);
    if (GET(HDRP(ptr)) != GET(FTRP(ptr)))
printf("Error: header does not match footer\n");
}





//#######################################################################
//The functions you provided modified


/*
 * mm_init - Initialize the memory manager
 */
int mm_init(void)
{
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == NULL)
        return -1;
    PUT(heap_listp, 0);                        /* alignment padding */
    PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1));  /* prologue header */
    PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1));  /* prologue footer */
    PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));   /* epilogue header */
    heap_listp += DSIZE;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
void *mm_malloc(size_t size)
{
    size_t asize;      //adjusted size
    size_t extendsize; //by how much you  want to extend the heap
    char *ptr;

    //checks if size is equal to zro and ignores it
    if (size <= 0)
        return NULL;

        //align the block size
    asize = ALIGN(size + SIZE_T_SIZE);

    //search in free list for a free block
    if ((ptr = find_fit(asize)) != NULL) {
        place(ptr, asize);
        return ptr;
    }

    //extends heap, if there is no fit
    extendsize = MAX(asize,CHUNKSIZE);
    if ((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    //place/divide block
    place(ptr, asize);
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

    //checks if size is zero and just frees the block
    if(size == 0) {
        free(oldptr);
        return 0;
    }

    //checks if ptr exists, if not then malloc size
    if(oldptr == NULL) {
        return mm_malloc(size);
    }

    //allocates new memory block of size bytes
    newptr = mm_malloc(size);

    //checks if realloc had fail, if yes, then the block is not touched
    if(!newptr) {
        return 0;
    }

    //copy old data
    oldsize = *SIZE_PTR(oldptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    //free the old block
    free(oldptr);

    return newptr;
}
