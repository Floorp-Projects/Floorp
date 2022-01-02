// Copied from upstream at revision 195c13743fe0ebc658714e2a9567d86529f20443.
// mach_override.c semver:1.2.0
//   Copyright (c) 2003-2012 Jonathan 'Wolf' Rentzsch: http://rentzsch.com
//   Some rights reserved: http://opensource.org/licenses/mit
//   https://github.com/rentzsch/mach_override

#include "mach_override.h"

#include <mach-o/dyld.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <sys/mman.h>

#include <CoreServices/CoreServices.h>

/**************************
*
*	Constants
*
**************************/
#pragma mark	-
#pragma mark	(Constants)

#define kPageSize 4096
#if defined(__ppc__) || defined(__POWERPC__)

long kIslandTemplate[] = {
	0x9001FFFC,	//	stw		r0,-4(SP)
	0x3C00DEAD,	//	lis		r0,0xDEAD
	0x6000BEEF,	//	ori		r0,r0,0xBEEF
	0x7C0903A6,	//	mtctr	r0
	0x8001FFFC,	//	lwz		r0,-4(SP)
	0x60000000,	//	nop		; optionally replaced
	0x4E800420 	//	bctr
};

#define kAddressHi			3
#define kAddressLo			5
#define kInstructionHi		10
#define kInstructionLo		11

#elif defined(__i386__)

#define kOriginalInstructionsSize 16
// On X86 we migh need to instert an add with a 32 bit immediate after the
// original instructions.
#define kMaxFixupSizeIncrease 5

unsigned char kIslandTemplate[] = {
	// kOriginalInstructionsSize nop instructions so that we
	// should have enough space to host original instructions
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	// Now the real jump instruction
	0xE9, 0xEF, 0xBE, 0xAD, 0xDE
};

#define kInstructions	0
#define kJumpAddress    kInstructions + kOriginalInstructionsSize + 1
#elif defined(__x86_64__)

#define kOriginalInstructionsSize 32
// On X86-64 we never need to instert a new instruction.
#define kMaxFixupSizeIncrease 0

#define kJumpAddress    kOriginalInstructionsSize + 6

