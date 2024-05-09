# OS Project 1

## Overview

This is a custom memory allocator implemented using segregated free lists and a linked list structure. The allocator provides functionality for memory allocation, deallocation, and reallocation, optimizing memory usage and performance. This was done as part of the course CMPSC473 - Operating Systems at the Pennsylvania State University -- University Park.

## Features

*   Segregated free lists of various size classes for efficient memory management.
*   Allocation: Blocks are allocated from appropriate size classes, with splitting and extension of the heap if necessary.
*   Free: Free blocks are coalesced to form larger blocks for reutilization.
*   Realloc: Combination of malloc and realloc functionalities for resizing memory blocks.
*   Footer Optimization: Efficient memory management by utilizing footer information for coalescing blocks.

## Implementation Details

### Segregated Free Lists

*   The allocator utilizes segregated free lists to manage memory blocks of different sizes efficiently.
*   Each size class corresponds to a range of block sizes, with pointers pointing to the first free block of each class.

### Allocation

*   Upon malloc request, the allocator traverses through the appropriate size class to find a suitable block.
*   If a suitable block is not found, the allocator iterates through other size classes, splitting blocks if necessary, or extends the heap.
*   The allocation process ensures optimal memory usage and minimizes fragmentation.

### Deallocation

*   Upon freeing a block, adjacent free blocks are coalesced to form larger blocks.
*   The coalesced block is updated in the appropriate size class for future allocations.

### Reallocation

*   Realloc combines the functionalities of malloc and realloc.
*   It reallocates memory blocks, possibly moving them to a new location if necessary.

### Footer Optimization

*   Footer information is utilized to optimize memory management.
*   Every data block's header includes a bit indicating whether the previous block is allocated, facilitating coalescing.

Usage
-----

To use the custom memory allocator:

1.  Include the allocator source files in your project.
2.  Call `mm_init()` to initialize the allocator.
3.  Use `malloc`, `free`, and `realloc` functions provided by the allocator for memory management.

## References
- Randal E. Bryant and David R. O'Hallaron. 2015. Computer Systems: A Programmer's Perspective (3rd. ed.). Pearson.
- Lecture Slides, CMPSC473 - Operating System Desgin 2023, Instructor: Ruslan Nikolaev, The Pennsylvania State University.
