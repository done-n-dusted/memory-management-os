/*
 * mm.c
 *
 * Name: Group 36 - Anurag and Sai Naveen
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read the project PDF document carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_//printf(...) //printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_//printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

#define WSIZE 8 /* Word and header/footer size (bytes) */
#define DSIZE 16 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

static inline size_t MAX(size_t x, size_t y) {
  return ((x) > (y) ? (x) : (y));
}

static inline size_t MIN(size_t x, size_t y) {
	return x < y ? x : y;
}

/* Pack a size and allocated bit into a word */
static size_t PACK(size_t size, size_t alloc) {
  return ((size) | (alloc));
}

/* Read and write a word at address p */
static size_t GET(void *p) {
  return (*(size_t *)(p));
}

static void PUT(void *p, size_t val) {
  (*(size_t *)(p) = (val));
}

static inline size_t GET_SIZE(void *p) {
	return (size_t)(GET(p) & (~0xf));
}

static int GET_ALLOC(void *p) {
  return (GET(p) & 0x1);
}

/* Given block ptr bp, compute address of its header and footer */
static void *HDRP(void *bp) {
  return ((char *)(bp) - WSIZE);
}

static void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

/* Given block ptr bp, compute address of next and previous blocks */
static void *NEXT_BLKP(void *bp) {
  return ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

static void *PREV_BLKP(void *bp) {
  return ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/*Pointer to get NEXT and PREVIOUS pointer of free_list*/
static void *NEXT_PTR(void *p) {
    return (*(char **)(p + WSIZE));
}

static void *PREV_PTR(void *p) {
    return (*(char **)(p));
}

/* What is the correct alignment? */
#define ALIGNMENT 16
#define EXTEND_SIZE 1024

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */

/* Global variables*/
static char *free_list_ptr = 0;
static char *heap_start_ptr = 0;

/*Helpers*/
static void *coalesceBlk(void *ptr);
static void *extend_heap(size_t size);
static void add_to_free_list(void *ptr);
static void delete_from_free_list(void *ptr);
static void place(void *ptr, size_t size);

bool mm_init(void)
{
    /* IMPLEMENT THIS */
    /*
    - create empty heap
    - add padding
    - + prologue header
    - + prologue footer
    - + epilogue header
    - + extend_heap with CHUNKSIZE
    */
    // seting the heap start ptr
    heap_start_ptr = mem_sbrk(4*WSIZE);
    if (heap_start_ptr == (void *)-1) {
        return false;
    }

    PUT(heap_start_ptr, 0);
    PUT(heap_start_ptr+(WSIZE), PACK(DSIZE, 1));
    PUT(heap_start_ptr+(2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_start_ptr+(3*WSIZE), PACK(0, 1));
    heap_start_ptr += (2*WSIZE);
    
    // the first block is free
    free_list_ptr = heap_start_ptr + WSIZE;

    if (extend_heap(CHUNKSIZE) == NULL) {
        return false;
    }

    return true;
}

/*
    Extend heap
*/
static void *extend_heap(size_t size){

    size = align(size);

    //printf("extend heap started\n");
    void *first_end = mem_sbrk(size);
    
    if(first_end == (void *)-1){
        //printf("extend heap - size increase - failed\n");
        return NULL;
    }

    //printf("extend heap - size increased - success\n");

    PUT(HDRP(first_end), PACK(size, 0));
    PUT(FTRP(first_end), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(first_end)), PACK(0, 1));
    
    //printf("extend heap - func done - pointing to %p\n", first_end);
    return (coalesceBlk(first_end));
    // return first_end;
    
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */
    /* 
        algo -
        ptr check if data exists in that block
        if yes move to next block
        else
            check if block size is >= size
            if yes, create block with proper header and return ptr
            else move to next block
        
        if can't find ptr, mem_sbrk to open new one
        assign that to ptr else NULL
    */

//    Flaws: Adjusting block size
    /* Check point 1
    //printf("malloc started - size = %lu\n", size);
    size_t adjusted_size = align(size + DSIZE);
    void *ptr = heap_start;

    // //printf("malloc pointing to - %p\n", ptr);
    
    while(ptr < heap_end){
        //printf("malloc pointing to - %p\n", ptr);

        size_t curr_blk_size = GET_SIZE(HDRP(ptr));
        size_t is_allocated = GET_ALLOC(HDRP(ptr));
        //printf("malloc currsize = %lu, alloc_stat = %lu\n", curr_blk_size, is_allocated);

        if(!is_allocated && curr_blk_size >= adjusted_size){
            //printf("malloc found block at - %p\n", ptr);
            //allocate this block
            //split if necessary
            size_t rem_size = curr_blk_size - adjusted_size;
            //printf("malloc rem_size - %lu\n", rem_size);

            if(rem_size >= DSIZE){ //limit size
                //printf("malloc rem_size >= DSIZE - %p\n", ptr);
                // //printf("###malloc currsize = %lu, alloc_stat = %lu\n", curr_blk_size, is_allocated);

                PUT(HDRP(ptr), PACK(adjusted_size, 1));
                PUT(FTRP(ptr), PACK(adjusted_size, 1));
                //printf("malloc ptr packed - %p - success\n", ptr);
                // //printf("$$$malloc currsize = %lu, alloc_stat = %d\n", curr_blk_size, GET_ALLOC(HDRP(ptr)));
                
                void *new_blk_ptr = NEXT_BLKP(ptr); //move to next block
                //printf("malloc new_blk_ptr - %p\n", new_blk_ptr);
                PUT(HDRP(new_blk_ptr), PACK(rem_size, 0));
                PUT(FTRP(new_blk_ptr), PACK(rem_size, 0));
                //printf("malloc new_blk_ptr packed - %p - success\n", new_blk_ptr);

                return ptr; //return allocated block
            }
            else{
                //printf("malloc rem_size < DSIZE - %p\n", ptr);
                PUT(HDRP(ptr), PACK(curr_blk_size, 1));
                PUT(FTRP(ptr), PACK(curr_blk_size, 1));
                //printf("malloc ptr packed - %p - success\n", ptr);
                return ptr;
            }
        }

        ptr = NEXT_BLKP(ptr);

    }

    //printf("no blk found, need to extend heap\n");

    //couldn't find block in heap: time to extend;

    size_t extended_size = MAX(adjusted_size, CHUNKSIZE);

    ptr = extend_heap(extended_size);
    return malloc(size); //output extended ptr
    */
   // final
    size_t adjusted_size = align(size + DSIZE);

    if (size == 0) {
        return NULL;
    }
    
    void *found_ptr = NULL;

    void *i;
	for (i = free_list_ptr; GET_ALLOC(HDRP(i)) == 0; i = NEXT_PTR(i)) {
		if (adjusted_size <= GET_SIZE(HDRP(i))) {
            found_ptr = i;
        }
	}

    if (found_ptr != NULL) {
        place(found_ptr, adjusted_size);
        return found_ptr;
    }

    //printf("no blk found, need to extend heap\n");

    //couldn't find block in heap: time to extend;

    size_t extended_size = MAX(adjusted_size, CHUNKSIZE);
    found_ptr = extend_heap(extended_size);
    
    if (found_ptr == NULL) {
        return NULL;
    }

    place(found_ptr, adjusted_size);
    return found_ptr;
}

/*
    place
*/
static void place(void *ptr, size_t size)
{
	size_t presentBlk_size = GET_SIZE(HDRP(ptr));   

	if ((presentBlk_size - size) >= (2 * DSIZE)) { 
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));

		delete_from_free_list(ptr);	
		ptr = NEXT_BLKP(ptr);
		
        PUT(HDRP(ptr), PACK(presentBlk_size - size, 0));
		PUT(FTRP(ptr), PACK(presentBlk_size - size, 0));
		
        coalesceBlk(ptr);
	} else {
		PUT(HDRP(ptr), PACK(presentBlk_size, 1));
		PUT(FTRP(ptr), PACK(presentBlk_size, 1));
		
        delete_from_free_list(ptr);
	}
}

