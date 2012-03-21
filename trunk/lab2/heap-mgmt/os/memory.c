//
//	memory.c
//
//	Routines for dealing with memory management.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "queue.h"

// JSM added freemap array
static uint32	freemap[MEM_FREEMAP_SIZE_IN_WORDS];
static int		physicalpages_max;
static int		physicalpages_free;

//----------------------------------------------------------------------
//
//	This silliness is required because the compiler believes that
//	it can invert a number by subtracting it from zero and subtracting
//	an additional 1.  This works unless you try to negate 0x80000000,
//	which causes an overflow when subtracted from 0.  Simply
//	trying to do an XOR with 0xffffffff results in the same code
//	being emitted.
//
//----------------------------------------------------------------------
static int negativeone = 0xFFFFFFFF;
static inline uint32 invert (uint32 n) {
  return (n ^ negativeone);
}

//----------------------------------------------------------------------
//
//	MemoryGetSize
//
//	Return the total size of memory in the simulator.  This is
//	available by reading a special location.
//
//----------------------------------------------------------------------
int MemoryGetSize() {
  return (*((int *)DLX_MEMSIZE_ADDRESS));
}


//----------------------------------------------------------------------
//
//	MemoryInitModule
//
//	Initialize the memory module of the operating system.
//      Basically just need to setup the freemap for pages, and mark
//      the ones in use by the operating system as "VALID", and mark
//      all the rest as not in use.
//
//----------------------------------------------------------------------
void MemoryModuleInit() {
	
	// JSM added code for memory module init
	
	int i;
	int os_pages_used;
	int pmem_size;

	pmem_size = MemoryGetSize();

	dbprintf('m', "PmemSize: %d\n", pmem_size);
	
	physicalpages_max = (pmem_size / MEM_PAGE_SIZE);

	dbprintf('m', "Number of physical pages in memory: %d\n", physicalpages_max);

	for(i=0; i<(physicalpages_max/32); i++)
	{
		freemap[i]=0xFFFFFFFF; // Set all pages as initially unused
	}

	dbprintf('m', "Last address of OS: %d\n", lastosaddress);
	
	os_pages_used = (lastosaddress / MEM_PAGE_SIZE)+1; // Calculate number of pages used by the OS
	
	dbprintf('m', "Number of pages used by OS: %d\n", os_pages_used);
	
	physicalpages_free = physicalpages_max - os_pages_used;

	dbprintf('m', "Number of free pages left in physical memory: %d\n", physicalpages_free);

	// Mark pages used by operating system as "VALID" (Set to '0')
	
	for(i=0; i < (os_pages_used/32); i++)
	{
		freemap[i] = 0;
	}
	
	freemap[i] = (0xFFFFFFFF) << (os_pages_used%32);

	
	for(i=0; i<MEM_FREEMAP_SIZE_IN_WORDS; i++)
	{
		dbprintf('m', "Freemap Int #%d value: 0x%.8X\n", i, freemap[i]);
	}

	
}


//--- YF Added code
//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr) {
//	TODO MemoryTranslate: add a heap space management
// need to add functions CheckHeapAddr -
	uint32 idx;
	uint32 offset;
	uint32 pte_idx;
	uint32 rtnAddress;

	dbprintf('m', "\nMemoryTranslateUserToSystem-Address to translate: 0x%.8X\n", addr);

	offset = addr & (MEM_PAGE_SIZE-1); // lower 12 bits

	//dbprintf('m', "MemoryTranslateUserToSystem-Offset: 0x%.8X\n", offset);

	idx = addr >> (MEM_L1FIELD_FIRST_BITNUM);// get rid of lower 12 bit offset

	dbprintf('m', "MemoryTranslateUserToSystem-Index: 0x%.8X\n", idx);

	pte_idx = pcb->pagetable[idx];

	dbprintf('m', "MemoryTranslateUserToSystem-PTE_IDX: 0x%.8X\n", pte_idx);

	if ((pte_idx & MEM_PTE_VALID) == 0) // Invalid page
	{
		printf("MemoryTranslateUserToSystem- INVALID PAGE TRANSLATION!!  SEG FAULT!!\n");
		pcb->currentSavedFrame[PROCESS_STACK_FAULT] = addr;
		MemoryPageFaultHandler(pcb);//TODO heap page fault handled differently.
	}

	pte_idx &= ~(MEM_PTE_READONLY | MEM_PTE_DIRTY | MEM_PTE_VALID);

	/* JSM -- not needed?
	if ((pte_idx & MEM_PTE_HEAP) != 0){//Heap check if in a valid location
		dbprintf('m', "MemoryTranslateUserToSystem-Heap adress found\n");
		if ( ((pcb->HeapFreeMap[offset/32]) & (0x1 << (addr%32))) == 0){ //Address is valid
			dbprintf('m', "MemoryTranslateUserToSystem-Heap address valid 0x%.8X\n", pte_idx);
			pte_idx &= ~MEM_PTE_HEAP;
			rtnAddress = pte_idx | offset;
		}
		else{//Invalid heap address: heap fault
			printf("MemoryTranslateUserToSystem-FATAL:Attempting to access unallocated area in the heap");
			printf("MemoryTranslateUserToSystem-KILLING PROCESS!!\n\n");
			ProcessKill();
			return(0);
		}
	}
	else{	rtnAddress = pte_idx | offset;}// not a heap address
	*/
	rtnAddress = pte_idx | offset;
	dbprintf('m', "MemoryTranslateUserToSystem-Translated Address: 0x%.8X\n", rtnAddress);
	return (rtnAddress);
}