unsigned char kIslandTemplate[] = {
	// kOriginalInstructionsSize nop instructions so that we
	// should have enough space to host original instructions
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	// Now the real jump instruction
	0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

#endif

/**************************
*
*	Data Types
*
**************************/
#pragma mark	-
#pragma mark	(Data Types)

typedef	struct	{
	char	instructions[sizeof(kIslandTemplate)];
}	BranchIsland;

/**************************
*
*	Funky Protos
*
**************************/
#pragma mark	-
#pragma mark	(Funky Protos)

static mach_error_t
allocateBranchIsland(
		BranchIsland	**island,
		void *originalFunctionAddress);

	mach_error_t
freeBranchIsland(
		BranchIsland	*island );

#if defined(__ppc__) || defined(__POWERPC__)
	mach_error_t
setBranchIslandTarget(
		BranchIsland	*island,
		const void		*branchTo,
		long			instruction );
#endif

#if defined(__i386__) || defined(__x86_64__)
mach_error_t
setBranchIslandTarget_i386(
						   BranchIsland	*island,
						   const void		*branchTo,
						   char*			instructions );
void
atomic_mov64(
		uint64_t *targetAddress,
		uint64_t value );

	static Boolean
eatKnownInstructions(
	unsigned char	*code,
	uint64_t		*newInstruction,
	int				*howManyEaten,
	char			*originalInstructions,
	int				*originalInstructionCount,
	uint8_t			*originalInstructionSizes );

	static void
fixupInstructions(
    uint32_t		offset,
    void		*instructionsToFix,
	int			instructionCount,
	uint8_t		*instructionSizes );
#endif

/*******************************************************************************
*
*	Interface
*
*******************************************************************************/
#pragma mark	-
#pragma mark	(Interface)

#if defined(__i386__) || defined(__x86_64__)
mach_error_t makeIslandExecutable(void *address) {
	mach_error_t err = err_none;
    uintptr_t page = (uintptr_t)address & ~(uintptr_t)(kPageSize-1);
    int e = err_none;
    e |= mprotect((void *)page, kPageSize, PROT_EXEC | PROT_READ | PROT_WRITE);
    e |= msync((void *)page, kPageSize, MS_INVALIDATE );
    if (e) {
        err = err_cannot_override;
    }
    return err;
}
#endif

    mach_error_t
mach_override_ptr(
	void *originalFunctionAddress,
    const void *overrideFunctionAddress,
    void **originalFunctionReentryIsland )
{
	assert( originalFunctionAddress );
	assert( overrideFunctionAddress );

	// this addresses overriding such functions as AudioOutputUnitStart()
	// test with modified DefaultOutputUnit project
#if defined(__x86_64__)
    for(;;){
        if(*(uint16_t*)originalFunctionAddress==0x25FF)    // jmp qword near [rip+0x????????]
            originalFunctionAddress=*(void**)((char*)originalFunctionAddress+6+*(int32_t *)((uint16_t*)originalFunctionAddress+1));
        else break;
    }
#elif defined(__i386__)
    for(;;){
        if(*(uint16_t*)originalFunctionAddress==0x25FF)    // jmp *0x????????
            originalFunctionAddress=**(void***)((uint16_t*)originalFunctionAddress+1);
        else break;
    }
#endif

	long	*originalFunctionPtr = (long*) originalFunctionAddress;
	mach_error_t	err = err_none;

#if defined(__ppc__) || defined(__POWERPC__)
	//	Ensure first instruction isn't 'mfctr'.
	#define	kMFCTRMask			0xfc1fffff
	#define	kMFCTRInstruction	0x7c0903a6

	long	originalInstruction = *originalFunctionPtr;
	if( !err && ((originalInstruction & kMFCTRMask) == kMFCTRInstruction) )
		err = err_cannot_override;
#elif defined(__i386__) || defined(__x86_64__)
	int eatenCount = 0;
	int originalInstructionCount = 0;
	char originalInstructions[kOriginalInstructionsSize];
	uint8_t originalInstructionSizes[kOriginalInstructionsSize];
	uint64_t jumpRelativeInstruction = 0; // JMP

	Boolean overridePossible = eatKnownInstructions ((unsigned char *)originalFunctionPtr,
										&jumpRelativeInstruction, &eatenCount,
										originalInstructions, &originalInstructionCount,
										originalInstructionSizes );
	if (eatenCount + kMaxFixupSizeIncrease > kOriginalInstructionsSize) {
		//printf ("Too many instructions eaten\n");
		overridePossible = false;
	}
	if (!overridePossible) err = err_cannot_override;
	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);
#endif

	//	Make the original function implementation writable.
	if( !err ) {
		err = vm_protect( mach_task_self(),
				(vm_address_t) originalFunctionPtr, 8, false,
				(VM_PROT_ALL | VM_PROT_COPY) );
		if( err )
			err = vm_protect( mach_task_self(),
					(vm_address_t) originalFunctionPtr, 8, false,
					(VM_PROT_DEFAULT | VM_PROT_COPY) );
	}
	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);

	//	Allocate and target the escape island to the overriding function.
	BranchIsland	*escapeIsland = NULL;
	if( !err ) {
		err = allocateBranchIsland( &escapeIsland, originalFunctionAddress );
		if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);
	}

#if defined(__ppc__) || defined(__POWERPC__)
	if( !err )
		err = setBranchIslandTarget( escapeIsland, overrideFunctionAddress, 0 );

	//	Build the branch absolute instruction to the escape island.
	long	branchAbsoluteInstruction = 0; // Set to 0 just to silence warning.
	if( !err ) {
		long escapeIslandAddress = ((long) escapeIsland) & 0x3FFFFFF;
		branchAbsoluteInstruction = 0x48000002 | escapeIslandAddress;
	}
#elif defined(__i386__) || defined(__x86_64__)
        if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);

	if( !err )
		err = setBranchIslandTarget_i386( escapeIsland, overrideFunctionAddress, 0 );

	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);
	// Build the jump relative instruction to the escape island
#endif


