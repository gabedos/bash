#include "malloc.h"

// malloc.c created by Gabriel Dos Santos on 12/16/2021

// Important structs and defines used in malloc implementation

typedef struct ptr_with_size{
    void * ptr;
    long size;
} ptr_with_size;

//Metadata struct is 8 byte aligned * important for system optimization *
struct header {
    struct header * prev;
    struct header * next;
    long size;
};
typedef struct header * Header;

#define INTMAX (2147483647)

#define PSUB(x, y) ((uintptr_t) x - (uintptr_t) y)
#define PADD(x, y) ((uintptr_t) x + (uintptr_t) y)

#define FREED (0)
#define USED (1)
#define ALIGNMENT (8)

#define ALLOCSIZE(x) ((int) (x + sizeof(struct header)))
#define SPLITSIZE(x) ((int) (x + sizeof(struct header) + ALIGNMENT))

void printFreeList();
void printAllocList();
void printLastMalloc();

char HeapInitalized = 0;
Header lastMalloc = NULL;

int HeapInitalizer() {

    Header old_break = (Header) sbrk(sizeof(struct header));

    //Insufficient space in Heap to create header
    if(old_break == (void *) -1){
        return 1;
    }

    //Initalize the Heap header
    old_break->prev = NULL;
    old_break->next = NULL;
    old_break->size = 0;
    HeapInitalized = 1;

    return 0;

}

void printFreeList() {

    int i = 0;
    for(Header freeTop = (Header) PSUB(sbrk(0), sizeof(struct header)); freeTop != NULL; freeTop = freeTop->prev){
        app_printf(2, "%d:%u:%u ", i++, freeTop->size, freeTop);
    }

}

void printLastMalloc() {
    app_printf(2, "LastMalloc: %u\n ", lastMalloc);
}

void printAllocList(){

    int i = 0;
    for(Header allocTop = lastMalloc; allocTop != NULL; allocTop = allocTop->prev){
        app_printf(2, "%d:%u:%u ", i++, allocTop->size, allocTop);
    }

}

// void * freeListAdd(uint64_t numbytes);
// Attempts to add an allocation into the free list
// On success it will add the new allocation and stitches together the list
// On failure it will return NULL and callee must deal with requesting more memory via sbrk
void * freeListAdd(uint64_t numbytes, Header currBlock) {

    //Won't fit in the freeList
    if(numbytes > INTMAX){
        return NULL;
    }

    //Traverse the free list
    for(; currBlock != NULL; currBlock = currBlock->prev) {

        //Found a suficiently large enough region
        if(currBlock->size >= (int) numbytes) {

            //Determine whether remaining free space is useable (ie enough space for a header and smallest block)
            if(currBlock->size >= SPLITSIZE(numbytes)) {
                // Split the region
                // STEP 1: UPDATE THE FREE LIST (replace node with new)

                Header newFreeElt = (Header) PADD(currBlock, ALLOCSIZE(numbytes));

                newFreeElt->next = currBlock->next;
                newFreeElt->prev = currBlock->prev;

                if(newFreeElt->next){
                    newFreeElt->next->prev = newFreeElt;
                }

                if(newFreeElt->prev){
                    newFreeElt->prev->next = newFreeElt;
                }

                newFreeElt->size = currBlock->size - ALLOCSIZE(numbytes);
                currBlock->size = numbytes;

            } else {
                // Claim the whole region
                // STEP 1: UPDATE THE FREE LIST (evict the current node and stitch)

                Header prevFreeElt = currBlock->prev;
                Header nextFreeElt = currBlock->next;
                assert(currBlock->size >= (long int) numbytes);

                if(prevFreeElt){
                    prevFreeElt->next = prevFreeElt->next->next;
                }

                if(nextFreeElt){
                    nextFreeElt->prev = nextFreeElt->prev->prev;
                }

            }

            // STEP 2: UPDATE THE ALLOC LIST

            if(!lastMalloc){
                //Empty alloc list (this is the first element)
                lastMalloc = currBlock;
                lastMalloc->prev = NULL;
                lastMalloc->next = NULL;

            } else {

                //Just add to the end of the alloc list
                lastMalloc->next = currBlock;
                currBlock->next = NULL;
                currBlock->prev = lastMalloc;
                lastMalloc = currBlock;

            }

            // STEP 3: RETURN THE NEWLY ALLOC'D POINTER

            return (void *) PADD(currBlock, sizeof(struct header));

        }
    
    }

    //Unavailable opening in free list
    return NULL;

}