//----------------------------------------------------------------------
//
//	MemoryMoveBetweenSpaces
//
//	Copy data between user and system spaces.  This is done page by
//	page by:
//	* Translating the user address into system space.
//	* Copying all of the data in that page
//	* Repeating until all of the data is copied.
//	A positive direction means the copy goes from system to user
//	space; negative direction means the copy goes from user to system
//	space.
//
//	This routine returns the number of bytes copied.  Note that this
//	may be less than the number requested if there were unmapped pages
//	in the user range.  If this happens, the copy stops at the
//	first unmapped address.
//
//----------------------------------------------------------------------
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir) {
  unsigned char *curUser;         // Holds current physical address representing user-space virtual address
  int		bytesCopied = 0;  // Running counter
  int		bytesToCopy;      // Used to compute number of bytes left in page to be copied

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.
    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) break;

    // Calculate the number of bytes to copy this time.  If we have more bytes
    // to copy than there are left in the current page, we'll have to just copy to the
    // end of the page and then go through the loop again with the next page.
    // In other words, "bytesToCopy" is the minimum of the bytes left on this page 
    // and the total number of bytes left to copy ("n").

    // First, compute number of bytes left in this page.  This is just
    // the total size of a page minus the current offset part of the physical
    // address.  MEM_PAGE_SIZE should be the size (in bytes) of 1 page of memory.
    // MEM_ADDRESS_OFFSET_MASK should be the bit mask required to get just the
    // "offset" portion of an address.
    bytesToCopy = MEM_PAGE_SIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);
    
    // Now find minimum of bytes in this page vs. total bytes left to copy
    if (bytesToCopy > n) {
      bytesToCopy = n;
    }

    // Perform the copy.
    if (dir >= 0) {
      bcopy (system, curUser, bytesToCopy);
    } else {
      bcopy (curUser, system, bytesToCopy);
    }

    // Keep track of bytes copied and adjust addresses appropriately.
    n -= bytesToCopy;           // Total number of bytes left to copy
    bytesCopied += bytesToCopy; // Total number of bytes copied thus far
    system += bytesToCopy;      // Current address in system space to copy next bytes from/into
    user += bytesToCopy;        // Current virtual address in user space to copy next bytes from/into
  }
  return (bytesCopied);
}

//----------------------------------------------------------------------
//
//	These two routines copy data between user and system spaces.
//	They call a common routine to do the copying; the only difference
//	between the calls is the actual call to do the copying.  Everything
//	else is identical.
//
//----------------------------------------------------------------------
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, from, to, n, 1));
}

int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, to, from, n, -1));
}


//----------------------------------------------------------------------
//
//	MemoryAllocatePage
//
//	Allocates a physical page in memory, returns 0 if unsuccessful, or
//		the allocated page number if successful.
//
//----------------------------------------------------------------------
int MemoryAllocatePage() {

	// JSM added code for physical memory allocation
	int i,j;

	if (physicalpages_free == 0)
	{
		dbprintf('m', "Unable to allocate physical page!\n");
		return(0);
	}
	else
	{
		for(i = 0; i < (physicalpages_max / 32); i++)
		{
			if(freemap[i] != 0)
			{
				dbprintf('m', "Freemap Index #%d has free pages, contents: 0x%.8X\n", i, freemap[i]);
				for(j = 0; j < 32; j++)
				{
					if((freemap[i] & (0x1 << j)) != 0)
					{
						freemap[i] &= invert(0x1 << j);

						dbprintf('m', "Physical Page #%d allocated!\n", (i*32+j));
						physicalpages_free--;
						dbprintf('m', "Freemap Index #%d after page allocated: 0x%.8X\n", i, freemap[i]);

						return(i*32+j);

					}
				}
			}
		}
		// SHOULD NEVER GET HERE!
		dbprintf('m', "Unable to allocate physical page AND error in allocation logic!!\n");
		return(0);
	}
}


