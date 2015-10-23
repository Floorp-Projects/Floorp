/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mpi.h"
#include "prtypes.h"

/*
 * This file implements a single function: s_mpi_getProcessorLineSize();
 * s_mpi_getProcessorLineSize() returns the size in bytes of the cache line
 * if a cache exists, or zero if there is no cache. If more than one
 * cache line exists, it should return the smallest line size (which is 
 * usually the L1 cache).
 *
 * mp_modexp uses this information to make sure that private key information
 * isn't being leaked through the cache.
 *
 * Currently the file returns good data for most modern x86 processors, and
 * reasonable data on 64-bit ppc processors. All other processors are assumed
 * to have a cache line size of 32 bytes unless modified by target.mk.
 * 
 */

#if defined(i386) || defined(__i386) || defined(__X86__) || defined (_M_IX86) || defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64)
/* X86 processors have special instructions that tell us about the cache */
#include "string.h"

#if defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64)
#define AMD_64 1
#endif

/* Generic CPUID function */
#if defined(AMD_64)

#if defined(__GNUC__)

void freebl_cpuid(unsigned long op, unsigned long *eax, 
	                 unsigned long *ebx, unsigned long *ecx, 
                         unsigned long *edx)
{
	__asm__("cpuid\n\t"
		: "=a" (*eax),
		  "=b" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (op));
}

#elif defined(_MSC_VER)

#include <intrin.h>

void freebl_cpuid(unsigned long op, unsigned long *eax, 
           unsigned long *ebx, unsigned long *ecx, 
           unsigned long *edx)
{
    int intrinsic_out[4];

    __cpuid(intrinsic_out, op);
    *eax = intrinsic_out[0];
    *ebx = intrinsic_out[1];
    *ecx = intrinsic_out[2];
    *edx = intrinsic_out[3];
}

#endif

#else /* !defined(AMD_64) */

/* x86 */

#if defined(__GNUC__)
void freebl_cpuid(unsigned long op, unsigned long *eax, 
	                 unsigned long *ebx, unsigned long *ecx, 
                         unsigned long *edx)
{
/* sigh GCC isn't smart enough to save the ebx PIC register on it's own
 * in this case, so do it by hand. Use edi to store ebx and pass the
 * value returned in ebx from cpuid through edi. */
	__asm__("mov %%ebx,%%edi\n\t"
		  "cpuid\n\t"
		  "xchgl %%ebx,%%edi\n\t"
		: "=a" (*eax),
		  "=D" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (op));
}

/*
 * try flipping a processor flag to determine CPU type
 */
static unsigned long changeFlag(unsigned long flag)
{
	unsigned long changedFlags, originalFlags;
	__asm__("pushfl\n\t"            /* get the flags */
	        "popl %0\n\t"
	        "movl %0,%1\n\t"	/* save the original flags */
	        "xorl %2,%0\n\t" 	/* flip the bit */
		"pushl %0\n\t"  	/* set the flags */
	        "popfl\n\t"
		"pushfl\n\t"		/* get the flags again (for return) */
		"popl %0\n\t"
		"pushl %1\n\t"		/* restore the original flags */
		 "popfl\n\t"
		: "=r" (changedFlags),
		  "=r" (originalFlags),
		  "=r" (flag)
		: "2" (flag));
	return changedFlags ^ originalFlags;
}

#elif defined(_MSC_VER)

/*
 * windows versions of the above assembler
 */
#define wcpuid __asm __emit 0fh __asm __emit 0a2h
void freebl_cpuid(unsigned long op,    unsigned long *Reax, 
    unsigned long *Rebx, unsigned long *Recx, unsigned long *Redx)
{
        unsigned long  Leax, Lebx, Lecx, Ledx;
        __asm {
        pushad
        mov     eax,op
        wcpuid
        mov     Leax,eax
        mov     Lebx,ebx
        mov     Lecx,ecx
        mov     Ledx,edx
        popad
        }
        *Reax = Leax;
        *Rebx = Lebx;
        *Recx = Lecx;
        *Redx = Ledx;
}

