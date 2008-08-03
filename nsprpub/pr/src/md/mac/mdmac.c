/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Types.h>
#include <Timer.h>
#include <Files.h>
#include <Errors.h>
#include <Folders.h>
#include <Gestalt.h>
#include <Events.h>
#include <Processes.h>
#include <TextUtils.h>
#include <MixedMode.h>
#include <LowMem.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stat.h>
#include <stdarg.h>
#include <unix.h>

#include "MacErrorHandling.h"

#include "primpl.h"
#include "prgc.h"

#include "mactime.h"

#include "mdmac.h"

// undefine getenv, so that _MD_GetEnv can call the version in NSStdLib::nsEnvironment.cpp.
#undef getenv

//
// Local routines
//
unsigned char GarbageCollectorCacheFlusher(PRUint32 size);

extern PRThread *gPrimaryThread;
extern ProcessSerialNumber gApplicationProcess;     // in macthr.c


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark CREATING MACINTOSH THREAD STACKS


enum {
	uppExitToShellProcInfo 				= kPascalStackBased,
	uppStackSpaceProcInfo				= kRegisterBased 
										  | RESULT_SIZE(SIZE_CODE(sizeof(long)))
										  | REGISTER_RESULT_LOCATION(kRegisterD0)
		 								  | REGISTER_ROUTINE_PARAMETER(1, kRegisterD1, SIZE_CODE(sizeof(UInt16)))
};

typedef CALLBACK_API( long , StackSpacePatchPtr )(UInt16 trapNo);
typedef REGISTER_UPP_TYPE(StackSpacePatchPtr)	StackSpacePatchUPP;

StackSpacePatchUPP	  gStackSpacePatchUPP = NULL;
UniversalProcPtr	  gStackSpacePatchCallThru = NULL;
long				(*gCallOSTrapUniversalProc)(UniversalProcPtr,ProcInfoType,...) = NULL;


pascal long StackSpacePatch(UInt16 trapNo)
{
	char		tos;
	PRThread	*thisThread;
	
	thisThread = PR_GetCurrentThread();
	
	//	If we are the primary thread, then call through to the
	//	good ol' fashion stack space implementation.  Otherwise,
	//	compute it by hand.
	if ((thisThread == gPrimaryThread) || 	
		(&tos < thisThread->stack->stackBottom) || 
		(&tos > thisThread->stack->stackTop)) {
		return gCallOSTrapUniversalProc(gStackSpacePatchCallThru, uppStackSpaceProcInfo, trapNo);
	}
	else {
		return &tos - thisThread->stack->stackBottom;
	}
}


static void InstallStackSpacePatch(void)
{
	long				systemVersion;
	OSErr				err;
	CFragConnectionID	connID;
	Str255				errMessage;
	Ptr					interfaceLibAddr;
	CFragSymbolClass	symClass;
	UniversalProcPtr	(*getOSTrapAddressProc)(UInt16);
	void				(*setOSTrapAddressProc)(StackSpacePatchUPP, UInt16);
	UniversalProcPtr	(*newRoutineDescriptorProc)(ProcPtr,ProcInfoType,ISAType);
	

	err = Gestalt(gestaltSystemVersion,&systemVersion);
	if (systemVersion >= 0x00000A00)	// we don't need to patch StackSpace()
		return;

	// open connection to "InterfaceLib"
	err = GetSharedLibrary("\pInterfaceLib", kPowerPCCFragArch, kFindCFrag,
											&connID, &interfaceLibAddr, errMessage);
	PR_ASSERT(err == noErr);
	if (err != noErr)
		return;

	// get symbol GetOSTrapAddress
	err = FindSymbol(connID, "\pGetOSTrapAddress", &(Ptr)getOSTrapAddressProc, &symClass);
	if (err != noErr)
		return;

	// get symbol SetOSTrapAddress
	err = FindSymbol(connID, "\pSetOSTrapAddress", &(Ptr)setOSTrapAddressProc, &symClass);
	if (err != noErr)
		return;
	
	// get symbol NewRoutineDescriptor
	err = FindSymbol(connID, "\pNewRoutineDescriptor", &(Ptr)newRoutineDescriptorProc, &symClass);
	if (err != noErr)
		return;
	
	// get symbol CallOSTrapUniversalProc
	err = FindSymbol(connID, "\pCallOSTrapUniversalProc", &(Ptr)gCallOSTrapUniversalProc, &symClass);
	if (err != noErr)
		return;

	// get and set trap address for StackSpace (A065)
	gStackSpacePatchCallThru = getOSTrapAddressProc(0x0065);
	if (gStackSpacePatchCallThru)
	{
		gStackSpacePatchUPP =
			(StackSpacePatchUPP)newRoutineDescriptorProc((ProcPtr)(StackSpacePatch), uppStackSpaceProcInfo, GetCurrentArchitecture());
		setOSTrapAddressProc(gStackSpacePatchUPP, 0x0065);
	}

#if DEBUG
	StackSpace();
#endif
}