// heapTopAdd(uint64_t numbytes, Header currBlock)
// Adds a new element to the end of the free list (pre-gauranteed space)
// Upon success it returns a pointer to the payload
void * heapTopAdd(uint64_t numbytes, Header currBlock) {

    // STEP 1: UPDATE THE FREE LIST

    Header newFreeHead = (Header) PADD(currBlock, ALLOCSIZE(numbytes));

    if(currBlock->prev){
        currBlock->prev->next = newFreeHead;
    }

    newFreeHead->prev = currBlock->prev;
    newFreeHead->next = NULL;
    newFreeHead->size = 0;

    // STEP 2; UPDATE THE ALLOC LIST

    currBlock->prev = lastMalloc;
    currBlock->next = NULL;
    currBlock->size = numbytes;

    if(lastMalloc){
        lastMalloc->next = currBlock;
    }

    lastMalloc = currBlock;

    return (void *) PADD(currBlock, sizeof(struct header));

}

void * malloc(uint64_t numbytes) {

    //Sizeless alloc: do nothing
    if(!numbytes || (numbytes > INTMAX)){
        return NULL;
    }

    numbytes = ROUNDUP(numbytes, 8);

    //First time calling malloc, must initialize free list
    if(!HeapInitalized){
        if(HeapInitalizer()){
            //Failed to initalize the heap
            return NULL;
        }
    }

    //Initialize variables
    Header freeListHead = (Header) PSUB(sbrk(0), sizeof(struct header));

    //Check whether alloc fits within the free list
    void * returnPointer = NULL;
    if((returnPointer = freeListAdd(numbytes, freeListHead))) {
        return returnPointer;
    }
    //Failed first-fit free-list lookup

    //Check sufficient space in remaining heap
    if((void *) -1 == sbrk(ALLOCSIZE(numbytes))) {
        return NULL;
    }

    //Add allocation to the top of the heap
    if((returnPointer = heapTopAdd(numbytes, freeListHead))){
        return returnPointer;
    }

    //Every attempt has failed (should not reach here)
    return NULL;

}


// void * findAddrFit(Header freeBlock, Header currBlock)
// Traverses a linked list to find an element 
// that fits in between two nodes to maintain order
void * findAddrFit(Header freeBlock, Header currBlock) {

    Header lastElt = NULL;
    for(; freeBlock < currBlock; currBlock = currBlock->prev) {
        lastElt = currBlock;
    }

    return lastElt;
}


void free(void *firstbyte) {

    //Null pointer: do nothing
    if(!firstbyte){
        return;
    }

    Header freeBlock = (Header) PSUB(firstbyte, sizeof(struct header));
    Header freeListHead = (Header) PSUB(sbrk(0), sizeof(struct header));

    // STEP 1: UPDATE THE ALLOC LIST

    Header prevAlloc = freeBlock->prev;
    Header nextAlloc = freeBlock->next;

    if(prevAlloc){
        prevAlloc->next = prevAlloc->next->next;
    }

    if(nextAlloc){
        nextAlloc->prev = nextAlloc->prev->prev;
    }

    if(lastMalloc == freeBlock) {
        lastMalloc = prevAlloc;
    }

    // STEP 2: UPDATE THE FREE LIST

    Header nextFreeElt = findAddrFit(freeBlock, freeListHead);

    Header prevFreeElt = nextFreeElt->prev;

    if(prevFreeElt){
        prevFreeElt->next = freeBlock;
    }

    if(nextFreeElt){
        nextFreeElt->prev = freeBlock;
    }

    freeBlock->prev = prevFreeElt;
    freeBlock->next = nextFreeElt;

    return;
}


void * calloc(uint64_t num, uint64_t sz) {

    Header oldHeapHead = (Header) PSUB(sbrk(0), sizeof(struct header));

    //Integer overflow check
    if((num * sz) / sz != num){
        return NULL;
    }

    void * newAddr = malloc(sz*num);

    //Insufficient memory
    if(!newAddr){
        return NULL;
    }

    memset(newAddr, 0, sz*num);

    return newAddr;
}