static unsigned long changeFlag(unsigned long flag)
{
	unsigned long changedFlags, originalFlags;
	__asm {
		push eax
		push ebx
		pushfd 	                /* get the flags */
	        pop  eax
		push eax		/* save the flags on the stack */
	        mov  originalFlags,eax  /* save the original flags */
		mov  ebx,flag
	        xor  eax,ebx            /* flip the bit */
		push eax                /* set the flags */
	        popfd
		pushfd                  /* get the flags again (for return) */
		pop  eax	
		popfd                   /* restore the original flags */
		mov changedFlags,eax
		pop ebx
		pop eax
	}
	return changedFlags ^ originalFlags;
}
#endif

#endif

#if !defined(AMD_64)
#define AC_FLAG 0x40000
#define ID_FLAG 0x200000

/* 386 processors can't flip the AC_FLAG, intel AP Note AP-485 */
static int is386()
{
    return changeFlag(AC_FLAG) == 0;
}

/* 486 processors can't flip the ID_FLAG, intel AP Note AP-485 */
static int is486()
{
    return changeFlag(ID_FLAG) == 0;
}
#endif


/*
 * table for Intel Cache.
 * See Intel Application Note AP-485 for more information 
 */

typedef unsigned char CacheTypeEntry;

typedef enum {
    Cache_NONE    = 0,
    Cache_UNKNOWN = 1,
    Cache_TLB     = 2,
    Cache_TLBi    = 3,
    Cache_TLBd    = 4,
    Cache_Trace   = 5,
    Cache_L1      = 6,
    Cache_L1i     = 7,
    Cache_L1d     = 8,
    Cache_L2      = 9 ,
    Cache_L2i     = 10 ,
    Cache_L2d     = 11 ,
    Cache_L3      = 12 ,
    Cache_L3i     = 13,
    Cache_L3d     = 14
} CacheType;