#if defined(__i386__) || defined(__x86_64__)
	if (!err) {
		uint32_t addressOffset = ((char*)escapeIsland - (char*)originalFunctionPtr - 5);
		addressOffset = OSSwapInt32(addressOffset);

		jumpRelativeInstruction |= 0xE900000000000000LL;
		jumpRelativeInstruction |= ((uint64_t)addressOffset & 0xffffffff) << 24;
		jumpRelativeInstruction = OSSwapInt64(jumpRelativeInstruction);
	}
#endif

	//	Optionally allocate & return the reentry island. This may contain relocated
	//  jmp instructions and so has all the same addressing reachability requirements
	//  the escape island has to the original function, except the escape island is
	//  technically our original function.
	BranchIsland	*reentryIsland = NULL;
	if( !err && originalFunctionReentryIsland ) {
		err = allocateBranchIsland( &reentryIsland, escapeIsland);
		if( !err )
			*originalFunctionReentryIsland = reentryIsland;
	}

#if defined(__ppc__) || defined(__POWERPC__)
	//	Atomically:
	//	o If the reentry island was allocated:
	//		o Insert the original instruction into the reentry island.
	//		o Target the reentry island at the 2nd instruction of the
	//		  original function.
	//	o Replace the original instruction with the branch absolute.
	if( !err ) {
		int escapeIslandEngaged = false;
		do {
			if( reentryIsland )
				err = setBranchIslandTarget( reentryIsland,
						(void*) (originalFunctionPtr+1), originalInstruction );
			if( !err ) {
				escapeIslandEngaged = CompareAndSwap( originalInstruction,
										branchAbsoluteInstruction,
										(UInt32*)originalFunctionPtr );
				if( !escapeIslandEngaged ) {
					//	Someone replaced the instruction out from under us,
					//	re-read the instruction, make sure it's still not
					//	'mfctr' and try again.
					originalInstruction = *originalFunctionPtr;
					if( (originalInstruction & kMFCTRMask) == kMFCTRInstruction)
						err = err_cannot_override;
				}
			}
		} while( !err && !escapeIslandEngaged );
	}
#elif defined(__i386__) || defined(__x86_64__)
	// Atomically:
	//	o If the reentry island was allocated:
	//		o Insert the original instructions into the reentry island.
	//		o Target the reentry island at the first non-replaced
	//        instruction of the original function.
	//	o Replace the original first instructions with the jump relative.
	//
	// Note that on i386, we do not support someone else changing the code under our feet
	if ( !err ) {
		uint32_t offset = (uintptr_t)originalFunctionPtr - (uintptr_t)reentryIsland;
		fixupInstructions(offset, originalInstructions,
					originalInstructionCount, originalInstructionSizes );

		if( reentryIsland )
			err = setBranchIslandTarget_i386( reentryIsland,
										 (void*) ((char *)originalFunctionPtr+eatenCount), originalInstructions );
		// try making islands executable before planting the jmp
#if defined(__x86_64__) || defined(__i386__)
        if( !err )
            err = makeIslandExecutable(escapeIsland);
        if( !err && reentryIsland )
            err = makeIslandExecutable(reentryIsland);
#endif
		if ( !err )
			atomic_mov64((uint64_t *)originalFunctionPtr, jumpRelativeInstruction);
	}
#endif

	//	Clean up on error.
	if( err ) {
		if( reentryIsland )
			freeBranchIsland( reentryIsland );
		if( escapeIsland )
			freeBranchIsland( escapeIsland );
	}

	return err;
}

/*******************************************************************************
*
*	Implementation
*
*******************************************************************************/
#pragma mark	-
#pragma mark	(Implementation)

static bool jump_in_range(intptr_t from, intptr_t to) {
  intptr_t field_value = to - from - 5;
  int32_t field_value_32 = field_value;
  return field_value == field_value_32;
}

/*******************************************************************************
	Implementation: Allocates memory for a branch island.

	@param	island			<-	The allocated island.
	@result					<-	mach_error_t

	***************************************************************************/

