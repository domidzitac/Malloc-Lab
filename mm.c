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
    "ateam",
    /* First member's full name */
    "Tyeece Hensley",
    /* First member's netid */
    "tkh265",
    /* Second member's full name (leave blank if none) */
    "Domnica Dzitac",
    /* Second member's netid (leave blank if none) */
    "did233"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//#######################################################

//MACROS AND CONSTANTS FOR MANIPULATING THE FREE LIST/SEGREGATED LIST

#define WSIZE     4          // word and header/footer size (bytes)
#define DSIZE     8          // double word size (bytes)
#define CHUNKSIZE (1<<12) //Extend the heap by this amount of bytes

#define LIST_MAXSIZE     20      
#define REALLOC_BUFFER  (1<<7)    

//determine the minimum and maximum
#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y)) 

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address p 
#define GET(p)            (*(unsigned int *)(p)) //reads
#define PUT_TAG(p, val)       (*(unsigned int *)(p) = (val) | GET_TAG(p)) //writes with a tag
#define PUT(p, val) (*(unsigned int *)(p) = (val)) //writes without a tag

// Predecessor/ Successor pointer of free blocks 
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

// Read the size and allocation bit from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p)   (GET(p) & 0x2) //gets the realloc tag 
#define SET_ReAllocTAG(p)   (GET(p) |= 0x2) //sets the realloc tag
#define REMOVE_ReAllocTAG(p) (GET(p) &= ~0x2) //removes the realloc tag

// Compute address of block's header and footer 
#define HDRP(ptr) ((char *)(ptr) - WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

// Compute address of next and previous blocks 
#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE))

// Compute address of predecessor and successor for a free block 
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

// Compute address of predecessor and successor for a free block on the segregated list 
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

//Global variables

static char *heap_ptr = 0; //heap pointer to the beginning of the heap
//static char *freelist_ptr = 0; //pointer to the beginning of the free list
void *segregated_list[LIST_MAXSIZE]; 



//####################################################### END OF MACROS


//EXTRA FUNCTIONS/ HELPER FUNCTIONS



//####################################################### END OF HELPER FUNCTIONS


//YOUR GIVEN FUNCTIONS MODIFIED ### INITIAL FREE LIST  

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	
	int i;

	//Initialize the segregated list with NULL
	
	for (i=0; i< LIST_MAXSIZE; i++){
		segregated_list[i]=NULL;

	}

	// Create the initial empty heap
    	if ((long)(heap_ptr = mem_sbrk(4 * WSIZE)) == -1)
        	return -1;
    
    	PUT(heap_ptr, 0);                            /* Alignment padding */
    	PUT(heap_ptr + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    	PUT(heap_ptr + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    	PUT(heap_ptr + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    
	//extend the empty heap with a free block
    	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t adjustsize;
	size_t extendheapsize;
	void *final= NULL;
	int i;

	//Checks if size is equal to zero and ignores it
	if (size==0){
		return NULL;
	}

	//Align the block size
	 if (size <= DSIZE){
		adjustsize= (2*DSIZE);
	}
	else{
		adjustsize= ALIGN(DSIZE+size);
	}
	
	size_t finalsize = adjustsize;

	//Search in segregated list for a free block
	

	for(i=0; i<LIST_MAXSIZE; i++ ){
		if((i==LIST_MAXSIZE-1) || ((finalsize <= 1)&&(segregated_list[i]!=NULL)) ){
			final=segregated_list[i];
			while((final != NULL)&&((adjustsize>GET_SIZE(HDRP(final))||(GET_TAG(HDRP(final))) {
				final=PRED(final);
			]
			if (final != NULL){
				break;
			}
		}
		finalsize >>= 1;
	}
	//If there is no fit, extends heap

	if (final ==NULL){
		extendheapsize = MAX (adjustsize, CHUNKSIZE);
		
		if ((final = extend_heap(extendheapsize))==NULL)
			return NULL;
	}
	//Place/divide block
	final = place(final,adjustsize);
	//Returns pointer to place block
	return final;
	 
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

	size_t size = GET_SIZE(HDRP(ptr)); //gets the size of the current block from its header
	
	REMOVE_ReAllocTAG(HDRP(NEXT_BLKP(ptr))); //removing the reallocation tag from the succesor block
	PUT_TAG(HDRP(ptr), PACK(size,0));//write tagged header
	PUT_TAG(FTRP(ptr), PACK(size,0));//write tagged footer

	//insert free ptr node into the segregated list and merge adjesent free blocks
	insert_node(ptr,size);
	coalesce(ptr);
	
	return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}