struct _cache {
    CacheTypeEntry type;
    unsigned char lineSize;
};
static const struct _cache CacheMap[256] = {
/* 00 */ {Cache_NONE,    0   },
/* 01 */ {Cache_TLBi,    0   },
/* 02 */ {Cache_TLBi,    0   },
/* 03 */ {Cache_TLBd,    0   },
/* 04 */ {Cache_TLBd,        },
/* 05 */ {Cache_UNKNOWN, 0   },
/* 06 */ {Cache_L1i,     32  },
/* 07 */ {Cache_UNKNOWN, 0   },
/* 08 */ {Cache_L1i,     32  },
/* 09 */ {Cache_UNKNOWN, 0   },
/* 0a */ {Cache_L1d,     32  },
/* 0b */ {Cache_UNKNOWN, 0   },
/* 0c */ {Cache_L1d,     32  },
/* 0d */ {Cache_UNKNOWN, 0   },
/* 0e */ {Cache_UNKNOWN, 0   },
/* 0f */ {Cache_UNKNOWN, 0   },
/* 10 */ {Cache_UNKNOWN, 0   },
/* 11 */ {Cache_UNKNOWN, 0   },
/* 12 */ {Cache_UNKNOWN, 0   },
/* 13 */ {Cache_UNKNOWN, 0   },
/* 14 */ {Cache_UNKNOWN, 0   },
/* 15 */ {Cache_UNKNOWN, 0   },
/* 16 */ {Cache_UNKNOWN, 0   },
/* 17 */ {Cache_UNKNOWN, 0   },
/* 18 */ {Cache_UNKNOWN, 0   },
/* 19 */ {Cache_UNKNOWN, 0   },
/* 1a */ {Cache_UNKNOWN, 0   },
/* 1b */ {Cache_UNKNOWN, 0   },
/* 1c */ {Cache_UNKNOWN, 0   },
/* 1d */ {Cache_UNKNOWN, 0   },
/* 1e */ {Cache_UNKNOWN, 0   },
/* 1f */ {Cache_UNKNOWN, 0   },
/* 20 */ {Cache_UNKNOWN, 0   },
/* 21 */ {Cache_UNKNOWN, 0   },
/* 22 */ {Cache_L3,      64  },
/* 23 */ {Cache_L3,      64  },
/* 24 */ {Cache_UNKNOWN, 0   },
/* 25 */ {Cache_L3,      64  },
/* 26 */ {Cache_UNKNOWN, 0   },
/* 27 */ {Cache_UNKNOWN, 0   },
/* 28 */ {Cache_UNKNOWN, 0   },
/* 29 */ {Cache_L3,      64  },
/* 2a */ {Cache_UNKNOWN, 0   },
/* 2b */ {Cache_UNKNOWN, 0   },
/* 2c */ {Cache_L1d,     64  },
/* 2d */ {Cache_UNKNOWN, 0   },
/* 2e */ {Cache_UNKNOWN, 0   },
/* 2f */ {Cache_UNKNOWN, 0   },
/* 30 */ {Cache_L1i,     64  },
/* 31 */ {Cache_UNKNOWN, 0   },
/* 32 */ {Cache_UNKNOWN, 0   },
/* 33 */ {Cache_UNKNOWN, 0   },
/* 34 */ {Cache_UNKNOWN, 0   },
/* 35 */ {Cache_UNKNOWN, 0   },
/* 36 */ {Cache_UNKNOWN, 0   },
/* 37 */ {Cache_UNKNOWN, 0   },
/* 38 */ {Cache_UNKNOWN, 0   },
/* 39 */ {Cache_L2,      64  },
/* 3a */ {Cache_UNKNOWN, 0   },
/* 3b */ {Cache_L2,      64  },
/* 3c */ {Cache_L2,      64  },
/* 3d */ {Cache_UNKNOWN, 0   },
/* 3e */ {Cache_UNKNOWN, 0   },
/* 3f */ {Cache_UNKNOWN, 0   },
/* 40 */ {Cache_L2,      0   },
/* 41 */ {Cache_L2,      32  },
/* 42 */ {Cache_L2,      32  },
/* 43 */ {Cache_L2,      32  },
/* 44 */ {Cache_L2,      32  },
/* 45 */ {Cache_L2,      32  },
/* 46 */ {Cache_UNKNOWN, 0   },
/* 47 */ {Cache_UNKNOWN, 0   },
/* 48 */ {Cache_UNKNOWN, 0   },
/* 49 */ {Cache_UNKNOWN, 0   },
/* 4a */ {Cache_UNKNOWN, 0   },
/* 4b */ {Cache_UNKNOWN, 0   },
/* 4c */ {Cache_UNKNOWN, 0   },
/* 4d */ {Cache_UNKNOWN, 0   },
/* 4e */ {Cache_UNKNOWN, 0   },
/* 4f */ {Cache_UNKNOWN, 0   },
/* 50 */ {Cache_TLBi,    0   },
/* 51 */ {Cache_TLBi,    0   },
/* 52 */ {Cache_TLBi,    0   },
/* 53 */ {Cache_UNKNOWN, 0   },
/* 54 */ {Cache_UNKNOWN, 0   },
/* 55 */ {Cache_UNKNOWN, 0   },
/* 56 */ {Cache_UNKNOWN, 0   },
/* 57 */ {Cache_UNKNOWN, 0   },
/* 58 */ {Cache_UNKNOWN, 0   },
/* 59 */ {Cache_UNKNOWN, 0   },
/* 5a */ {Cache_UNKNOWN, 0   },
/* 5b */ {Cache_TLBd,    0   },
/* 5c */ {Cache_TLBd,    0   },
/* 5d */ {Cache_TLBd,    0   },
/* 5e */ {Cache_UNKNOWN, 0   },
/* 5f */ {Cache_UNKNOWN, 0   },
/* 60 */ {Cache_UNKNOWN, 0   },
/* 61 */ {Cache_UNKNOWN, 0   },
/* 62 */ {Cache_UNKNOWN, 0   },
/* 63 */ {Cache_UNKNOWN, 0   },
/* 64 */ {Cache_UNKNOWN, 0   },
/* 65 */ {Cache_UNKNOWN, 0   },
/* 66 */ {Cache_L1d,     64  },
/* 67 */ {Cache_L1d,     64  },
/* 68 */ {Cache_L1d,     64  },
/* 69 */ {Cache_UNKNOWN, 0   },
/* 6a */ {Cache_UNKNOWN, 0   },
/* 6b */ {Cache_UNKNOWN, 0   },
/* 6c */ {Cache_UNKNOWN, 0   },
/* 6d */ {Cache_UNKNOWN, 0   },
/* 6e */ {Cache_UNKNOWN, 0   },
/* 6f */ {Cache_UNKNOWN, 0   },
/* 70 */ {Cache_Trace,   1   },
/* 71 */ {Cache_Trace,   1   },
/* 72 */ {Cache_Trace,   1   },
/* 73 */ {Cache_UNKNOWN, 0   },
/* 74 */ {Cache_UNKNOWN, 0   },
/* 75 */ {Cache_UNKNOWN, 0   },
/* 76 */ {Cache_UNKNOWN, 0   },
/* 77 */ {Cache_UNKNOWN, 0   },
/* 78 */ {Cache_UNKNOWN, 0   },
/* 79 */ {Cache_L2,      64  },
/* 7a */ {Cache_L2,      64  },
/* 7b */ {Cache_L2,      64  },
/* 7c */ {Cache_L2,      64  },
/* 7d */ {Cache_UNKNOWN, 0   },
/* 7e */ {Cache_UNKNOWN, 0   },
/* 7f */ {Cache_UNKNOWN, 0   },
/* 80 */ {Cache_UNKNOWN, 0   },
/* 81 */ {Cache_UNKNOWN, 0   },
/* 82 */ {Cache_L2,      32  },
/* 83 */ {Cache_L2,      32  },
/* 84 */ {Cache_L2,      32  },
/* 85 */ {Cache_L2,      32  },
/* 86 */ {Cache_L2,      64  },
/* 87 */ {Cache_L2,      64  },
/* 88 */ {Cache_UNKNOWN, 0   },
/* 89 */ {Cache_UNKNOWN, 0   },
/* 8a */ {Cache_UNKNOWN, 0   },
/* 8b */ {Cache_UNKNOWN, 0   },
/* 8c */ {Cache_UNKNOWN, 0   },
/* 8d */ {Cache_UNKNOWN, 0   },
/* 8e */ {Cache_UNKNOWN, 0   },
/* 8f */ {Cache_UNKNOWN, 0   },
/* 90 */ {Cache_UNKNOWN, 0   },
/* 91 */ {Cache_UNKNOWN, 0   },
/* 92 */ {Cache_UNKNOWN, 0   },
/* 93 */ {Cache_UNKNOWN, 0   },
/* 94 */ {Cache_UNKNOWN, 0   },
/* 95 */ {Cache_UNKNOWN, 0   },
/* 96 */ {Cache_UNKNOWN, 0   },
/* 97 */ {Cache_UNKNOWN, 0   },
/* 98 */ {Cache_UNKNOWN, 0   },
/* 99 */ {Cache_UNKNOWN, 0   },
/* 9a */ {Cache_UNKNOWN, 0   },
/* 9b */ {Cache_UNKNOWN, 0   },
/* 9c */ {Cache_UNKNOWN, 0   },
/* 9d */ {Cache_UNKNOWN, 0   },
/* 9e */ {Cache_UNKNOWN, 0   },
/* 9f */ {Cache_UNKNOWN, 0   },
/* a0 */ {Cache_UNKNOWN, 0   },
/* a1 */ {Cache_UNKNOWN, 0   },
/* a2 */ {Cache_UNKNOWN, 0   },
/* a3 */ {Cache_UNKNOWN, 0   },
/* a4 */ {Cache_UNKNOWN, 0   },
/* a5 */ {Cache_UNKNOWN, 0   },
/* a6 */ {Cache_UNKNOWN, 0   },
/* a7 */ {Cache_UNKNOWN, 0   },
/* a8 */ {Cache_UNKNOWN, 0   },
/* a9 */ {Cache_UNKNOWN, 0   },
/* aa */ {Cache_UNKNOWN, 0   },
/* ab */ {Cache_UNKNOWN, 0   },
/* ac */ {Cache_UNKNOWN, 0   },
/* ad */ {Cache_UNKNOWN, 0   },
/* ae */ {Cache_UNKNOWN, 0   },
/* af */ {Cache_UNKNOWN, 0   },
/* b0 */ {Cache_TLBi,    0   },
/* b1 */ {Cache_UNKNOWN, 0   },
/* b2 */ {Cache_UNKNOWN, 0   },
/* b3 */ {Cache_TLBd,    0   },
/* b4 */ {Cache_UNKNOWN, 0   },
/* b5 */ {Cache_UNKNOWN, 0   },
/* b6 */ {Cache_UNKNOWN, 0   },
/* b7 */ {Cache_UNKNOWN, 0   },
/* b8 */ {Cache_UNKNOWN, 0   },
/* b9 */ {Cache_UNKNOWN, 0   },
/* ba */ {Cache_UNKNOWN, 0   },
/* bb */ {Cache_UNKNOWN, 0   },
/* bc */ {Cache_UNKNOWN, 0   },
/* bd */ {Cache_UNKNOWN, 0   },
/* be */ {Cache_UNKNOWN, 0   },
/* bf */ {Cache_UNKNOWN, 0   },
/* c0 */ {Cache_UNKNOWN, 0   },
/* c1 */ {Cache_UNKNOWN, 0   },
/* c2 */ {Cache_UNKNOWN, 0   },
/* c3 */ {Cache_UNKNOWN, 0   },
/* c4 */ {Cache_UNKNOWN, 0   },
/* c5 */ {Cache_UNKNOWN, 0   },
/* c6 */ {Cache_UNKNOWN, 0   },
/* c7 */ {Cache_UNKNOWN, 0   },
/* c8 */ {Cache_UNKNOWN, 0   },
/* c9 */ {Cache_UNKNOWN, 0   },
/* ca */ {Cache_UNKNOWN, 0   },
/* cb */ {Cache_UNKNOWN, 0   },
/* cc */ {Cache_UNKNOWN, 0   },
/* cd */ {Cache_UNKNOWN, 0   },
/* ce */ {Cache_UNKNOWN, 0   },
/* cf */ {Cache_UNKNOWN, 0   },
/* d0 */ {Cache_UNKNOWN, 0   },
/* d1 */ {Cache_UNKNOWN, 0   },
/* d2 */ {Cache_UNKNOWN, 0   },
/* d3 */ {Cache_UNKNOWN, 0   },
/* d4 */ {Cache_UNKNOWN, 0   },
/* d5 */ {Cache_UNKNOWN, 0   },
/* d6 */ {Cache_UNKNOWN, 0   },
/* d7 */ {Cache_UNKNOWN, 0   },
/* d8 */ {Cache_UNKNOWN, 0   },
/* d9 */ {Cache_UNKNOWN, 0   },
/* da */ {Cache_UNKNOWN, 0   },
/* db */ {Cache_UNKNOWN, 0   },
/* dc */ {Cache_UNKNOWN, 0   },
/* dd */ {Cache_UNKNOWN, 0   },
/* de */ {Cache_UNKNOWN, 0   },
/* df */ {Cache_UNKNOWN, 0   },
/* e0 */ {Cache_UNKNOWN, 0   },
/* e1 */ {Cache_UNKNOWN, 0   },
/* e2 */ {Cache_UNKNOWN, 0   },
/* e3 */ {Cache_UNKNOWN, 0   },
/* e4 */ {Cache_UNKNOWN, 0   },
/* e5 */ {Cache_UNKNOWN, 0   },
/* e6 */ {Cache_UNKNOWN, 0   },
/* e7 */ {Cache_UNKNOWN, 0   },
/* e8 */ {Cache_UNKNOWN, 0   },
/* e9 */ {Cache_UNKNOWN, 0   },
/* ea */ {Cache_UNKNOWN, 0   },
/* eb */ {Cache_UNKNOWN, 0   },
/* ec */ {Cache_UNKNOWN, 0   },
/* ed */ {Cache_UNKNOWN, 0   },
/* ee */ {Cache_UNKNOWN, 0   },
/* ef */ {Cache_UNKNOWN, 0   },
/* f0 */ {Cache_UNKNOWN, 0   },
/* f1 */ {Cache_UNKNOWN, 0   },
/* f2 */ {Cache_UNKNOWN, 0   },
/* f3 */ {Cache_UNKNOWN, 0   },
/* f4 */ {Cache_UNKNOWN, 0   },
/* f5 */ {Cache_UNKNOWN, 0   },
/* f6 */ {Cache_UNKNOWN, 0   },
/* f7 */ {Cache_UNKNOWN, 0   },
/* f8 */ {Cache_UNKNOWN, 0   },
/* f9 */ {Cache_UNKNOWN, 0   },
/* fa */ {Cache_UNKNOWN, 0   },
/* fb */ {Cache_UNKNOWN, 0   },
/* fc */ {Cache_UNKNOWN, 0   },
/* fd */ {Cache_UNKNOWN, 0   },
/* fe */ {Cache_UNKNOWN, 0   },
/* ff */ {Cache_UNKNOWN, 0   }
};