void * realloc(void * ptr, uint64_t sz) {

    // Free the old pointer
    free(ptr);

    //Special Cases:

    // (1) No preexisting data so just allocate
    if(ptr == NULL){
        return malloc(sz);
    }

    // (2) No new size so just free
    if(sz == 0){
        return NULL;
    }

    // Regular Case:

    // 1) Malloc a new block of smaller sz
    void * newAlloc = malloc(sz);

    if(newAlloc == NULL){
        return NULL;
    }

    uint64_t copySize;
    Header oldPtrHead = (Header) PSUB(ptr, sizeof(struct header));

    if(sz > (uint64_t) oldPtrHead->size){
        copySize = oldPtrHead->size;
    } else {
        copySize = sz;
    }

    // 2) memcpy the old into the new
    memcpy(newAlloc, ptr, copySize);

    return newAlloc;

}


void defrag() {

    Header freeListTop = (Header) PSUB(sbrk(0), sizeof(struct header));
    freeListTop = freeListTop->prev;    //Don't include heapHead into coalescing

    Header freeListSubTop;

    if(freeListTop){
        freeListSubTop = freeListTop->prev;
    }

    for(; freeListTop != NULL; freeListTop = freeListTop->prev, freeListSubTop = freeListSubTop->prev) {

        //There is no element capable of coalescing
        if(freeListSubTop == NULL) {
            break;
        }

        //Determine whether adjacent elements
        if(freeListTop == (Header) PADD(freeListSubTop, ALLOCSIZE(freeListSubTop->size))) {

                freeListSubTop->size += ALLOCSIZE(freeListTop->size);

                freeListSubTop->next = freeListTop->next;
                
                if(freeListTop->next){
                    freeListTop->next->prev = freeListSubTop;
                }

        }

    }

}

int heap_info(heap_info_struct * info) {

    Header freeHead = (Header) PSUB(sbrk(0), sizeof(struct header));
    Header allocHead = lastMalloc;

    //Acquire free-list info (free space and largest free check)
    int freeSpace = 0;
    int biggestFree = 0;

    for(Header currBlock = freeHead; currBlock != NULL; currBlock = currBlock->prev){
        int currSize = currBlock->size;
        freeSpace += ALLOCSIZE(currSize);
        if(currSize > biggestFree){
            biggestFree = currSize;
        }
    }

    info->free_space = freeSpace;
    info->largest_free_chunk = biggestFree;

    //Acquire malloc-list info (length, allocSize, allocPointer (in descending order))
    int allocLength = 0;
    for(Header currBlock = allocHead; currBlock != NULL; currBlock = currBlock->prev) {
        allocLength++;
    }

    info->num_allocs = allocLength;

    //No allocations present in the heap
    if(allocLength == 0){
        info->size_array = NULL;
        info->ptr_array = NULL;
        return 0;
    }

    //Allocating necessary arrays
    long * sizeArray = malloc(sizeof(long)*allocLength);
    void ** ptrArray = malloc(sizeof(void *)*allocLength);

    ptr_with_size * allocList = malloc(sizeof(struct ptr_with_size)*allocLength);

    if(!(sizeArray && ptrArray && allocList)){
        //Insufficient space to complete the info struct/sort
        free(sizeArray);
        free(ptrArray);
        free(allocList);
        return -1;
    }

    int index = 0;

    for(Header currBlock = allocHead; currBlock != NULL; currBlock = currBlock->prev, index++) {
        //Traverse entire alloc linked list to store pointers and size in array

        allocList[index].ptr = (void *) PADD(currBlock, sizeof(struct header));
        allocList[index].size = currBlock->size;

    }

    //Sort the struct into descending order
    _Quicksort(allocList, allocLength, sizeof(struct ptr_with_size), &Ptr_comparator);

    //Export the sorted data in the malloc'd lists
    for(index = 0; index < allocLength; index++) {
        sizeArray[index] = allocList[index].size;
        ptrArray[index] = allocList[index].ptr;
    }

    free(allocList);

    info->size_array = sizeArray;
    info->ptr_array = ptrArray;

    return 0;
}