static mach_error_t
allocateBranchIslandAux(
		BranchIsland	**island,
		void *originalFunctionAddress,
		bool forward)
{
	assert( island );
	assert( sizeof( BranchIsland ) <= kPageSize );

	vm_map_t task_self = mach_task_self();
	vm_address_t original_address = (vm_address_t) originalFunctionAddress;
	vm_address_t address = original_address;

	for (;;) {
		vm_size_t vmsize = 0;
		memory_object_name_t object = 0;
		kern_return_t kr = 0;
		vm_region_flavor_t flavor = VM_REGION_BASIC_INFO;
		// Find the region the address is in.
#if __WORDSIZE == 32
		vm_region_basic_info_data_t info;
		mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT;
		kr = vm_region(task_self, &address, &vmsize, flavor,
			       (vm_region_info_t)&info, &info_count, &object);
#else
		vm_region_basic_info_data_64_t info;
		mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
		kr = vm_region_64(task_self, &address, &vmsize, flavor,
				  (vm_region_info_t)&info, &info_count, &object);
#endif
		if (kr != KERN_SUCCESS)
			return kr;
		assert((address & (kPageSize - 1)) == 0);

		// Go to the first page before or after this region
		vm_address_t new_address = forward ? address + vmsize : address - kPageSize;
#if __WORDSIZE == 64
		if(!jump_in_range(original_address, new_address))
			break;
#endif
		address = new_address;

		// Try to allocate this page.
		kr = vm_allocate(task_self, &address, kPageSize, 0);
		if (kr == KERN_SUCCESS) {
			*island = (BranchIsland*) address;
			return err_none;
		}
		if (kr != KERN_NO_SPACE)
			return kr;
	}

	return KERN_NO_SPACE;
}

static mach_error_t
allocateBranchIsland(
		BranchIsland	**island,
		void *originalFunctionAddress)
{
  mach_error_t err =
    allocateBranchIslandAux(island, originalFunctionAddress, true);
  if (!err)
    return err;
  return allocateBranchIslandAux(island, originalFunctionAddress, false);
}


/*******************************************************************************
	Implementation: Deallocates memory for a branch island.

	@param	island	->	The island to deallocate.
	@result			<-	mach_error_t

	***************************************************************************/

	mach_error_t
freeBranchIsland(
		BranchIsland	*island )
{
	assert( island );
	assert( (*(long*)&island->instructions[0]) == kIslandTemplate[0] );
	assert( sizeof( BranchIsland ) <= kPageSize );
	return vm_deallocate( mach_task_self(), (vm_address_t) island,
			      kPageSize );
}

/*******************************************************************************
	Implementation: Sets the branch island's target, with an optional
	instruction.

	@param	island		->	The branch island to insert target into.
	@param	branchTo	->	The address of the target.
	@param	instruction	->	Optional instruction to execute prior to branch. Set
							to zero for nop.
	@result				<-	mach_error_t

	***************************************************************************/
#if defined(__ppc__) || defined(__POWERPC__)
	mach_error_t
setBranchIslandTarget(
		BranchIsland	*island,
		const void		*branchTo,
		long			instruction )
{
	//	Copy over the template code.
    bcopy( kIslandTemplate, island->instructions, sizeof( kIslandTemplate ) );

    //	Fill in the address.
    ((short*)island->instructions)[kAddressLo] = ((long) branchTo) & 0x0000FFFF;
    ((short*)island->instructions)[kAddressHi]
    	= (((long) branchTo) >> 16) & 0x0000FFFF;

    //	Fill in the (optional) instuction.
    if( instruction != 0 ) {
        ((short*)island->instructions)[kInstructionLo]
        	= instruction & 0x0000FFFF;
        ((short*)island->instructions)[kInstructionHi]
        	= (instruction >> 16) & 0x0000FFFF;
    }

    //MakeDataExecutable( island->instructions, sizeof( kIslandTemplate ) );
	msync( island->instructions, sizeof( kIslandTemplate ), MS_INVALIDATE );

    return err_none;
}
#endif

#if defined(__i386__)
	mach_error_t