/*
 * free
 */
void free(void* ptr)
{
    /* IMPLEMENT THIS */
    /* algo -
        ptr check if exits, check if allocated
            if allocated - remove the data / actually dont need to delete the data, just just the key to 0, meaning empty, next time u allocate something there, it will remove it automatically
            else - do nothing

        everytime we do free, need to combine the adj block if free,
            check if next is free in header
            check if previous is free in footer
                combine for eff use
    */

    if (ptr == NULL) {
        return;
    }

    //printf("free started\n");
    size_t presentBlk_size = GET_SIZE(HDRP(ptr));
    //printf("free presentBlk_size - %lu\n", presentBlk_size);
    
    PUT(HDRP(ptr), PACK(presentBlk_size, 0));
    PUT(FTRP(ptr), PACK(presentBlk_size, 0));
    //printf("free - ptr packed %p, and coalesce called \n", ptr);
    coalesceBlk(ptr);

    return;
}

static void *coalesceBlk(void *ptr)
{
    // printf("coalesceBlk - started\n");
    size_t size = GET_SIZE(HDRP(ptr));
    size_t is_prevoius_allocated = GET_ALLOC(FTRP(PREV_BLKP(ptr))) || PREV_BLKP(ptr) == ptr;
    size_t is_next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

    //printf("coalesceBlk - size = %lu, is_prevoius_allocated = %lu, is_next_allocated = %lu\n", size, is_prevoius_allocated, is_next_allocated);

    if (is_prevoius_allocated && is_next_allocated) {
        //printf("coalesceBlk - both previous and next are not free\n");
        
        add_to_free_list(ptr);
        return ptr;
    }
    else if (is_prevoius_allocated && !is_next_allocated) {
        /* 
        if next is free, then merge, size will increase and the header and footer need to update
            - update header to 0
            - update footer to 0
            - for both the size with be present + next size
        */
        //printf("coalesceBlk - next is free\n");
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        
        delete_from_free_list(NEXT_BLKP(ptr));
        
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        //printf("coalesceBlk - present+next combied\n");
        add_to_free_list(ptr);
        return ptr;
    }
    else if (!is_prevoius_allocated && is_next_allocated) {
        /* 
        if previous is free, then merge, size will increase and the header and footer need to update
            - update footer to 0
            - update prevoius header to 0
            - update size with prevoius + present
            - return privous blk
        */
        //printf("coalesceBlk - previous is free\n");
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        
        delete_from_free_list(PREV_BLKP(ptr));
        
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        //printf("coalesceBlk - previous+present combied\n");
        add_to_free_list(ptr);
        return PREV_BLKP(ptr);
    }
    else {
        /*
        if both are free, merge all three
            - update the header of prevoius to 0
            - update the footer of next to 0
            - size with be addition of all theree
            - return the privous blk
        */
        //printf("coalesceBlk - both are free\n");
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));

        delete_from_free_list(PREV_BLKP(ptr));
        delete_from_free_list(NEXT_BLKP(ptr));
        
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        //printf("coalesceBlk - previous+present+next combied\n");
        add_to_free_list(ptr);
        return PREV_BLKP(ptr);
    }

}

