#include "my_malloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER; // Used for locking version

flist list={NULL,NULL};

// meta * freelisthead=NULL;
// meta * freelisttail=NULL;
__thread flist tls_list = {NULL, NULL};

// locking version
void *ts_malloc_lock(size_t size){
    //lock
    pthread_mutex_lock(&lock);
    void * ans=bf_malloc(size,&list,1);
    //unlock
    pthread_mutex_unlock(&lock);
    return ans;
}
void ts_free_lock(void *ptr){
    pthread_mutex_lock(&lock);
    bf_free(ptr,&list);
    pthread_mutex_unlock(&lock);
}
// non-locking version
void *ts_malloc_nolock(size_t size){
    void * ans=bf_malloc(size,&tls_list,0);
    return ans;
}
void ts_free_nolock(void *ptr){
    bf_free(ptr,&tls_list);
}


// Called by ff_malloc and bf_malloc
// Input: requested size, pointer(point to the metadata of the fit free region)
// Output: the pointer pointing to data(instead of metadata)
void * malloc_helper(size_t size,meta * p,flist * fl,int lock_status){
    // fprintf(stderr,"start allocate\n");
    if (p!=NULL){
        // find fit free region
        return split_region(size,p,fl);
    }
    else{
        // do not find fit free region
        // use sbrk (size of metadata + requested size)
        meta * thedatanew;

        if(lock_status==0){
        //lock: sbrk
        pthread_mutex_lock(&lock);
        thedatanew = sbrk(sizeof(meta) + size);
        //unlock
        pthread_mutex_unlock(&lock);}
        else{
            // already locked: lock_status==1
            thedatanew = sbrk(sizeof(meta) + size);
        }

        if (thedatanew == (meta*)(-1)) {
            return NULL;
        }
        entire_heap_memory = entire_heap_memory+sizeof(meta) + size;
        thedatanew->prev=NULL;
        thedatanew->next=NULL;
        thedatanew->size=size;
        // fprintf(stderr,"end allocate2\n");
        return (void *)thedatanew + sizeof(meta);
    }
}


// If there is enough space, split the area according to the required size; 
// if there is not enough space, delete the area directly from the freelist.
// Input: requested size, pointer(point to the metadata of the fit free region)
// Output: the pointer pointing to data(instead of metadata)
void * split_region(size_t size, meta * p,flist * fl){
    // fprintf(stderr,"start split\n");
    if(p->size > size + sizeof(meta)){
        // enough to split
        // allocate the later part of the block according to the required size
        // no need to change prev/next pointers of p
        p->size=p->size-size-sizeof(meta);
        meta * splittedregion=(meta *)((void *)(p) + sizeof(meta)+ p->size);
        splittedregion->prev=NULL;
        splittedregion->next=NULL;
        splittedregion->size=size;
        return (void *)splittedregion+ sizeof(meta);
    }
    else{ 
        //p->size == size + sizeof(meta) or p->size == size
        // can not split
        p->size=size; // reset size
        remove_freelist(p,fl);    
        return (void *)p + sizeof(meta);
    }
}

// Called by ff_free and bf_free
void free_region(void *ptr,flist * fl){
    if(ptr == NULL){
        return;
    }
    // current points to metadata of the freed_region
    // fprintf(stderr,"enter free :\n");
    meta * current=(meta *)(ptr - sizeof(meta));
    insert_freelist(current,fl);
    // fprintf(stderr,"finish free .\n");
}