/*
 * use the above table to determine the CacheEntryLineSize.
 */
static void
getIntelCacheEntryLineSize(unsigned long val, int *level, 
						unsigned long *lineSize)
{
    CacheType type;

    type = CacheMap[val].type;
    /* only interested in data caches */
    /* NOTE val = 0x40 is a special value that means no L2 or L3 cache.
     * this data check has the side effect of rejecting that entry. If
     * that wasn't the case, we could have to reject it explicitly */
    if (CacheMap[val].lineSize == 0) {
	return;
    }
    /* look at the caches, skip types we aren't interested in.
     * if we already have a value for a lower level cache, skip the
     * current entry */
    if ((type == Cache_L1)|| (type == Cache_L1d)) {
	*level = 1;
	*lineSize = CacheMap[val].lineSize;
    } else if ((*level >= 2) && ((type == Cache_L2) || (type == Cache_L2d))) {
	*level = 2;
	*lineSize = CacheMap[val].lineSize;
    } else if ((*level >= 3) && ((type == Cache_L3) || (type == Cache_L3d))) {
	*level = 3;
	*lineSize = CacheMap[val].lineSize;
    }
    return;
}


static void
getIntelRegisterCacheLineSize(unsigned long val, 
			int *level, unsigned long *lineSize)
{
    getIntelCacheEntryLineSize(val >> 24 & 0xff, level, lineSize);
    getIntelCacheEntryLineSize(val >> 16 & 0xff, level, lineSize);
    getIntelCacheEntryLineSize(val >> 8 & 0xff, level, lineSize);
    getIntelCacheEntryLineSize(val & 0xff, level, lineSize);
}

