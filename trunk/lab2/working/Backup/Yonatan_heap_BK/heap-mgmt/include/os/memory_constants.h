#ifndef	_memory_constants_h_
#define	_memory_constants_h_

//------------------------------------------------
// #define's that you are given:
//------------------------------------------------

// We can read this address in I/O space to figure out how much memory
// is available on the system.
#define	DLX_MEMSIZE_ADDRESS	0xffff0000

// Return values for success and failure of functions
#define MEM_SUCCESS 1
#define MEM_FAIL -1

//--------------------------------------------------------
// Put your constant definitions related to memory here.
// Be sure to prepend any constant names with "MEM_" so 
// that the grader knows they are defined in this file.
//--------------------------------------------------------


// JSM -- Added memory constants
#define MEM_L1FIELD_FIRST_BITNUM 12
#define MEM_L2FIELD_FIRST_BITNUM 12
#define MEM_MAX_VIRTUAL_ADDRESS 0xFFFFF // 1 MB Virtual address space = 10^20 - 1
#define MEM_MAX_PHYSICAL_ADDRESS 0x1FFFFF // 2 MB Physical address space = 10^21 - 1
#define MEM_PTE_READONLY 0x4
#define MEM_PTE_DIRTY 0x2
#define MEM_PTE_VALID 0x1

#define MEM_PAGE_SIZE (0x1 << MEM_L2FIELD_FIRST_BITNUM)
#define MEM_ADDRESS_OFFSET_MASK (MEM_PAGE_SIZE - 1)
#define MEM_L1_PAGE_TABLE_SIZE ((MEM_MAX_VIRTUAL_ADDRESS+1) >> MEM_L1FIELD_FIRST_BITNUM)
#define	MEM_L2_PAGE_TABLE_SIZE (0x1 << (MEM_L1FIELD_FIRST_BITNUM - MEM_L2FIELD_FIRST_BITNUM))

#define MEM_NUM_OF_PHYSICAL_PAGES ((MEM_MAX_PHYSICAL_ADDRESS+1) >> MEM_L2FIELD_FIRST_BITNUM)
#define MEM_FREEMAP_SIZE_IN_WORDS (MEM_NUM_OF_PHYSICAL_PAGES/32)

//YF L2 work
#define MEM_L1_ADDRESS_MASK 0xF << (MEM_L1FIELD_FIRST_BITNUM-1)// 000F0000

//YF 16*Heap work
#define MEM_MAX_HEAP_FREEMAP (MEM_PAGE_SIZE)/4/32 //=4 four integer index for heap free map
#define MEM_MAX_HEAP_POINTER_ARRAY (MEM_PAGE_SIZE-1)/32 //=128 ONLY word addressed
#define MEM_PTE_HEAP 0x8 //To distinguish weather this is a correct location in heap or not
#define MEM_WORD_ADRESS_MASK 0xFE0// MASK OF LOWER TWO BITS

#endif	// _memory_constants_h_