//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark ENVIRONMENT VARIABLES


typedef struct EnvVariable EnvVariable;

struct EnvVariable {
	char 			*variable;
	char			*value;
	EnvVariable		*next;
};

EnvVariable		*gEnvironmentVariables = NULL;

char *_MD_GetEnv(const char *name)
{
	EnvVariable 	*currentVariable = gEnvironmentVariables;

	while (currentVariable) {
		if (!strcmp(currentVariable->variable, name))
			return currentVariable->value;
			
		currentVariable = currentVariable->next;
	}

	return getenv(name);
}

PR_IMPLEMENT(int) 
_MD_PutEnv(const char *string)
{
	EnvVariable 	*currentVariable = gEnvironmentVariables;
	char			*variableCopy,
				    *value,
					*current;
					
	variableCopy = strdup(string);
	PR_ASSERT(variableCopy != NULL);

	current = variableCopy;
	while (*current != '=')
		current++;

	*current = 0;
	current++;

	value = current;

	while (currentVariable) {
		if (!strcmp(currentVariable->variable, variableCopy))
			break;
		
		currentVariable = currentVariable->next;
	}

	if (currentVariable == NULL) {
		currentVariable = PR_NEW(EnvVariable);
		
		if (currentVariable == NULL) {
			PR_DELETE(variableCopy);
			return -1;
		}
		
		currentVariable->variable = strdup(variableCopy);
		currentVariable->value = strdup(value);
		currentVariable->next = gEnvironmentVariables;
		gEnvironmentVariables = currentVariable;
	}
	
	else {
		PR_DELETE(currentVariable->value);
		currentVariable->value = strdup(current);

		/* This is a temporary hack.  Working on a real fix, remove this when done. */
		/* OK, there are two ways to access the  */
		/* library path, getenv() and PR_GetLibraryPath().  Take a look at PR_GetLibraryPath(). */
		/* You'll see that we keep the path in a global which is intialized at startup from */
		/* a call to getenv().  From then on, they have nothing in common. */
		/* We need to keep them in synch.  */
		if (strcmp(currentVariable->variable, "LD_LIBRARY_PATH") == 0)
			PR_SetLibraryPath(currentVariable->value);
	}
	
	PR_DELETE(variableCopy);
	return 0;
}



//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark MISCELLANEOUS

PRWord *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
	(void) setjmp(t->md.jb);
    }
    *np = sizeof(t->md.jb) / sizeof(PRUint32);
    return (PRWord*) (t->md.jb);
}

void _MD_GetRegisters(PRUint32 *to)
{
  (void) setjmp((void*) to);
}

