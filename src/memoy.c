#include <stdlib.h>


//dev defines
#define UNIT_TEST_MEMORY_H 1

#if UNIT_TEST_MEMORY_H != 0
#include <stdio.h>
#endif//UNIT_TEST_MEMORY_H


//constants
#define RESERVE_BLOCK_COUNT_HINT_DEFAULT 10
#define FIXED_SIZE_MEMORY_BLOCK_ALLOC 10

//#define RELEASED_PTR (void*)UINT_MAX
#define RELEASED_PTR NULL


enum ResizeStrategy
{
    DOUBLING,       //Double its current capacity when resizing
    FIXED_BLOCKS,   //Increase by a fixed size when resizing
    TIERED_DOUBLING //Increase by a fixed size first and subsequent resizes doubles capacity
};


struct MemoryBlock
{
    void* handle;
    size_t size;
};


struct MemoryBlockManager
{
    enum ResizeStrategy resizeStrategy;     //Strategy for resizing the number of memory blocks
    size_t reserveHint;                     //How much additional memory to reserve when this manager runs out of memory blocks

    size_t capacity;                        //How many memory blocks are allocated
    size_t size;                            //How many memory blocks are in use

    struct MemoryBlock* memoryBlocks;       //Array of memory blocks allocated
};

//resize the capacity of the memory manager
//returns 0 if the resize was successful, otherwise 1
int resizeMemoryManager(struct MemoryBlockManager* manager)
{
    size_t newSize = 0;

    switch(manager->resizeStrategy)
    {
        case FIXED_BLOCKS:
        {
            newSize = manager->capacity + manager->reserveHint;
        }
        break;

        case TIERED_DOUBLING:
        {
            newSize = manager->capacity <= manager->reserveHint ? 
                manager->capacity + manager->reserveHint 
                : manager->capacity * 2;
        }
        break;

        case DOUBLING:
        default:
        {
            newSize = manager->capacity * 2;
        }
        break;
    }

    realloc(manager->memoryBlocks, newSize);

    if(manager->memoryBlocks == NULL)
    {
        manager->capacity = 0; 
        //no more memory
        return 1;
    }

    manager->capacity = newSize;

    return 0;
}


//mallocs a memory block for use
//returns the pointer to the memory block, otherwise NULL
void* MallocMemoryBlock(struct MemoryBlockManager* manager, size_t size)
{
    if(manager->capacity <= manager->size)
    {
        int outOfMemory = resizeMemoryManager(manager);
        if(outOfMemory)
        {
            printf("out of memory!\n");
            return NULL;
        }
    }

    void* newBlock = malloc(size);
    if(newBlock == NULL)
    {
        printf("Malloc failed!\n");
        return NULL;
    }

    manager->memoryBlocks[manager->size].handle = newBlock;
    manager->memoryBlocks[manager->size].size = size;
    ++manager->size;

    return newBlock;
}


//releases a memory block
//returns 0 if the block was freed, otherwise 1
int FreeMemoryBlock(struct MemoryBlockManager* manager, void* block)
{
    //find the memory block
    size_t i = 0;
    for(i = 0; i < manager->size; ++i)
    {
        if(manager->memoryBlocks[i].handle == block)
        {
            free(&manager->memoryBlocks[i]);
            block = RELEASED_PTR;
            break;
        }
    }

    if(block != RELEASED_PTR)
    {
        printf("%p is not managed by memory manager %p", block, (void*)manager);
        return 1; //block was not managed by the memory manager!
    }

    //decrease ref count
    --manager->size;
    return 0;
}


//initializes a memory block manager
void InitMemoryBlockManager(struct MemoryBlockManager* manager, size_t reserveHint, enum ResizeStrategy strategy)
{
    manager->resizeStrategy = strategy;
    manager->reserveHint = reserveHint;

    manager->memoryBlocks = (struct MemoryBlock*)malloc(sizeof(struct MemoryBlock) * reserveHint);
    manager->capacity = reserveHint;
    manager->size = 0;
}


//destroys a memory block manager
void DestroyManagerBlockManager(struct MemoryBlockManager* manager)
{
    size_t i = 0, size = manager->size;
    for(; i < size; ++i)
    {
        FreeMemoryBlock(manager, manager->memoryBlocks + i);
    }
    free(manager->memoryBlocks);
}


size_t GetMemoryLeakSize(struct MemoryBlockManager* manager)
{
    size_t i = 0, leakedSize = 0;
    for(i = 0; i < manager->size; ++i)
    {
        leakedSize += manager->memoryBlocks[i].size;
    }
    return leakedSize;
}


#if UNIT_TEST_MEMORY_H != 0
int programEntryPoint(struct MemoryBlockManager * manager)
{
    int arraySize = 5;
    int* intArray = (int*)MallocMemoryBlock(manager, sizeof(int) * arraySize);

    size_t i = 0;
    for(i = 0; i < arraySize; ++i)
    {
        intArray[i] = i * i;
    }

    for(i = 0; i < arraySize; ++i)
    {
        printf("%i : %i\n", i, intArray[i]);
    }

    int rv = FreeMemoryBlock(manager, intArray);
    if(rv == 0)
    {
        printf("successful free\n");
    }
    else
    {
        printf("failed to free!\n");
    }
}


int main()
{
    printf("Starting unit test...\n");
    struct MemoryBlockManager memoryManager;

    int returnCode = 0;
    InitMemoryBlockManager(&memoryManager, RESERVE_BLOCK_COUNT_HINT_DEFAULT, TIERED_DOUBLING);
    {
        returnCode = programEntryPoint(&memoryManager);    
        if(memoryManager.size != 0)
        {
            printf("%i memory blocks leaked!\n", memoryManager.size);
            printf("%i bytes of memory leaked!\n", GetMemoryLeakSize(&memoryManager));
        }
    }
    DestroyManagerBlockManager(&memoryManager);
    return returnCode;
}
#endif//UNIT_TEST_MEMORY_H