// when region is freed, insert the region to freelist in order，
// and merge the adjacent free regions
void insert_freelist(meta * current,flist * fl){
    if(fl->head==NULL && fl->tail==NULL){
        fl->head=current;
        fl->tail=current;
    }
    //add into linkedlist freelist
    else if(current<fl->head){
        // update head of freelist
        fl->head->prev=current;
        current->next=fl->head;
        current->prev=NULL;
        fl->head=current;
        merge_back(current);
    }
    else if(current>fl->tail){
        // update tail of freelist
        fl->tail->next=current;
        current->prev=fl->tail;
        current->next=NULL;
        fl->tail=current;
        merge_front(current);
    }
    else{
    // insert in order   
    // merge the back when satisfied
    // merge the front when satisfied
        meta * p=fl->head;
        while(p!=NULL){
            if(p>current){
                // find the location to insert
                // insert the free region in front of the p
                p->prev->next=current;
                current->prev=p->prev;
                current->next=p;
                p->prev=current;
                merge_back(current);
                merge_front(current);
                break;
            }
            p=p->next;
        }    
    }  
}

// if adjacent to the back，merge current and current->next
void merge_back(meta * current){
    if(current->next!=NULL){
        if(((void *)current+sizeof(meta)+current->size)==(void *)current->next){
            meta * med=current->next;
            // fprintf(stderr,"merge 1---------\n");
            // fprintf(stderr,"%zu\n",current->size);
            // fprintf(stderr,"%zu\n",current->next->size);

            current->size=current->size + (size_t)(sizeof(meta) + current->next->size);

            if(med->next!=NULL){
            current->next=med->next;
            med->next->prev=current;
            }
            else{//med->next==NULL
                current->next=NULL;
            }
        }
    } 
}
// if adjacent to the front，merge current and current->prev
void merge_front(meta * current){
    if(current->prev!=NULL){
        if(((void *)current-current->prev->size-sizeof(meta))==(void *)current->prev){
            // fprintf(stderr,"merge 2---------\n");
            // fprintf(stderr,"%zu\n",current->size);
            // fprintf(stderr,"%zu\n",current->prev->size);

            current->prev->size=current->prev->size + (size_t)(sizeof(meta) + current->size);
            
            if(current->next!=NULL){
            current->prev->next=current->next;
            current->next->prev=current->prev;

            }
            else{
                current->prev->next=NULL;
            }
        }
    }
}


// when region is allocated,
// remove the region(metadata+size) from the freelist
void remove_freelist( meta * current,flist * fl){
    if (( fl->tail == fl->head) && (fl->head == current)){
        // if there is only one region in freelist
        fl->head=NULL;
        fl->tail=NULL;
    }
    else{
        if(current->prev!=NULL){
        current->prev->next=current->next;}
        else{
            fl->head=current->next;
        }

        if(current->next!=NULL){
        current->next->prev=current->prev;}
    }
}


// Best Fit malloc
// get best fit allocation address
void *bf_malloc(size_t size,flist * fl,int lock_status){
    meta * p=fl->head;
    meta * currentbest=NULL;
    int flag=0; // used to first update the currentbest
    while(p!=NULL){
        if(p->size == size){
            // ends the loop immediately when find the best fit
            currentbest=p;
            break;
        }
        if(p->size > size){
            if(flag==0){
                currentbest=p;
                flag=1;
            }
            currentbest=compare(size,p,currentbest);
        }
        p=p->next;
    }
    //check the pointer
    return malloc_helper(size,currentbest,fl,lock_status);

}


// compare the currentbest fit region and the other fit region
// update the currentbest fit region if needed
meta * compare(size_t size,meta * p,meta * best){
    // compare which one is the best fit
    if( best->size > p->size ){
        return p;
    }
    else{
        return best;
    }
}


// Best Fit free
void bf_free(void *ptr,flist * fl){
    free_region(ptr,fl);
    // print_freeList();
}


// unsigned long get_data_segment_size(){
//     return entire_heap_memory;
// }

// unsigned long get_data_segment_free_space_size(){
//     unsigned long sum=0;
//     meta * p=freelisthead;
//     while(p!=NULL){
//         sum=sum+p->size+sizeof(meta);
//         p=p->next;
//     }
//     return sum;
// }

// void print_freeList() {
//   meta * p = freelisthead;
//   while (p != NULL) {
//     printf("p: %p, p->size: %lu\n", p, p->size);
//     p = p->next;
//   }
//   printf("\n\n");
// }