setBranchIslandTarget_i386(
	BranchIsland	*island,
	const void		*branchTo,
	char*			instructions )
{

	//	Copy over the template code.
    bcopy( kIslandTemplate, island->instructions, sizeof( kIslandTemplate ) );

	// copy original instructions
	if (instructions) {
		bcopy (instructions, island->instructions + kInstructions, kOriginalInstructionsSize);
	}

    // Fill in the address.
    int32_t addressOffset = (char *)branchTo - (island->instructions + kJumpAddress + 4);
    *((int32_t *)(island->instructions + kJumpAddress)) = addressOffset;

    msync( island->instructions, sizeof( kIslandTemplate ), MS_INVALIDATE );
    return err_none;
}

#elif defined(__x86_64__)
mach_error_t
setBranchIslandTarget_i386(
        BranchIsland	*island,
        const void		*branchTo,
        char*			instructions )
{
    // Copy over the template code.
    bcopy( kIslandTemplate, island->instructions, sizeof( kIslandTemplate ) );

    // Copy original instructions.
    if (instructions) {
        bcopy (instructions, island->instructions, kOriginalInstructionsSize);
    }

    //	Fill in the address.
    *((uint64_t *)(island->instructions + kJumpAddress)) = (uint64_t)branchTo;
    msync( island->instructions, sizeof( kIslandTemplate ), MS_INVALIDATE );

    return err_none;
}
#endif


#if defined(__i386__) || defined(__x86_64__)
// simplistic instruction matching
typedef struct {
	unsigned int length; // max 15
	unsigned char mask[15]; // sequence of bytes in memory order
	unsigned char constraint[15]; // sequence of bytes in memory order
}	AsmInstructionMatch;

#if defined(__i386__)
static AsmInstructionMatch possibleInstructions[] = {
	// clang-format off
	{ 0x5, {0xFF, 0x00, 0x00, 0x00, 0x00}, {0xE9, 0x00, 0x00, 0x00, 0x00} },	// jmp 0x????????
	{ 0x5, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {0x55, 0x89, 0xe5, 0xc9, 0xc3} },	// push %ebp; mov %esp,%ebp; leave; ret
	{ 0x1, {0xFF}, {0x90} },							// nop
	{ 0x1, {0xFF}, {0x55} },							// push %esp
	{ 0x2, {0xFF, 0xFF}, {0x89, 0xE5} },				                // mov %esp,%ebp
	{ 0x1, {0xFF}, {0x53} },							// push %ebx
	{ 0x3, {0xFF, 0xFF, 0x00}, {0x83, 0xEC, 0x00} },	                        // sub 0x??, %esp
	{ 0x6, {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00}, {0x81, 0xEC, 0x00, 0x00, 0x00, 0x00} },	// sub 0x??, %esp with 32bit immediate
	{ 0x1, {0xFF}, {0x57} },							// push %edi
	{ 0x1, {0xFF}, {0x56} },							// push %esi
	{ 0x2, {0xFF, 0xFF}, {0x31, 0xC0} },						// xor %eax, %eax
	{ 0x3, {0xFF, 0x4F, 0x00}, {0x8B, 0x45, 0x00} },  // mov $imm(%ebp), %reg
	{ 0x3, {0xFF, 0x4C, 0x00}, {0x8B, 0x40, 0x00} },  // mov $imm(%eax-%edx), %reg
	{ 0x4, {0xFF, 0xFF, 0xFF, 0x00}, {0x8B, 0x4C, 0x24, 0x00} },  // mov $imm(%esp), %ecx
	{ 0x5, {0xFF, 0x00, 0x00, 0x00, 0x00}, {0xB8, 0x00, 0x00, 0x00, 0x00} },	// mov $imm, %eax
	{ 0x6, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {0xE8, 0x00, 0x00, 0x00, 0x00, 0x58} },	// call $imm; pop %eax
	{ 0x0 }
	// clang-format on
};
#elif defined(__x86_64__)
static AsmInstructionMatch possibleInstructions[] = {
	// clang-format off
	{ 0x5, {0xFF, 0x00, 0x00, 0x00, 0x00}, {0xE9, 0x00, 0x00, 0x00, 0x00} },	// jmp 0x????????
	{ 0x1, {0xFF}, {0x90} },							// nop
	{ 0x1, {0xF8}, {0x50} },							// push %rX
	{ 0x3, {0xFF, 0xFF, 0xFF}, {0x48, 0x89, 0xE5} },				// mov %rsp,%rbp
	{ 0x4, {0xFF, 0xFF, 0xFF, 0x00}, {0x48, 0x83, 0xEC, 0x00} },	                // sub 0x??, %rsp
	{ 0x4, {0xFB, 0xFF, 0x00, 0x00}, {0x48, 0x89, 0x00, 0x00} },	                // move onto rbp
	{ 0x4, {0xFF, 0xFF, 0xFF, 0xFF}, {0x40, 0x0f, 0xbe, 0xce} },			// movsbl %sil, %ecx
	{ 0x2, {0xFF, 0x00}, {0x41, 0x00} },						// push %rXX
	{ 0x2, {0xFF, 0x00}, {0x85, 0x00} },						// test %rX,%rX
	{ 0x5, {0xF8, 0x00, 0x00, 0x00, 0x00}, {0xB8, 0x00, 0x00, 0x00, 0x00} },   // mov $imm, %reg
	{ 0x3, {0xFF, 0xFF, 0x00}, {0xFF, 0x77, 0x00} },  // pushq $imm(%rdi)
	{ 0x2, {0xFF, 0xFF}, {0x31, 0xC0} },						// xor %eax, %eax
	{ 0x2, {0xFF, 0xFF}, {0x89, 0xF8} },			// mov %edi, %eax

	//leaq offset(%rip),%rax
	{ 0x7, {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00}, {0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00} },

	{ 0x0 }
	// clang-format on
};
#endif