//----------------------------------------------------------------------
//
//	MemoryFreePage
//
//	Frees a physical page in memory
//
//----------------------------------------------------------------------
void MemoryFreePage(uint32 page) {

	// JSM added code for freeing physical page

	freemap[(page/32)] |= 0x1 << (page%32);
	dbprintf('m', "Physical Page #%d freed!\n", page);
	dbprintf('m', "Freemap Index #%d after page freed: 0x%.8X\n", (page/32), freemap[(page/32)]);
	physicalpages_free++;


	return;
}


//---------------------------------------------------------------------
// MemoryPageFaultHandler is called in traps.c whenever a page fault 
// (better known as a "seg fault" occurs.  If the address that was
// being accessed is on the stack, we need to allocate a new page 
// for the stack.  If it is not on the stack, then this is a legitimate
// seg fault and we should kill the process.  Returns MEM_SUCCESS
// on success, and kills the current process on failure.  Note that
// fault_address is the beginning of the page of the virtual address that 
// caused the page fault, i.e. it is the vaddr with the offset zero-ed
// out.
//---------------------------------------------------------------------
int MemoryPageFaultHandler(PCB *pcb)
{
  uint32 newpage;
  uint32 idx;

  dbprintf('M',"PAGE FAULT!\n");
  dbprintf('M',"Page Fault Address: 0x%.8X, Stack Pointer: 0x%.8X\n", pcb->currentSavedFrame[PROCESS_STACK_FAULT], pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER]);


	if(pcb->currentSavedFrame[PROCESS_STACK_FAULT] >= ((pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER] - 8) & ~(MEM_PAGE_SIZE-1)))
	{
		// Allocate page for user stack

		if(pcb->num_p_pages >= 32)
		{
			// KILL
			printf("Process is requesting more than 32 total pages!!\n");
			printf("KILLING PROCESS!!\n");
			ProcessKill();
		}

		newpage = MemoryAllocatePage();
		if (newpage == 0)
		{
			printf ("MemoryPageFaultHandler-FATAL: couldn't allocate memory - no free pages!\n");
			exitsim ();	// NEVER RETURNS!
			return(MEM_FAIL);
		}
		idx = pcb->currentSavedFrame[PROCESS_STACK_FAULT] >> (MEM_L1FIELD_FIRST_BITNUM);
		dbprintf('M', "MemoryPageFaultHandler-Allocating physical page #%d (Address 0x%.8X) for process virtual page #%d (user stack)\n", newpage, (newpage*MEM_PAGE_SIZE),idx);
		pcb->pagetable[idx] = ((newpage*MEM_PAGE_SIZE) | MEM_PTE_VALID);
		(pcb->num_p_pages)++;
		printf("Allocating additional page for process %d, ", GetCurrentPid());
		printf("Total physical pages now owned by process: %d\n", pcb->num_p_pages);

		return(MEM_SUCCESS);
	}
	else
	{
		printf("PAGE FAULT ERROR!  Within virtual address space, but referencing non-allocated page!!\n");
		printf("Page Fault Address: 0x%.8X, ", pcb->currentSavedFrame[PROCESS_STACK_FAULT]);
		printf("Stack Pointer: 0x%.8X\n", pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER]);
		printf("KILLING PROCESS!!\n");
		ProcessKill();
		return(MEM_FAIL);
	}

}

