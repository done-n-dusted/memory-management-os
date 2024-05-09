/*
 * mm.c
 *
 * Name: Group 36 - Anurag and Sai Naveen
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read the project PDF document carefully and in its entirety before beginning.
 * 
    Block Structure:

    FREE: | Header (size, previous allocated, present allocated) | Prev Ptr | Next Ptr | Footer |

    DATA: | Header (size, previous allocated, present allocated) | Data |

 * Using Segregated free lists of various size classes. Each every class (i) has blocks
 * 2**(5+i) to (2**(6+i) - 1) bytes.
 * The starting of the heap has pointers corresponding to all the classes that point to
 * the first free block of that particular class.
 * 
 * Allocation:
 * You can get the pointer to the first free block of a particular class when malloc is called,
 * then traverse until you get a suitable block of that size and then allocate.
 * The iteration happens with the next and previous pointers stored in each free block.
 * If a suitable block is not found, you go the next class and now split it and save these
 * new split blocks in appropriate classes.
 * If a block is not found, the heap is extended to accomodate it.
 * 
 * Free:
 * Every header of allocated blocks even stores if previous block is allocated or not.
 * All the free blocks around the block are coalesced so a single huge block is created.
 * This block is updated in the appropriate size-class.
 * 
 * realloc:
 * It is a combination of both malloc and realloc.
 * 
 * Footer Optimization:
 * Only free blocks have footers. Now every data block's header has a bit dedicated to 
 * previous blocks allocation, which makes it easier to check if coalescing with the previous
 * block is necessary.
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
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_// // printf(...)
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

/* What is the correct alignment? */
#define ALIGNMENT 16

// definitions for seg list

#define WSIZE 8 /* Word and header/footer size (bytes) */
#define DSIZE 16 /* Double word size (bytes) */
#define CHUNKSIZE (1<<14) /* Extend heap by this amount (bytes) */

#define MIN_SIZE 32
#define uint unsigned int //just convenience
/*
Classes:
0 - 2**5 to 2**6 - 1
to 
8 - 2**13 to inf

*/

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*Global Variables*/
void *heap_start_ptr = NULL;
void *heap_data_ptr = NULL;
void *heap_end_ptr = NULL;
const unsigned short NUM_SIZE_CLASS = 16;

/*static methods*/
static void *coalesce(void *ptr);
static void print_blk(void* ptr);
static void *extend_heap(size_t size);
static void add_blk(void *bp);
static void delete_blk(void *bp);
static void *best_fit(size_t size);
static void place(void *ptr, size_t size);
static void *traverse_in_class_list(void *ptr, size_t size);
static unsigned short get_size_class(size_t size);
static char *get_head_of_class(size_t size);
static void update_head_for_class(uint class, void *new);
static void update_data_header_depending_on_prev_alloc(void* ptr, size_t size);


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

static inline void PUT_PTR(void *p1, void *p2) {
	*(size_t *)(p1) = (size_t)(p2);
    // *p1 = p2;
}

static inline size_t GET_SIZE(void *p) {
	return (size_t)(GET(p) & (~0xf));
}

static int GET_ALLOC(void *p) {
    return (GET(p) & 0x1);
}

static int GET_PREV_ALLOC(void *p) {
    return (GET(p) & 0x2);
}

/* Given block ptr bp, compute address of its header and footer */
static void *HDRP(void *bp) {
  return ((char *)(bp) - WSIZE);
}

static void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

/* get previous free block of same class*/
static void *PREVFREE(void *p){
    // return (void **)(p);
    return (void *)*(size_t *)(p);
}

/*get next free block of previous same class*/
static void *NEXTFREE(void *p){
    size_t *temp = (size_t *)((char*)(p) + WSIZE); //changes
    // return (void*)*(size_t *)((char*)(p) + WSIZE);
    return (void*)*temp;
}

static inline void *PREVFREE_FIELD(void *p) {
	return (void *)((char *)(p));
}

static inline void *NEXTFREE_FIELD(void *p) {
    return (void *)((char *)(p) + WSIZE);

}

/*Move to next block*/
static void *NEXT_BLKP(void *bp) {
  return ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}