static Boolean codeMatchesInstruction(unsigned char *code, AsmInstructionMatch* instruction)
{
	Boolean match = true;

	size_t i;
	for (i=0; i<instruction->length; i++) {
		unsigned char mask = instruction->mask[i];
		unsigned char constraint = instruction->constraint[i];
		unsigned char codeValue = code[i];

		match = ((codeValue & mask) == constraint);
		if (!match) break;
	}

	return match;
}

#if defined(__i386__) || defined(__x86_64__)
	static Boolean
eatKnownInstructions(
	unsigned char	*code,
	uint64_t		*newInstruction,
	int				*howManyEaten,
	char			*originalInstructions,
	int				*originalInstructionCount,
	uint8_t			*originalInstructionSizes )
{
	Boolean allInstructionsKnown = true;
	int totalEaten = 0;
	unsigned char* ptr = code;
	int remainsToEat = 5; // a JMP instruction takes 5 bytes
	int instructionIndex = 0;

	if (howManyEaten) *howManyEaten = 0;
	if (originalInstructionCount) *originalInstructionCount = 0;
	while (remainsToEat > 0) {
		Boolean curInstructionKnown = false;

		// See if instruction matches one  we know
		AsmInstructionMatch* curInstr = possibleInstructions;
		do {
			if ((curInstructionKnown = codeMatchesInstruction(ptr, curInstr))) break;
			curInstr++;
		} while (curInstr->length > 0);

		// if all instruction matches failed, we don't know current instruction then, stop here
		if (!curInstructionKnown) {
			allInstructionsKnown = false;
			fprintf(stderr, "mach_override: some instructions unknown! Need to update mach_override.c\n");
			break;
		}

		// At this point, we've matched curInstr
		int eaten = curInstr->length;
		ptr += eaten;
		remainsToEat -= eaten;
		totalEaten += eaten;

		if (originalInstructionSizes) originalInstructionSizes[instructionIndex] = eaten;
		instructionIndex += 1;
		if (originalInstructionCount) *originalInstructionCount = instructionIndex;
	}


	if (howManyEaten) *howManyEaten = totalEaten;

	if (originalInstructions) {
		Boolean enoughSpaceForOriginalInstructions = (totalEaten < kOriginalInstructionsSize);

		if (enoughSpaceForOriginalInstructions) {
			memset(originalInstructions, 0x90 /* NOP */, kOriginalInstructionsSize); // fill instructions with NOP
			bcopy(code, originalInstructions, totalEaten);
		} else {
			// printf ("Not enough space in island to store original instructions. Adapt the island definition and kOriginalInstructionsSize\n");
			return false;
		}
	}

	if (allInstructionsKnown) {
		// save last 3 bytes of first 64bits of codre we'll replace
		uint64_t currentFirst64BitsOfCode = *((uint64_t *)code);
		currentFirst64BitsOfCode = OSSwapInt64(currentFirst64BitsOfCode); // back to memory representation
		currentFirst64BitsOfCode &= 0x0000000000FFFFFFLL;

		// keep only last 3 instructions bytes, first 5 will be replaced by JMP instr
		*newInstruction &= 0xFFFFFFFFFF000000LL; // clear last 3 bytes
		*newInstruction |= (currentFirst64BitsOfCode & 0x0000000000FFFFFFLL); // set last 3 bytes
	}

	return allInstructionsKnown;
}

	static void
