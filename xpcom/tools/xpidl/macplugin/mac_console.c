#include "mac_console.h"

#ifndef __CONSOLE__
#include <console.h>
#endif

extern CWPluginContext gPluginContext;

UInt32 mac_console_count = 0;
CWMemHandle mac_console_handle = NULL;

/*
 *	The following four functions provide the UI for the console package.
 *	Users wishing to replace SIOUX with their own console package need
 *	only provide the four functions below in a library.
 */

/*
 *	extern short InstallConsole(short fd);
 *
 *	Installs the Console package, this function will be called right
 *	before any read or write to one of the standard streams.
 *
 *	short fd:		The stream which we are reading/writing to/from.
 *	returns short:	0 no error occurred, anything else error.
 */

short InstallConsole(short fd)
{
#pragma unused (fd)
	mac_console_count = 0;
	CWAllocMemHandle(gPluginContext, 8192, false, &mac_console_handle);
	return 0;
}

/*
 *	extern void RemoveConsole(void);
 *
 *	Removes the console package.  It is called after all other streams
 *	are closed and exit functions (installed by either atexit or _atexit)
 *	have been called.  Since there is no way to recover from an error,
 *	this function doesn't need to return any.
 */

void RemoveConsole(void)
{
	if (mac_console_handle != NULL) {
		CWFreeMemHandle(gPluginContext, mac_console_handle);
		mac_console_handle = NULL;
	}
}

/*
 *	extern long WriteCharsToConsole(char *buffer, long n);
 *
 *	Writes a stream of output to the Console window.  This function is
 *	called by write.
 *
 *	char *buffer:	Pointer to the buffer to be written.
 *	long n:			The length of the buffer to be written.
 *	returns short:	Actual number of characters written to the stream,
 *					-1 if an error occurred.
 */

long WriteCharsToConsole(char *buffer, long n)
{
	long size = 0;
	char* ptr = NULL;

	if (CWGetMemHandleSize(gPluginContext, mac_console_handle, &size) == noErr) {
		if (mac_console_count + n >= size) {
			size += 8192;
			if (CWResizeMemHandle(gPluginContext, mac_console_handle, size) != noErr)
				return -1;
		}
	}

	if (CWLockMemHandle(gPluginContext, mac_console_handle, false, &ptr) == noErr) {
		BlockMoveData(buffer, ptr + mac_console_count, n);
		mac_console_count += n;
		CWUnlockMemHandle(gPluginContext, mac_console_handle);
	}
	
	return 0;
}

/*
 *	extern long ReadCharsFromConsole(char *buffer, long n);
 *
 *	Reads from the Console into a buffer.  This function is called by
 *	read.
 *
 *	char *buffer:	Pointer to the buffer which will recieve the input.
 *	long n:			The maximum amount of characters to be read (size of
 *					buffer).
 *	returns short:	Actual number of characters read from the stream,
 *					-1 if an error occurred.
 */

long ReadCharsFromConsole(char *buffer, long n)
{
	return 0;
}