/*Move to previous block*/
static void *PREV_BLKP(void *bp) {
  return ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/*Check if this is the last pointer*/
static bool ISLAST(void *p) {
    return NEXTFREE(p) == NULL; // if next points to nothing
}

/*helper functions*/
static unsigned short get_size_class(size_t size) {
    unsigned short i = 0;
	for (i = 0; i < NUM_SIZE_CLASS - 1; ++i) {
		if (size >= (size_t)(1 << (i + 5)) && size < (size_t)(1 << (i + 5 + 1))) {
			return i;
		}
	}

	return NUM_SIZE_CLASS - 1;
}

static char *get_head_of_class(size_t size) {
    unsigned short class = get_size_class(size);
	char *i = heap_start_ptr;
	i += class * WSIZE;
	return i;
}

static void update_head_for_class(uint class, void *new) {
    char* that_ptr = (char *)(heap_start_ptr) + class*WSIZE;
    PUT_PTR(that_ptr, new);
    return;
}

static void update_data_header_depending_on_prev_alloc(void* ptr, size_t size) {
    size_t is_previous_allocated = GET_PREV_ALLOC(HDRP(ptr));
    // data block - no footer

    if (is_previous_allocated == 2) {
        PUT(HDRP(ptr), PACK(size, 3)); // 11
    }
    else {
        PUT(HDRP(ptr), PACK(size, 1)); // 01
    }
    return;
}

static void update_next_bps_prev_alloced_to_true(void* ptr) {
    void* next_ptr = NEXT_BLKP(ptr);
    size_t is_allocated = GET_ALLOC(HDRP(next_ptr));
    size_t size = GET_SIZE(HDRP(next_ptr));
    // printf("unbpatt: next_ptr = %p, size = %lu, alloc = %lu\n", next_ptr, size, is_allocated);

    if (is_allocated) {
        PUT(HDRP(next_ptr), PACK(size, 3)); // 11
    }
    else {
        
        
        PUT(HDRP(next_ptr), PACK(size, 2)); // 10

        // footer only exists if free blk
        PUT(FTRP(next_ptr), PACK(size, 2));
    }
    return;
}



/*
 * Initialize: returns false on error, true on success.
 */

bool mm_init(void)
{
    /* IMPLEMENT THIS */
    /*
        we add the lists ptrs before the pro-blk, as its easy
        intially all will be NULL
    */
    heap_start_ptr = NULL;
    // // printf("\nmm_init: started\n");
    heap_start_ptr = mem_sbrk(NUM_SIZE_CLASS * WSIZE);
    if (heap_start_ptr == (void *)-1) {
        // // printf("mm_init: ended with NULL from mem_sbrk(NUM_SIZE_CLASS * WSIZE)\n");
        return false;
    }

    for (unsigned short i = 0; i < NUM_SIZE_CLASS; i++) {
        PUT_PTR((char *)heap_start_ptr + (i * WSIZE), NULL);
    }

    /*
        add pro and epi
        - we add a temp ptr to know the start as we increase the size, might loose the start
    */
    void *temp = heap_start_ptr;
    
    heap_start_ptr = mem_sbrk(4*WSIZE); // need 32 for exactly fitting the padding + pro + epi
    if (heap_start_ptr == (void *)-1) {
        // // printf("mm_init: ended with NULL from mem_sbrk(8*WSIZE)\n");
        return false;
    }

    PUT(heap_start_ptr, 0);
    PUT(heap_start_ptr+(WSIZE), PACK(DSIZE, 3)); // prologue header
    PUT(heap_start_ptr+(2*WSIZE), PACK(DSIZE, 3)); // prologue footer

    PUT(heap_start_ptr+(3*WSIZE), PACK(0, 3)); // epilogue header

    /*
        set the start back to free_ptrs
    */
    heap_data_ptr = heap_start_ptr;
    heap_start_ptr = temp;

    if (extend_heap(CHUNKSIZE) == NULL) {
        // // printf("mm_init: ended with NULL from extend_heap(CHUNKSIZE)\n");
        return false;
    }
    // // printf("mm_init: heap_start_ptr = %p\n", heap_start_ptr);
    // // printf("mm_init: ended\n");
    // heap_end_ptr = mem_heap_hi();
    return true;
}

static void *extend_heap(size_t size) {
    // // printf("extend_heap: started with size - %lu\n", size);
    if (size >= MIN_SIZE) {
        // // printf("extend_heap: size >= min\n");
        size = align(size);
    }
    else {
        // // printf("extend_heap: size < min\n");
        size = align(size);
        size = MIN_SIZE;
    }
    
    void *first_end = mem_sbrk(size);
    if (first_end == (void *)-1) {
        // // printf("extend_heap: ended with NULL from mem_sbrk\n");
        return NULL;
    }

    // // printf("Extend Heap: first_end = %p\n", first_end);
    PUT(HDRP(first_end), PACK(size, GET_PREV_ALLOC(HDRP(first_end))));
    PUT(FTRP(first_end), PACK(size, GET_PREV_ALLOC(HDRP(first_end))));
    PUT(HDRP(NEXT_BLKP(first_end)), PACK(0, 1)); // new epi - 01
    
    add_blk(first_end);
    // // printf("extend_heap: ended\n");
    return (coalesce(first_end));
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    // // printf("malloc: started with size - %lu\n", size);
    /* IMPLEMENT THIS */
    if(size == 0){
        // // printf("malloc: ended with size == 0\n");
        return NULL;
    }

    if (size > DSIZE) {
        // // printf("malloc: size > DSIZE\n");
        size = align(size + WSIZE); // now we only need to store header
    }
    else {
        // // printf("malloc: size <= DSIZE\n");
        size = MIN_SIZE;
    }

    void *found = best_fit(size);
    // if (found != (void *)-1) { //was equal meant best_fit failed
    if (found != NULL) { //was equal meant best_fit failed
        // place in list
        // // printf("malloc: found place in list - best fit\n");
        place(found, size);
        // // printf("malloc: ended with found - %p\n", found);
        return found;
    }

    // // printf("malloc: no place found in list - need to extend\n");
    size_t extended_size = MAX(size, CHUNKSIZE);
    found = extend_heap(extended_size);
    
    // if (found == (void *)-1) {
    if (!found) {
        return NULL;
    }

    // place in list
    place(found, size);
    // // printf("malloc: ended with found - %p\n", found);
    return found;
}

static void *best_fit(size_t size) {
    /* IMPLEMENT THIS */
    // // printf("best_fit: started\n");
    unsigned short pclass = get_size_class(size);
    char *head_ptr = get_head_of_class(size);
    // // printf("best_fit: pclass = %hu, head_ptr = %p\n", pclass, head_ptr);

    while (pclass < NUM_SIZE_CLASS) {
        // // printf("best_fit while: pclass = %hu, head_ptr = %p\n", pclass, head_ptr);
        void *ptr_val = (void*)*(size_t*)head_ptr;
        if (ptr_val != NULL) {
            void *found = traverse_in_class_list(ptr_val, size);
            // if (found != (void *)-1) {
            if (found != NULL) {
                // // printf("best_fit: found\n");
                // // printf("best_fit: ended\n");
                return found;
            }
        }

        pclass += 1;
        head_ptr += WSIZE;
    }
    // // printf("best_fit: ended with NULL\n");
    return NULL;
}

static void *traverse_in_class_list(void *ptr, size_t size) {
    // // printf("traverse_in_class_list: started\n");
	void *i = ptr;
	while (i != NULL) {
        size_t csize = GET_SIZE(HDRP(i));
        // size_t is_allocated = GET_ALLOC(HDRP(ptr));
        size_t is_allocated = GET_ALLOC(HDRP(i));

		if ((size <= csize) && !is_allocated) {
            // // printf("traverse_in_class_list: found at %p\n", i);
            // // printf("traverse_in_class_list: ended\n");
            return i;
        }

		i = NEXTFREE(i);
	}
    // // printf("traverse_in_class_list: ended\n");
	return NULL;
}

static void place(void *ptr, size_t size) {
    /* IMPLEMENT THIS */
    // printf("place: started\n");
    size_t csize = GET_SIZE(HDRP(ptr));

    delete_blk(ptr);

    if (csize - size >= MIN_SIZE) {
        // need to split
        // printf("place: need to split\n");
        
        // data block
        update_data_header_depending_on_prev_alloc(ptr, size);

        // split block is free and previos is allocated so 10
        ptr = NEXT_BLKP(ptr);
		PUT(HDRP(ptr), PACK(csize - size, 2));
		PUT(FTRP(ptr), PACK(csize - size, 2));
        
        add_blk(ptr);
    }
    else {
        // printf("place: no spliting\n");
        
        // data block
        update_data_header_depending_on_prev_alloc(ptr, csize);

        // update next bp to prev alloc is true
        update_next_bps_prev_alloced_to_true(ptr);
    }

    // printf("place: ended\n");
    return;
}

/*
 * free
 */
void free(void* ptr)
{
    // // printf("free: started for ptr = %p\n", ptr);
    /* IMPLEMENT THIS */
    if (ptr == NULL) {
        // // printf("free: ptr = %p is NULL\n", ptr);
        // // printf("free: ended\n");
        return;
    }

    size_t size = GET_SIZE(HDRP(ptr));
    // // printf("free: ptr = %p is of size = %lu\n", ptr, size);

    size_t is_prev_allocated = GET_PREV_ALLOC(HDRP(ptr));

    if (is_prev_allocated == 2) {
        PUT(HDRP(ptr), PACK(size, 2)); // 10
        PUT(FTRP(ptr), PACK(size, 2)); // 10
    }
    else {
        PUT(HDRP(ptr), PACK(size, 0)); // 00
        PUT(FTRP(ptr), PACK(size, 0)); // 00
    }

    void* next_ptr = NEXT_BLKP(ptr);
    size_t next_size = GET_SIZE(HDRP(next_ptr));
    size_t next_allocated = GET_ALLOC(HDRP(next_ptr));
    
    if (next_allocated) {
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(next_size, 1)); // 01
    }
    else {
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(next_size, 0)); // 00
    }
    add_blk(ptr);
    coalesce(ptr);

    // // printf("free: ended\n");
    return;
}