static void delete_from_free_list(void *ptr)
{
    if (PREV_PTR(ptr) == NULL) {
        free_list_ptr = NEXT_PTR(ptr);
    }
    else {
        (*(char **)((PREV_PTR(ptr)) + WSIZE)) = NEXT_PTR(ptr);
    }

    (*(char **)(NEXT_PTR(ptr))) = PREV_PTR(ptr);
}

static void add_to_free_list(void *ptr)
{
	(*(char **)(ptr + WSIZE)) = free_list_ptr;
	(*(char **)(free_list_ptr)) = ptr;
    (*(char **)(ptr)) = NULL;
	
    free_list_ptr = ptr;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    /* algo -
        we need malloc for this
        when we inc/dec the size, we cna find the best fit new block and change the address accordingly
            need to copy the data - see how
                is it needed to do best fit when the size dec ?
    */
    //printf("realloc - started\n");
    /* Checkpoint 1
    
    void *updated_ptr;

    if (oldptr == NULL) {
        //printf("realloc - old ptr is Null, so point to heap start\n");
        return malloc(size);
    }

    if (size == 0) {
        //printf("realloc - size is 0, so free\n");
        free(oldptr);
        return NULL;
    }

    //printf("realloc - finding where to place the blk\n");
    updated_ptr = malloc(size);
    //printf("realloc - got updated_ptr - %p\n", updated_ptr);

    if (updated_ptr == NULL) {
        //printf("realloc - updated_ptr came back Null\n");
        return NULL;
    }

	size_t cp_size = MIN(size, GET_SIZE(HDRP(oldptr)));
	memcpy(updated_ptr, oldptr, cp_size);
    //printf("realloc - copied to new ptr - %p\n", updated_ptr);

	free(oldptr);
    //printf("realloc - freed old ptr\n");

	return updated_ptr;
    */
    //final
    void *updated_ptr;

    if((int)size < 0) 
    	return NULL;

    if (oldptr == NULL) {
        //printf("realloc - old ptr is Null, so point to heap start\n");
        return malloc(size);
    }

    if (size == 0) {
        //printf("realloc - size is 0, so free\n");
        free(oldptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(oldptr));
    size_t new_size = align(size + (2*WSIZE));

    if (new_size <= old_size) {
        return oldptr;
    }
    else {
        size_t is_next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
        size_t next_Blk_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        size_t available_size = old_size + next_Blk_size;

        if (!is_next_allocated && available_size >= new_size) {
            delete_from_free_list(NEXT_BLKP(oldptr));
            
            PUT(HDRP(oldptr),PACK(available_size,1));
            PUT(FTRP(oldptr),PACK(available_size,1));
            
            return oldptr;
        }
        else {
            updated_ptr = malloc(size);
            //printf("realloc - got updated_ptr - %p\n", updated_ptr);

            if (updated_ptr == NULL) {
                //printf("realloc - updated_ptr came back Null\n");
                return NULL;
            }

            place(updated_ptr, new_size);
            memcpy(updated_ptr, oldptr, old_size);
            //printf("realloc - copied to new ptr - %p\n", updated_ptr);

            free(oldptr);
            return updated_ptr;
        }
    }
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{   
    
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    // heap size check
    size_t curr_heap_size = mem_heap_hi() - mem_heap_lo();
    printf("heapchecker\n");
    if(curr_heap_suze != heap_size){
        printf("Current Heap Size = %lu, Expected Heap Size = %lu\n", curr_heap_size, heap_size);
        printf("Heap Sizes not equal!\n");
        return false;
    }

    // header-footer size check and alloc
    void *ptr = heap_start;
    while(ptr <= heap_end){
        size_t size_in_header, size_in_footer;
        size_t alloc_in_header, alloc_in_footer;
        size_in_header = GET_SIZE(HDRP(ptr));
        size_in_footer = GET_SIZE(FTRP(ptr));
        alloc_in_header = GET_ALLOC(HDRP(ptr));
        alloc_in_footer = GET_ALLOC(FTRP(ptr));

        if(size_in_header != size_in_footer){
            printf("pointer to data @ %p, size in hdr = %lu, size in ftr = %lu\n", ptr, size_in_header, size_in_footer);
            printf("data sizes not equal in hdr and ftr\n");
            return false;
        }

        if(alloc_in_header != alloc_in_footer){
            printf("pointer to data @ %p, alloc in hdr = %lu, alloc in ftr = %lu\n", ptr, alloc_in_header, alloc_in_footer);
            printf("data Allocations not equal in hdr and ftr\n");
            return false;
        }

        ptr = NEXT_BLKP(ptr);
    }

    // prologue check
    void *prologue_hdr = PERV_BLKP(heap_start);
    void *prologue_ftr = NEXT_BLKP(prologue_hdr);
    size_t prologue_hdr_alloc = GET_ALLOC(prologue_hdr);
    size_t prologue_ftr_alloc = GET_ALLOC(prologue_ftr);
    if(!(prologue_hdr_alloc && prologue_ftr_alloc)){
        printf("Prologue incorrect\n");
        return false;
    }

    // epilogue check
    void *epilogue_hdr = mem_heap_lo() - WSIZE;
    size_t epilogue_alloc = GET_ALLOC(epilogue_hdr);
    if(!epilogue_hdr_alloc){
        printf("epilogue incorret\n");
        return false;
    }
#endif /* DEBUG */
    return true;
}