void _MD_EarlyInit()
{
	Handle				environmentVariables;

	GetCurrentProcess(&gApplicationProcess);

	INIT_CRITICAL_REGION();
	InitIdleSemaphore();

#if !defined(MAC_NSPR_STANDALONE)
	// MacintoshInitializeMemory();  Moved to mdmacmem.c: AllocateRawMemory(Size blockSize)
#else
	MacintoshInitializeMemory();
#endif
	MacintoshInitializeTime();
	
	//	Install resource-controlled environment variables.
	
	environmentVariables = GetResource('Envi', 128);
	if (environmentVariables != NULL) {
	
		Size 	resourceSize;
		char	*currentPutEnvString = (char *)*environmentVariables,
				*currentScanChar = currentPutEnvString;
				
		resourceSize = GetHandleSize(environmentVariables);			
		DetachResource(environmentVariables);
		HLock(environmentVariables);
		
		while (resourceSize--) {
		
			if ((*currentScanChar == '\n') || (*currentScanChar == '\r')) {
				*currentScanChar = 0;
				_MD_PutEnv (currentPutEnvString);
				currentPutEnvString = currentScanChar + 1;
			}
		
			currentScanChar++;
		
		}
		
		DisposeHandle(environmentVariables);

	}

#ifdef PR_INTERNAL_LOGGING
	_MD_PutEnv ("NSPR_LOG_MODULES=clock:6,cmon:6,io:6,mon:6,linker:6,cvar:6,sched:6,thread:6");
#endif

	InstallStackSpacePatch();
}

void _MD_FinalInit()
{
	_MD_InitNetAccess();
}

void PR_InitMemory(void) {
#ifndef NSPR_AS_SHARED_LIB
	//	Needed for Mac browsers without Java.  We don't want them calling PR_INIT, since it
	//	brings in all of the thread support.  But we do need to allow them to initialize
	//	the NSPR memory package.
	//	This should go away when all clients of the NSPR want threads AND memory.
	MacintoshInitializeMemory();
#endif
}

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark TERMINATION


//	THIS IS *** VERY *** IMPORTANT... our CFM Termination proc.
//	This allows us to deactivate our Time Mananger task even
//	if we are not totally gracefully exited.  If this is not
//	done then we will randomly crash at later times when the
//	task is called after the app heap is gone.

#if TARGET_CARBON
extern OTClientContextPtr	clientContext;
#define CLOSE_OPEN_TRANSPORT()	CloseOpenTransportInContext(clientContext)

#else

#define CLOSE_OPEN_TRANSPORT()	CloseOpenTransport()
#endif /* TARGET_CARBON */

extern pascal void __NSTerminate(void);

void CleanupTermProc(void)
{
	_MD_StopInterrupts();	// deactive Time Manager task

	CLOSE_OPEN_TRANSPORT();
	TermIdleSemaphore();
	TERM_CRITICAL_REGION();
	
	__NSTerminate();
}



//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark STRING OPERATIONS

#if !defined(MAC_NSPR_STANDALONE)

//	PStrFromCStr converts the source C string to a destination
//	pascal string as it copies. The dest string will
//	be truncated to fit into an Str255 if necessary.
//  If the C String pointer is NULL, the pascal string's length is set to zero
//
void 
PStrFromCStr(const char* src, Str255 dst)
{
	short 	length  = 0;
	
	// handle case of overlapping strings
	if ( (void*)src == (void*)dst )
	{
		unsigned char*		curdst = &dst[1];
		unsigned char		thisChar;
				
		thisChar = *(const unsigned char*)src++;
		while ( thisChar != '\0' ) 
		{
			unsigned char	nextChar;
			
			// use nextChar so we don't overwrite what we are about to read
			nextChar = *(const unsigned char*)src++;
			*curdst++ = thisChar;
			thisChar = nextChar;
			
			if ( ++length >= 255 )
				break;
		}
	}
	else if ( src != NULL )
	{
		unsigned char*		curdst = &dst[1];
		short 				overflow = 255;		// count down so test it loop is faster
		register char		temp;
	
		// Can't do the K&R C thing of "while (*s++ = *t++)" because it will copy trailing zero
		// which might overrun pascal buffer.  Instead we use a temp variable.
		while ( (temp = *src++) != 0 ) 
		{
			*(char*)curdst++ = temp;
				
			if ( --overflow <= 0 )
				break;
		}
		length = 255 - overflow;
	}
	dst[0] = length;
}