static void *coalesce(void *ptr)
{
    /* IMPLEMENT THIS */
    // // printf("coalesce: started for ptr = %p\n", ptr);

    size_t size = GET_SIZE(HDRP(ptr));
    size_t is_previous_allocated = GET_PREV_ALLOC(HDRP(ptr));
    size_t is_next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

    if (is_previous_allocated && is_next_allocated) {
        // // printf("coalesce: both allocated\n");
        // // printf("coalesce: ended\n");
        return ptr;
    }
    else if (is_previous_allocated && !is_next_allocated) {
        /* 
        if next is free, then merge, size will increase and the header and footer need to update
            - update header to 0
            - update footer to 0
            - for both the size with be present + next size
        */
        // // printf("coalesce: combine with next\n");
        delete_blk(NEXT_BLKP(ptr));
        delete_blk(ptr);

        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));

        PUT(HDRP(ptr), PACK(size, 2)); // 10
        PUT(FTRP(ptr), PACK(size, 2)); // 10
    }
    else if (!is_previous_allocated && is_next_allocated) { //incorrect variable changed
        /* 
        if previous is free, then merge, size will increase and the header and footer need to update
            - update footer to 0
            - update previous header to 0
            - update size with previous + present
            - return privous blk
        */
        // // printf("coalesce: combine with prev\n");
        delete_blk(PREV_BLKP(ptr));
        delete_blk(ptr);

        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 2));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 2));

        ptr = PREV_BLKP(ptr);
    }
    else {
        /*
        if both are free, merge all three
            - update the header of previous to 0
            - update the footer of next to 0
            - size with be addition of all theree
            - return the privous blk
        */
        // // printf("coalesce: combine with both next and prev\n");
        delete_blk(PREV_BLKP(ptr));
        delete_blk(NEXT_BLKP(ptr));
        delete_blk(ptr);

        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        size += GET_SIZE(FTRP(NEXT_BLKP(ptr)));

        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 2));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 2));

        ptr = PREV_BLKP(ptr);
    }

    add_blk(ptr);
    // // printf("coalesce: ended\n");
    return ptr;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    void *updated_ptr;

    if (oldptr == NULL) {
        return malloc(size);
    }

    if (size == 0) {
        free(oldptr);
        return NULL;
    }

    updated_ptr = malloc(size);

    if (updated_ptr == NULL) {
        return NULL;
    }

    size_t cp_size = MIN(size, GET_SIZE(HDRP(oldptr)));
	memcpy(updated_ptr, oldptr, cp_size);

	free(oldptr);

	return updated_ptr;
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

