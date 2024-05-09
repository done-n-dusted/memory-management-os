# OS Project 1

## Group Members
* Sai Naveen Katla (svk6448@psu.edu)
* Anurag Pendyala (app5997@psu.edu)

# Checkpoint 1 Work

### Sai Naveen Katla's Contributions

* Created util functions like HDRP(), FTRP(), GET_SIZE(), etc.
* free and coaleseBlk functions
* check_heap epilgoue and prologue check

### Anurag Pendyala's Contributions
* mem_init and extend_heap
* malloc
* check_heap alloc and size check

# Final Submission Work

## Idea: Segregated Free Lists
### Sai Naveen Katla's Contributions
- `free()`, `realloc()`
- `coalesce()`
- `place()`
- Extracting and Updating header components for a particular size-class
- Printing blocks for debugging
- `heap_checker()` for free blocks

### Anurag Pendyala's Contributions
- `mm_init()`, `malloc()`
- `best_fit()`
- Inline Functions like `NEXTFREE`, `PREVFREE`
- `add_blk()` and `delete_blk()`
- Remaining `heap_checker()`

# References
- Randal E. Bryant and David R. O'Hallaron. 2015. Computer Systems: A Programmer's Perspective (3rd. ed.). Pearson.
- Lecture Slides, CMPSC473 - Operating System Desgin 2023, Instructor: Ruslan Nikolaev, The Pennsylvania State University.