//---------------------------------------------------------------------
//
//	malloc(PCB *pcb,int memsize)
//
//		allocate a memory block of size memsize bytes in the heap space and
//			return a pointer corresponding to the starting virtual address
//			of the allocated block.
//---------------------------------------------------------------------
void *malloc(PCB *pcb, int memsize){
	uint32 wordBlocks;
	//uint32 new_Virtual_Address;
	int idx;
	void *rtnAddr;

	/*
	if ((memsize % 4) == 0)	{
		wordBlocks = (uint32) memsize;
	}
	else{
		wordBlocks = (uint32) memsize/8 +1;
	}*/

	if (memsize <= 0)
	{
		// RETURN VOID, INVALID REQUEST
		printf("malloc-INVALID MALLOC REQUEST!\n");
		return((void *)NULL);
	}
	else
	{
		wordBlocks = (uint32) ((memsize+3)/4);
	}


	idx = MemoryHeapAllocateChunk(pcb, wordBlocks+1); // Allocate enough words for initial request plus one to hold size or "chunk"
																										// Return INDEX corresponding to starting word of the chunk to return to user

	//new_Virtual_Address = ((uint32) (pcb->userHeapArea)) >> (MEM_L1FIELD_FIRST_BITNUM);// get rid of lower 12 bit offset
	//new_Virtual_Address= (new_Virtual_Address*MEM_PAGE_SIZE +	(MemoryHeapAllocatePage(pcb,wordBlocks))*4) | MEM_PTE_VALID;

	if (idx == -1)
	{
		// RETURN VOID, UNABLE TO MALLOC
		printf("malloc-UNABLE TO MALLOC DESIRED CHUNK SIZE!\n");
		return((void *)NULL);
	}
	else
	{
		rtnAddr = (void *) (idx*4)+(MEM_PAGE_SIZE*4); // Virtual Address of pointer to newly allocated "chunk"
	}

	printf("Created a heap block of size %d bytes: ", (wordBlocks+1)*4);
	printf("virtual address 0x%.8X, ", (int)rtnAddr);
	printf("physical address 0x%.8X\n", MemoryTranslateUserToSystem(pcb, (int)rtnAddr));
	return (rtnAddr);
}

//---------------------------------------------------------------------
//
//	mfree(PCB *pcb,void *ptr)
//
//		free the heap block identified by ptr. return -1 if none to free
//---------------------------------------------------------------------
int mfree(PCB *pcb, void *ptr){
	int bytesFreed;
	int idx;
	int size;
	int i;
	uint32 *size_addr;

	if(ptr == NULL)
	{
		// RETURN -1, NULL POINTER
		printf("mfree-UNABLE TO FREE CHUNK... NULL POINTER!\n");
		return(-1);
	}


	idx = ((int)(ptr) - (MEM_PAGE_SIZE*4))/4;
	dbprintf('M', "mfree- Index into heap: %d\n", idx);

	if(idx < 0 || idx >= (MEM_MAX_HEAP_FREEMAP*32))
	{
		// RETURN -1, INVALID POINTER
		printf("mfree-Invalid Pointer!, Index: %d\n", idx);
		return(-1);
	}

	if((pcb->HeapValidPtrMap[(idx)/32] & (0x1 << ((idx)%32))) != 0)
	{
		// VALID POINTER TO FREE
		size_addr = (uint32 *)MemoryTranslateUserToSystem(pcb, (uint32)ptr - 4);
		size = *size_addr; // Store size at physical address corresponding to start of chunk
		dbprintf('M', "mfree- Size in words of chunk to free: %d, address: 0x%.8X\n", size, (int)size_addr);

		// THIS WORKS!!!!!!!!!!!!!

		// Clear valid pointer bit in HeapValidPtrMap (set to 0)
		pcb->HeapValidPtrMap[(idx)/32] &= invert( (0x1 << ((idx)%32)) );

		// Mark corresponding bits in HeapFreeMap as "free" (set to 1)
		pcb->HeapFreeMap[(idx-1)/32] |= (0x1 << ((idx-1)%32)); // "size word"

		for(i=0; i<size; i++)
		{
			pcb->HeapFreeMap[(idx+i)/32] |= (0x1 << ((idx+i)%32)); // all data words
		}


		dbprintf('M', "\nmfree-Heap pointer map after allocation:\n\n");
		for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
		{
			dbprintf('M', "0x%.8X\n", pcb->HeapValidPtrMap[i]);
		}


		dbprintf('M', "\nmfree-Heap freemap after free:\n\n");
		for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
		{
			dbprintf('M', "0x%.8X\n", pcb->HeapFreeMap[i]);
		}

	}
	else
	{
		// RETURN -1, INVALID POINTER
		printf("mfree-Invalid Pointer!\n");
		return(-1);
	}


	printf("Freeing heap block of size %d bytes: ", (size+1)*4);
	printf("virtual address 0x%.8X, ", (int)ptr);
	printf("physical address 0x%.8X\n", MemoryTranslateUserToSystem(pcb, (uint32)ptr));

	return(size);
}