static void print_blk(void* ptr) {
    /*
        - see if free/allocated
        - Header - size + present alloc

        - Header - size and present alloc
        - prev ptr
        - present ptr
        - Footer
    */

    size_t is_present_allocated = GET_ALLOC(HDRP(ptr));
    size_t size = GET_SIZE(HDRP(ptr));
    size_t footer_size = GET_SIZE(FTRP(ptr));
    size_t is_present_allocated_in_footer = GET_ALLOC(FTRP(ptr));

    if (is_present_allocated) {
        printf("ptr - %p | Header Alloc = %s | Header size = %lu | Footer Alloc = %s | Footer size = %lu\n", ptr, is_present_allocated ? "True" : "False", size, is_present_allocated_in_footer ? "True" : "False", footer_size);
    }
    else {
        printf("ptr - %p | Header Alloc = %s | Header size = %lu | Prev ptr = %p | Next ptr = %p | Footer Alloc = %s | Footer size = %lu\n", ptr, is_present_allocated ? "True" : "False", size, PREVFREE(ptr), NEXTFREE(ptr), is_present_allocated_in_footer ? "True" : "False", footer_size);
    }
}

static void add_blk(void *bp) {
    /* IMPLEMENT THIS */
    // // printf("add_blk: started for ptr = %p\n", bp);
    size_t size = GET_SIZE(HDRP(bp));
    char *head_ptr = get_head_of_class(size);
    uint class = get_size_class(size);
    void* ptr_val = (void *)*(size_t *)head_ptr;

    if(ptr_val != NULL) {
        // // printf("add_blk: need to make as head\n");
        PUT_PTR(NEXTFREE_FIELD(bp), ptr_val);
        PUT_PTR(PREVFREE_FIELD(bp), NULL); // as it is the head
        PUT_PTR(PREVFREE_FIELD(ptr_val), bp);
    }
    else {
        // // printf("add_blk: head of class is NULL\n");
        PUT_PTR(NEXTFREE_FIELD(bp), NULL);
        PUT_PTR(PREVFREE_FIELD(bp), NULL);
    }

    // PUT_PTR(head_ptr, bp); // update the head - missed this
    update_head_for_class(class, bp);
    // // printf("add_blk: ended, head_ptr = %p\n", head_ptr);
    return;
}

