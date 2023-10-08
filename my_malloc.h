#include <stdio.h>
#include <stdlib.h>

struct metadata{
    struct metadata * prev;
    struct metadata * next;
    size_t size; 
};
typedef struct metadata meta;

struct freelist{
  meta * head;
  meta * tail;
};
typedef struct freelist flist;

//Thread Safe malloc/free: locking version 
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

//Thread Safe malloc/free: non-locking version 
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

// void * ff_malloc(size_t size);
// void ff_free(void * ptr);

void * bf_malloc(size_t size,flist * fl,int lock_status);
void bf_free(void * ptr,flist * fl);

void * malloc_helper(size_t size,meta * p,flist * fl,int lock_status);
void free_region(void * ptr,flist * fl);
void insert_freelist(meta * current,flist * fl);
void remove_freelist( meta * current,flist * fl);
void * split_region(size_t size,meta * ptr,flist * fl);
meta * compare(size_t size,meta * p,meta * best);
void merge_back(meta * current);
void merge_front(meta * current);

// void print_freeList();

unsigned long entire_heap_memory = 0;
// unsigned long get_data_segment_size();             //in bytes
// unsigned long get_data_segment_free_space_size();  //in bytes