int MemoryHeapAllocateChunk(PCB *pcb, uint32 wordBlocks) {
	uint32 i, j, k;
	uint32 count = 0;
	int idx;
	uint32 *addr;

	dbprintf('M', "MemoryHeapAllocatePage-Heap wordBlocks #%d\n" ,wordBlocks);

	dbprintf('M', "MemoryHeapAllocatePage-Heap freemap before allocation:\n");
	for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
	{
		dbprintf('M', "0x%.8X\n", pcb->HeapFreeMap[i]);
	}


	for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
	{
		for(j = 0; j < 32; j++)
		{
			if((pcb->HeapFreeMap[i] & (0x1 << j)) != 0)
			{
				//freemap[i] &= invert(0x1 << j);
				count++;

				if (count == wordBlocks)
				{
					// FOUND SPACE IN HEAP FOR REQUESTED WORDS!!

					idx = i*32+j; // Index of LAST word
					idx -= (wordBlocks-1); // Index of FIRST word (to hold size data)


					// uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr)
					addr = (uint32 *)MemoryTranslateUserToSystem(pcb, (uint32)((idx*4)+(MEM_PAGE_SIZE*4)));
					*addr = (wordBlocks-1); // Store size at physical address corresponding to start of chunk

					// Mark bits in HeapFreeMap as used
					for(k = idx; k < (idx + wordBlocks); k++)
					{
						pcb->HeapFreeMap[(k/32)] &= invert(0x1 << (k%32));
					}

					// Mark bit in HeapValidPtrMap corresponding to pointer of start of chunk (returned to user)
					pcb->HeapValidPtrMap[(idx+1)/32] |= (0x1 << ((idx+1)%32));

					dbprintf('M', "\nMemoryHeapAllocatePage-Heap freemap after allocation:\n\n");
					for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
					{
						dbprintf('M', "0x%.8X\n", pcb->HeapFreeMap[i]);
					}

					dbprintf('M', "\nMemoryHeapAllocatePage-Heap pointer map after allocation:\n\n");
					for(i = 0; i < MEM_MAX_HEAP_FREEMAP; i++)
					{
						dbprintf('M', "0x%.8X\n", pcb->HeapValidPtrMap[i]);
					}

					dbprintf('M', "\nMemoryHeapAllocatePage- Index of user first byte: %d\n\n", idx+1);
					return(idx+1); // Return index of first DATA word for newly allocated chunk

				}
			}
			else
			{
				count = 0; // reset count
			}
		}
	}

	dbprintf('M', "Unable to allocate requested heap memory!!\n");
	//printf("KILLING PROCESS!!\n"); // DONT KILL PROCESS, JUST RETURN FAIL
	//ProcessKill();
	return(MEM_FAIL);
}


/*
uint32 MemoryHeapAllocateChunk(PCB *pcb,uint32 wordBlocks) {
	uint32 i, j, k;
	uint32 failed;
	uint32 end_address;
	dbprintf('m', "MemoryHeapAllocatePage-Heap wordBlocks #%d\n" ,wordBlocks);
	for (i = 0; i < MEM_MAX_HEAP_FREEMAP; i++) {
		if (pcb->HeapFreeMap[i] != 0) {
			dbprintf('m', "MemoryHeapAllocatePage-Heap Index #%d has free table, contents: 0x%.8X\n", i, pcb->HeapFreeMap[i]);
			for (j = 0; j < 32; j++) {
				if ((pcb->HeapFreeMap[i] & (0x1 << j)) != 0) {
					//Find a contiguous block of wordBlocks
					end_address = j + wordBlocks -1;//j is also valid so 1 less
					failed= 0;
					for (k = j; k <= end_address; k++){
						if ((pcb->HeapFreeMap[i] & (0x1 << k)) == 0){
							j = k;
							failed = 1;
						}
					}
					if (failed == 0){ //
							for(k=j;k<=end_address; k++){pcb->HeapFreeMap[i] &= invert(0x1 << k);}
							dbprintf('m', "MemoryHeapAllocatePage-Freemap_PT return #%d after L2 Page Table: 0x%.8X\n", (i * 32 + j), pcb->HeapFreeMap[i]);
							return (i * 32 + j);
							//TODO add jost
					}
				}
			}
		}
	}
	dbprintf('m', "Unable to allocate Any more heap memory!!\n"); //TODO CALL heap grow here: NOW Just exit
	printf("KILLING PROCESS!!\n");
	ProcessKill();
	return(MEM_FAIL);
}
*/