/*
 * returns '0' if no recognized cache is found, or if the cache
 * information is supported by this processor 
 */
static unsigned long
getIntelCacheLineSize(int cpuidLevel)
{
    int level = 4;
    unsigned long lineSize = 0;
    unsigned long eax, ebx, ecx, edx;
    int repeat, count;

    if (cpuidLevel < 2) {
	return 0;
    }

    /* command '2' of the cpuid is intel's cache info call. Each byte of the
     * 4 registers contain a potential descriptor for the cache. The CacheMap	
     * table maps the cache entry with the processor cache. Register 'al'
     * contains a count value that cpuid '2' needs to be called in order to 
     * find all the cache descriptors. Only registers with the high bit set
     * to 'zero' have valid descriptors. This code loops through all the
     * required calls to cpuid '2' and passes any valid descriptors it finds
     * to the getIntelRegisterCacheLineSize code, which breaks the registers
     * down into their component descriptors. In the end the lineSize of the
     * lowest level cache data cache is returned. */
    freebl_cpuid(2, &eax, &ebx, &ecx, &edx);
    repeat = eax & 0xf;
    for (count = 0; count < repeat; count++) {
	if ((eax & 0x80000000) == 0) {
	    getIntelRegisterCacheLineSize(eax & 0xffffff00, &level, &lineSize);
	}
	if ((ebx & 0x80000000) == 0) {
	    getIntelRegisterCacheLineSize(ebx, &level, &lineSize);
	}
	if ((ecx & 0x80000000) == 0) {
	    getIntelRegisterCacheLineSize(ecx, &level, &lineSize);
	}
	if ((edx & 0x80000000) == 0) {
	    getIntelRegisterCacheLineSize(edx, &level, &lineSize);
	}
	if (count+1 != repeat) {
	    freebl_cpuid(2, &eax, &ebx, &ecx, &edx);
	}
    }
    return lineSize;
}

