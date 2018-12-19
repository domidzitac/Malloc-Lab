/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
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


/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr ptr, compute address of its header and footer */
#define HDRP(ptr)       ((char *)(ptr) - WSIZE)
#define FTRP(ptr)       ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Given block ptr ptr, compute address of next and previous blocks */
#define NEXT_BLKP(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

/* The only global variable is a pointer to the first block */
static char *heap_listp;

/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *ptr, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *ptr);
static void printblock(void *ptr);
static void checkblock(void *ptr);

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
void *malloc(size_t size)
{
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *ptr;
    
    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs. */
//    if (size <= DSIZE)
//        asize = DSIZE + OVERHEAD;
//    else
//        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);
    asize = ALIGN(size + SIZE_T_SIZE);
    
    /* Search the free list for a fit */
    if ((ptr = find_fit(asize)) != NULL) {
        place(ptr, asize);
        return ptr;
    }
    
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(ptr, asize);
    return ptr;
}

/*
 * mm_free - Free a block
 */
void free(void *ptr)
{
    if(!ptr) return;
    size_t size = GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


/* Not implemented. For consistency with 15-213 malloc driver */
void *realloc(void *oldptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return malloc(size);
    }
    
    newptr = malloc(size);
    
    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }
    
    /* Copy the old data. */
    oldsize = *SIZE_PTR(oldptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);
    
    /* Free the old block. */
    free(oldptr);
    
    return newptr;
}

/*
 * mm_checkheap - Check the heap for consistency
 */
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

/* The remaining routines are internal helper routines */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    void *ptr;
    size_t size;
	
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((ptr = mem_sbrk(size)) == (void *) -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(ptr), PACK(size, 0));         /* free block header */
    PUT(FTRP(ptr), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); /* new epilogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(ptr);
}

/*
 * place - Place block of asize bytes at start of free block ptr
 *         and split if remainder would be at least minimum block size
 */
static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(ptr));
    
    if ((csize - asize) >= (DSIZE + OVERHEAD)) {
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(csize-asize, 0));
        PUT(FTRP(ptr), PACK(csize-asize, 0));
    }
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

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));
    
    if (prev_alloc && next_alloc) {            /* Case 1 */
        return ptr;
    }
    
    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size,0));
        return(ptr);
    }
    
    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        return(PREV_BLKP(ptr));
    }
    
    else {                                     /* Case 4 */
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