fixupInstructions(
	uint32_t	offset,
    void		*instructionsToFix,
	int			instructionCount,
	uint8_t		*instructionSizes )
{
	// The start of "leaq offset(%rip),%rax"
	static const uint8_t LeaqHeader[] = {0x48, 0x8d, 0x05};

	int	index;
	for (index = 0;index < instructionCount;index += 1)
	{
		if (*(uint8_t*)instructionsToFix == 0xE9) // 32-bit jump relative
		{
			uint32_t *jumpOffsetPtr = (uint32_t*)((uintptr_t)instructionsToFix + 1);
			*jumpOffsetPtr += offset;
		}

		// leaq offset(%rip),%rax
		if (memcmp(instructionsToFix, LeaqHeader, 3) == 0) {
			uint32_t *LeaqOffsetPtr = (uint32_t*)((uintptr_t)instructionsToFix + 3);
			*LeaqOffsetPtr += offset;
		}

		// 32-bit call relative to the next addr; pop %eax
		if (*(uint8_t*)instructionsToFix == 0xE8)
		{
			// Just this call is larger than the jump we use, so we
			// know this is the last instruction.
			assert(index == (instructionCount - 1));
			assert(instructionSizes[index] == 6);

                        // Insert "addl $offset, %eax" in the end so that when
                        // we jump to the rest of the function %eax has the
                        // value it would have if eip had been pushed by the
                        // call in its original position.
			uint8_t *op = instructionsToFix;
			op += 6;
			*op = 0x05; // addl
			uint32_t *addImmPtr = (uint32_t*)(op + 1);
			*addImmPtr = offset;
		}

		instructionsToFix = (void*)((uintptr_t)instructionsToFix + instructionSizes[index]);
    }
}
#endif

#if defined(__i386__)
__asm(
			".text;"
			".align 2, 0x90;"
			"_atomic_mov64:;"
			"	pushl %ebp;"
			"	movl %esp, %ebp;"
			"	pushl %esi;"
			"	pushl %ebx;"
			"	pushl %ecx;"
			"	pushl %eax;"
			"	pushl %edx;"

			// atomic push of value to an address
			// we use cmpxchg8b, which compares content of an address with
			// edx:eax. If they are equal, it atomically puts 64bit value
			// ecx:ebx in address.
			// We thus put contents of address in edx:eax to force ecx:ebx
			// in address
			"	mov		8(%ebp), %esi;"  // esi contains target address
			"	mov		12(%ebp), %ebx;"
			"	mov		16(%ebp), %ecx;" // ecx:ebx now contains value to put in target address
			"	mov		(%esi), %eax;"
			"	mov		4(%esi), %edx;"  // edx:eax now contains value currently contained in target address
			"	lock; cmpxchg8b	(%esi);" // atomic move.

			// restore registers
			"	popl %edx;"
			"	popl %eax;"
			"	popl %ecx;"
			"	popl %ebx;"
			"	popl %esi;"
			"	popl %ebp;"
			"	ret"
);
#elif defined(__x86_64__)
void atomic_mov64(
		uint64_t *targetAddress,
		uint64_t value )
{
    *targetAddress = value;
}
#endif
#endif