/*
 * returns '0' if the cache info is not supported by this processor.
 * This is based on the AMD extended cache commands for cpuid. 
 * (see "AMD Processor Recognition Application Note" Publication 20734).
 * Some other processors use the identical scheme.
 * (see "Processor Recognition, Transmeta Corporation").
 */
static unsigned long
getOtherCacheLineSize(unsigned long cpuidLevel)
{
    unsigned long lineSize = 0;
    unsigned long eax, ebx, ecx, edx;

    /* get the Extended CPUID level */
    freebl_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    cpuidLevel = eax;

    if (cpuidLevel >= 0x80000005) {
	freebl_cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
	lineSize = ecx & 0xff; /* line Size, L1 Data Cache */
    }
    return lineSize;
}

static const char * const manMap[] = {
#define INTEL     0
    "GenuineIntel",
#define AMD       1
    "AuthenticAMD",
#define CYRIX     2
    "CyrixInstead",
#define CENTAUR   2
    "CentaurHauls",
#define NEXGEN    3
    "NexGenDriven",
#define TRANSMETA 4
    "GenuineTMx86",
#define RISE      5
    "RiseRiseRise",
#define UMC       6
    "UMC UMC UMC ",
#define SIS       7
    "Sis Sis Sis ",
#define NATIONAL  8
    "Geode by NSC",
};