void CStrFromPStr(ConstStr255Param pString, char **cString)
{
	// Allocates a cString and copies a Pascal string into it.
	unsigned int	len;
	
	len = pString[0];
	*cString = malloc(len+1);
	
	if (*cString != NULL) {
		strncpy(*cString, (char *)&pString[1], len);
		(*cString)[len] = NULL;
	}
}


void dprintf(const char *format, ...)
{
#if DEBUG
    va_list ap;
	Str255 buffer;
	
	va_start(ap, format);
	buffer[0] = PR_vsnprintf((char *)buffer + 1, sizeof(buffer) - 1, format, ap);
	va_end(ap);
	
	DebugStr(buffer);
#endif /* DEBUG */
}

#else

void debugstr(const char *debuggerMsg)
{
	Str255		pStr;
	
	PStrFromCStr(debuggerMsg, pStr);
	DebugStr(pStr);
}


char *strdup(const char *source)
{
	char 	*newAllocation;
	size_t	stringLength;

	PR_ASSERT(source);
	
	stringLength = strlen(source) + 1;
	
	newAllocation = (char *)PR_MALLOC(stringLength);
	if (newAllocation == NULL)
		return NULL;
	BlockMoveData(source, newAllocation, stringLength);
	return newAllocation;
}

//	PStrFromCStr converts the source C string to a destination
//	pascal string as it copies. The dest string will
//	be truncated to fit into an Str255 if necessary.
//  If the C String pointer is NULL, the pascal string's length is set to zero
//
void PStrFromCStr(const char* src, Str255 dst)
{
	short 	length  = 0;
	
	// handle case of overlapping strings
	if ( (void*)src == (void*)dst )
	{
		unsigned char*		curdst = &dst[1];
		unsigned char		thisChar;
				
		thisChar = *(const unsigned char*)src++;
		while ( thisChar != '\0' ) 
		{
			unsigned char	nextChar;
			
			// use nextChar so we don't overwrite what we are about to read
			nextChar = *(const unsigned char*)src++;
			*curdst++ = thisChar;
			thisChar = nextChar;
			
			if ( ++length >= 255 )
				break;
		}
	}
	else if ( src != NULL )
	{
		unsigned char*		curdst = &dst[1];
		short 				overflow = 255;		// count down so test it loop is faster
		register char		temp;
	
		// Can't do the K&R C thing of "while (*s++ = *t++)" because it will copy trailing zero
		// which might overrun pascal buffer.  Instead we use a temp variable.
		while ( (temp = *src++) != 0 ) 
		{
			*(char*)curdst++ = temp;
				
			if ( --overflow <= 0 )
				break;
		}
		length = 255 - overflow;
	}
	dst[0] = length;
}


void CStrFromPStr(ConstStr255Param pString, char **cString)
{
	// Allocates a cString and copies a Pascal string into it.
	unsigned int	len;
	
	len = pString[0];
	*cString = PR_MALLOC(len+1);
	
	if (*cString != NULL) {
		strncpy(*cString, (char *)&pString[1], len);
		(*cString)[len] = NULL;
	}
}


size_t strlen(const char *source)
{
	size_t currentLength = 0;
	
	if (source == NULL)
		return currentLength;
			
	while (*source++ != '\0')
		currentLength++;
		
	return currentLength;
}

int strcmpcore(const char *str1, const char *str2, int caseSensitive)
{
	char 	currentChar1, currentChar2;

	while (1) {
	
		currentChar1 = *str1;
		currentChar2 = *str2;
		
		if (!caseSensitive) {
			
			if ((currentChar1 >= 'a') && (currentChar1 <= 'z'))
				currentChar1 += ('A' - 'a');
		
			if ((currentChar2 >= 'a') && (currentChar2 <= 'z'))
				currentChar2 += ('A' - 'a');
		
		}
	
		if (currentChar1 == '\0')
			break;
	
		if (currentChar1 != currentChar2)
			return currentChar1 - currentChar2;
			
		str1++;
		str2++;
	
	}
	
	return currentChar1 - currentChar2;
}