static void delete_blk(void *bp) {
    /* IMPLEMENT THIS */
    // Deleting a block from list
    // // printf("delete_blk: started for ptr = %p\n", bp);
    size_t size = GET_SIZE(HDRP(bp));
    // char *head_ptr = get_head_of_class(size);

    if (PREVFREE(bp) == NULL) {
        // bp is the head
        if (NEXTFREE(bp) == NULL) {
            // class is empty
            // PUT_PTR(head_ptr, NULL);
            update_head_for_class(get_size_class(size), NULL);
        }
        else {
            // set next as root and set the prev free ptr for next as null
            PUT_PTR(PREVFREE_FIELD(NEXTFREE(bp)), NULL);
            // PUT_PTR(head_ptr, NEXTFREE(bp));
            update_head_for_class(get_size_class(size), NEXTFREE(bp));

        }
    }
    else {
        if (NEXTFREE(bp) == NULL) {
            // this is at the end
            PUT_PTR(NEXTFREE_FIELD(PREVFREE(bp)), NULL);
        }
        else {
            // middle linked list removal
            PUT_PTR(PREVFREE_FIELD(NEXTFREE(bp)), PREVFREE(bp));
            PUT_PTR(NEXTFREE_FIELD(PREVFREE(bp)), NEXTFREE(bp));
        }
    }
    // // printf("delete_blk: ended\n");
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    /*
        Things we need to check:
        -   if data block
            - header present, allocated bit true, next block's prev alloc bit is true

        - if free block
            - header == footer
            - alloc bit is false
            - next block's prev alloc bit is false
    */
    // heap size check
    size_t curr_heap_size = mem_heap_hi() - mem_heap_lo();
    printf("heapchecker\n");
    if(curr_heap_suze != heap_size){
        printf("Current Heap Size = %lu, Expected Heap Size = %lu\n", curr_heap_size, heap_size);
        printf("Heap Sizes not equal!\n");
        return false;
    }
    // Checking just allocated blocks
    void *ptr = heap_data_ptr;
    while(ptr <= heap_end_ptr){
        size_t hsize = GET_SIZE(HDRP(ptr));
        size_t h_alloc = GET_ALLOC(HDRP(ptr));

        if(h_alloc == 1){// Allocated block
            if(GET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr))) != 2){
                printf("Next block says current (%p)is not allocated\n", ptr);
            }
        } 
    }

    // Checking free blocks of heaps
    for(int c = 0; c < NUM_SIZE_CLASS; c++){
        void *curr_head = *((size_t *)heap_start_ptr + c);

        while(curr_head){
            size_t hsize = GET_SIZE(HDRP(curr_head));
            size_t fsize = GET_SIZE(FTRP(curr_head));
            if(hsize != fsize){
                printf("Header (%lu) and Footer (%lu) sizes are different\n", hsize, fsize);
                return false
            }

            if(GET_ALLOC(HDRP(curr_head)) != 0){
                printf("Block set to allocated in header, but is actually free\n");
                return false;
            }

            if(GET_ALLOC(FTRP(curr_head)) != 0){
                printf("Block set to allocated in footer, but is actually free\n");
                return false;
            }

            if(GET_PREV_ALLOC(NEXT_BLKP(curr_head)) != 0){
                printf("Next block says free block is not free\n");
                return false;
            }
            curr_head = NEXTFREE(curr_head);
        }


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