static const int n_manufacturers = sizeof(manMap)/sizeof(manMap[0]);


#define MAN_UNKNOWN 9

#if !defined(AMD_64)
#define SSE2_FLAG (1<<26)
unsigned long
s_mpi_is_sse2()
{
    unsigned long eax, ebx, ecx, edx;

    if (is386() || is486()) {
	return 0;
    }
    freebl_cpuid(0, &eax, &ebx, &ecx, &edx);

    /* has no SSE2 extensions */
    if (eax == 0) {
	return 0;
    }

    freebl_cpuid(1,&eax,&ebx,&ecx,&edx);
    return (edx & SSE2_FLAG) == SSE2_FLAG;
}
#endif

unsigned long
s_mpi_getProcessorLineSize()
{
    unsigned long eax, ebx, ecx, edx;
    PRUint32 cpuid[3];
    unsigned long cpuidLevel;
    unsigned long cacheLineSize = 0;
    int manufacturer = MAN_UNKNOWN;
    int i;
    char string[13];

#if !defined(AMD_64)
    if (is386()) {
	return 0; /* 386 had no cache */
    } if (is486()) {
	return 32; /* really? need more info */
    }
#endif

    /* Pentium, cpuid command is available */
    freebl_cpuid(0, &eax, &ebx, &ecx, &edx);
    cpuidLevel = eax;
    /* string holds the CPU's manufacturer ID string - a twelve
     * character ASCII string stored in ebx, edx, ecx, and
     * the 32-bit extended feature flags are in edx, ecx.
     */
    cpuid[0] = ebx;
    cpuid[1] = ecx;
    cpuid[2] = edx;
    memcpy(string, cpuid, sizeof(cpuid));
    string[12] = 0;

    manufacturer = MAN_UNKNOWN;
    for (i=0; i < n_manufacturers; i++) {
	if ( strcmp(manMap[i],string) == 0) {
	    manufacturer = i;
	}
    }

    if (manufacturer == INTEL) {
	cacheLineSize = getIntelCacheLineSize(cpuidLevel);
    } else {
	cacheLineSize = getOtherCacheLineSize(cpuidLevel);
    }
    /* doesn't support cache info based on cpuid. This means
     * an old pentium class processor, which have cache lines of
     * 32. If we learn differently, we can use a switch based on
     * the Manufacturer id  */
    if (cacheLineSize == 0) {
	cacheLineSize = 32;
    }
    return cacheLineSize;
}
#define MPI_GET_PROCESSOR_LINE_SIZE_DEFINED 1
#endif