int strcmp(const char *str1, const char *str2)
{
	return strcmpcore(str1, str2, true);
}

int strcasecmp(const char *str1, const char *str2)
{
	return strcmpcore(str1, str2, false);
}


void *memcpy(void *to, const void *from, size_t size)
{
	if (size != 0) {
#if DEBUG
		if ((UInt32)to < 0x1000)
			DebugStr("\pmemcpy has illegal to argument");
		if ((UInt32)from < 0x1000)
			DebugStr("\pmemcpy has illegal from argument");
#endif
		BlockMoveData(from, to, size);
	}
	return to;
}

void dprintf(const char *format, ...)
{
    va_list ap;
	char	*buffer;
	
	va_start(ap, format);
	buffer = (char *)PR_vsmprintf(format, ap);
	va_end(ap);
	
	debugstr(buffer);
	PR_DELETE(buffer);
}

void
exit(int result)
{
#pragma unused (result)

		ExitToShell();
}

void abort(void)
{
	exit(-1);
}

#endif

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark FLUSHING THE GARBAGE COLLECTOR

#if !defined(MAC_NSPR_STANDALONE)

unsigned char GarbageCollectorCacheFlusher(PRUint32)
{

    PRIntn is;

	UInt32		oldPriority;

	//	If java wasn't completely initialized, then bail 
	//	harmlessly.
	
	if (PR_GetGCInfo()->lock == NULL)
		return false;

#if DEBUG
	if (_MD_GET_INTSOFF() == 1)
		DebugStr("\pGarbageCollectorCacheFlusher at interrupt time!");
#endif

	//	The synchronization here is very tricky.  We really
	//	don't want any other threads to run while we are 
	//	cleaning up the gc heap... they could call malloc,
	//	and then we would be in trouble in a big way.  So,
	//	we jack up our priority and that of the finalizer
	//	so that we won't yield to other threads.
	//	dkc 5/17/96

	oldPriority = PR_GetThreadPriority(PR_GetCurrentThread());
	_PR_INTSOFF(is);
	_PR_SetThreadPriority(PR_GetCurrentThread(), (PRThreadPriority)30);
	_PR_INTSON(is);

	//	Garbage collect twice.  This will finalize any
	//	dangling AWT resources (images, components), and
	//	then free up their GC space, too.
	//	dkc 2/15/96
	//  interrupts must be on during PR_GC
	
	PR_GC();
	
	//	By setting the finalizer priority to 31, then we 
	//	ensure it will run before us.  When it finishes
	//	its list of finalizations, it returns to us
	//	for the second garbage collection.
	
	PR_Yield();

	PR_GC();
	
	//	Restore our old priorities.
	
	_PR_INTSOFF(is);
	_PR_SetThreadPriority(PR_GetCurrentThread(), (PRThreadPriority)oldPriority);
	_PR_INTSON(is);

	return false;
}

#endif

//##############################################################################
//##############################################################################
#pragma mark -
#pragma mark MISCELLANEOUS-HACKS


//
//		***** HACK  FIX THESE ****
//
extern long _MD_GetOSName(char *buf, long count)
{
	long	len;
	
	len = PR_snprintf(buf, count, "Mac OS");
	
	return 0;
}

extern long _MD_GetOSVersion(char *buf, long count)
{
	long	len;
	
	len = PR_snprintf(buf, count, "7.5");

	return 0;
}

extern long _MD_GetArchitecture(char *buf, long count)
{
	long	len;
	
#if defined(TARGET_CPU_PPC) && TARGET_CPU_PPC	
	len = PR_snprintf(buf, count, "PowerPC");
#else
	len = PR_snprintf(buf, count, "Motorola68k");
#endif

	return 0;
}