#if defined(__ppc64__) 
/*
 *  Sigh, The PPC has some really nice features to help us determine cache
 *  size, since it had lots of direct control functions to do so. The POWER
 *  processor even has an instruction to do this, but it was dropped in
 *  PowerPC. Unfortunately most of them are not available in user mode.
 *
 *  The dcbz function would be a great way to determine cache line size except
 *  1) it only works on write-back memory (it throws an exception otherwise), 
 *  and 2) because so many mac programs 'knew' the processor cache size was
 *  32 bytes, they used this instruction as a fast 'zero 32 bytes'. Now the new
 *  G5 processor has 128 byte cache, but dcbz only clears 32 bytes to keep
 *  these programs happy. dcbzl work if 64 bit instructions are supported.
 *  If you know 64 bit instructions are supported, and that stack is 
 *  write-back, you can use this code.
 */
#include "memory.h"

/* clear the cache line that contains 'array' */
static inline void dcbzl(char *array)
{
	register char *a asm("r2") = array;
	__asm__ __volatile__( "dcbzl %0,r0" : "=r" (a): "0"(a) );
}


#define PPC_DO_ALIGN(x,y) ((char *)\
			((((long long) (x))+((y)-1))&~((y)-1)))

#define PPC_MAX_LINE_SIZE 256
unsigned long
s_mpi_getProcessorLineSize()
{
    char testArray[2*PPC_MAX_LINE_SIZE+1];
    char *test;
    int i;

    /* align the array on a maximum line size boundary, so we
     * know we are starting to clear from the first address */
    test = PPC_DO_ALIGN(testArray, PPC_MAX_LINE_SIZE); 
    /* set all the values to 1's */
    memset(test, 0xff, PPC_MAX_LINE_SIZE);
    /* clear one cache block starting at 'test' */
    dcbzl(test);

    /* find the size of the cleared area, that's our block size */
    for (i=PPC_MAX_LINE_SIZE; i != 0; i = i/2) {
	if (test[i-1] == 0) {
	    return i;
	}
    }
    return 0;
}

#define MPI_GET_PROCESSOR_LINE_SIZE_DEFINED 1
#endif


/*
 * put other processor and platform specific cache code here
 * return the smallest cache line size in bytes on the processor 
 * (usually the L1 cache). If the OS has a call, this would be
 * a greate place to put it.
 *
 * If there is no cache, return 0;
 * 
 * define MPI_GET_PROCESSOR_LINE_SIZE_DEFINED so the generic functions
 * below aren't compiled.
 *
 */


/* target.mk can define MPI_CACHE_LINE_SIZE if it's common for the family or 
 * OS */
#if defined(MPI_CACHE_LINE_SIZE) && !defined(MPI_GET_PROCESSOR_LINE_SIZE_DEFINED)

unsigned long
s_mpi_getProcessorLineSize()
{
   return MPI_CACHE_LINE_SIZE;
}
#define MPI_GET_PROCESSOR_LINE_SIZE_DEFINED 1
#endif


/* If no way to get the processor cache line size has been defined, assume
 * it's 32 bytes (most common value, does not significantly impact performance)
 */ 
#ifndef MPI_GET_PROCESSOR_LINE_SIZE_DEFINED
unsigned long
s_mpi_getProcessorLineSize()
{
   return 32;
}
#endif

#ifdef TEST_IT
#include <stdio.h>

main()
{
    printf("line size = %d\n", s_mpi_getProcessorLineSize());
} 
#endif
