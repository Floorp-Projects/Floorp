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

/*********************************************************************

FILENAME
	Exceptions.h
	
DESCRIPTION
	A collection of routines and macros to handle assertions and
	exceptions.

COPYRIGHT
	Copyright © Apple Computer, Inc. 1989-1991
	All rights reserved.

ROUTINES
	EXTERNALS
		dprintf
		check_dprintf
		checkpos_dprintf

MACROS
	EXTERNALS
		check
		ncheck
		check_action
		ncheck_action
		require
		nrequire
		require_action
		nrequire_action
		resume

MODIFICATION HISTORY
	Nov 12 95		BKJ		Moved to MetroWerks environment & the NSPR
		
NOTE
	To keep code size down, use these routines and macros with the C
	compiler option -b2 or -b3. This will eliminate duplicate strings
	within a procedure.

*********************************************************************/

#ifndef __MACERRORHANDLING__
#define __MACERRORHANDLING__

/*********************************************************************

INCLUDES

*********************************************************************/

#include	<Types.h>

/*<FF>*/
/*********************************************************************

CONSTANTS AND CONTROL

*********************************************************************/

/*
	These defines are used to control the amount of information
	displayed when an assertion fails. DEBUGOFF and WARN will run
	silently. MIN will simply break into the debugger. ON will break
	and display the assertion that failed and the exception (for
	require statements). FULL will also display the source file name
	and line number. SYM does a SysBreak and is usefull when using a
	symbolic debugger like SourceBug or SADE. They should be set into
	DEBUGLEVEL. The default LEVEL is OFF.
*/

#define DEBUGOFF		0
#define DEBUGWARN		1
#define DEBUGMIN		2
#define DEBUGON			3
#define DEBUGFULL		4
#define DEBUGSYM		6

#ifndef	DEBUGLEVEL
#define	DEBUGLEVEL	DEBUGOFF
#endif	DEBUGLEVEL

/*
	resumeLabel is used to control the insertion of labels for use with
	the resume macro. If you do not use the resume macro and you wish
	to have multible exceptions per label then you can add the
	following define to you source code.
	
*/
#define resumeLabel(exception)											// Multiple exceptions per label
// #define resumeLabel(exception)	resume_ ## exception:				// Single exception per label


/*
	traceon and debugon are used to test for options
*/

#define	traceon	((DEBUGLEVEL > DEBUGWARN) && defined(TRACEON))
#define	debugon	(DEBUGLEVEL > DEBUGWARN)

/*
	Add some macros for DEBUGMIN and DEBUGSYM to keep the size down.
*/

#define	__DEBUGSMALL	((DEBUGLEVEL == DEBUGMIN) ||			\
								 (DEBUGLEVEL == DEBUGSYM))

#if	DEBUGLEVEL == DEBUGMIN
#define	__DebuggerBreak	Debugger()
#elif	DEBUGLEVEL == DEBUGSYM
#define  __DebuggerBreak	SysBreak()
#endif


/*<FF>*/
/*********************************************************************

MACRO
	check(assertion)

DESCRIPTION
	If debugging is on then check will test assertion and if it fails
	break into the debugger. Otherwise check does nothing.

*********************************************************************/

#if	__DEBUGSMALL

#define check(assertion)															\
	do {																			\
		if (assertion) ;															\
		else __DebuggerBreak;														\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define check(assertion)															\
	do {																			\
		if (assertion) ;															\
		else {																		\
			dprintf(notrace, "Assertion \"%s\" Failed",	#assertion);				\
		}																			\
	} while (false)

#elif	DEBUGLEVEL == DEBUGFULL

#define check(assertion)															\
	do {																			\
		if (assertion) ;															\
		else {																		\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"							\
									"File: %s\n"									\
									"Line: %d",										\
				#assertion, __FILE__, __LINE__);									\
		}																			\
	} while (false)
	
#else

#define check(assertion)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	ncheck(assertion)

DESCRIPTION
	If debugging is on then ncheck will test !assertion and if it fails
	break into the debugger. Otherwise ncheck does nothing.

*********************************************************************/

#if	__DEBUGSMALL

#define ncheck(assertion)													\
	do {																	\
		if (assertion) __DebuggerBreak;										\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define ncheck(assertion)													\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace, "Assertion \"!(%s [= %#08X])\" Failed",		\
				#assertion, __privateAssertion);							\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGFULL

#define ncheck(assertion)													\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"File: %s\n"							\
									"Line: %d",								\
			#assertion, __privateAssertion, __FILE__, __LINE__);			\
		}																	\
	} while (false)

#else

#define ncheck(assertion)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	check_action(assertion, action)

DESCRIPTION
	If debugging is on then check_action will test assertion and if it
	fails break into the debugger then execute action. Otherwise
	check_action does nothing.
	
*********************************************************************/

#if	__DEBUGSMALL

#define check_action(assertion, action)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			__DebuggerBreak;												\
			{ action }														\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define check_action(assertion, action)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace, "Assertion \"%s\" Failed",	#assertion);		\
			{ action }														\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGFULL

#define check_action(assertion, action)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"					\
									"File: %s\n"							\
									"Line: %d",								\
				#assertion, __FILE__, __LINE__);							\
			{ action }														\
		}																	\
	} while (false)

#else

#define check_action(assertion, action)

#endif

/*<FF>*/
/**************************************************************************************

MACRO
	ncheck_action(assertion, action)

DESCRIPTION
	If debugging is on then ncheck_action will test !assertion and if
	it fails break into the debugger then execute action. Otherwise
	ncheck_action does nothing.

*********************************************************************/

#if	__DEBUGSMALL

#define ncheck_action(assertion, action)									\
	do {																	\
		if (assertion) {													\
			__DebuggerBreak;												\
			{ action }														\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define ncheck_action(assertion, action)									\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace, "Assertion \"!(%s [= %#08X])\" Failed",		\
				#assertion, __privateAssertion);							\
			{ action }														\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGFULL

#define ncheck_action(assertion, action)									\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"File: %s\n"							\
									"Line: %d",								\
			#assertion, __privateAssertion, __FILE__, __LINE__);			\
			{ action }														\
		}																	\
	} while (false)

#else

#define ncheck_action(assertion, action)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	require(assertion, exception)

DESCRIPTION
	require will test assertion and if it fails:
		break into the debugger if debugging is on.
		goto exception.

*********************************************************************/

#if	__DEBUGSMALL

#define require(assertion, exception)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			__DebuggerBreak;												\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define require(assertion, exception)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"					\
									"Exception \"%s\" Raised",				\
			#assertion, #exception);										\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGFULL

#define require(assertion, exception)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"					\
									"Exception \"%s\" Raised\n"				\
									"File: %s\n"							\
									"Line: %d",								\
				#assertion, #exception, __FILE__, __LINE__);				\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#else

#define require(assertion, exception)										\
	do {																	\
		if (assertion) ;													\
		else {																\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	nrequire(assertion, exception)

DESCRIPTION
	nrequire will test !assertion and if it fails:
		break into the debugger if debugging is on.
		goto exception.

*********************************************************************/

#if	__DEBUGSMALL

#define nrequire(assertion, exception)										\
	do {																	\
		if (assertion) {													\
			DebugStr();														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define nrequire(assertion, exception)										\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"Exception \"%s\" Raised",				\
				#assertion, __privateAssertion, #exception);				\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGFULL

#define nrequire(assertion, exception)										\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"Exception \"%s\" Raised\n"				\
									"File: %s\n"							\
									"Line: %d",								\
				#assertion, __privateAssertion, #exception, __FILE__,		\
				__LINE__);													\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#else

#define nrequire(assertion, exception)										\
	do {																	\
		if (assertion) {													\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	require_action(assertion, exception, action)

DESCRIPTION
	require_action will test assertion and if it fails:
		break into the debugger if debugging is on.
		execute action.
		goto exception.

*********************************************************************/

#if	__DEBUGSMALL

#define require_action(assertion, exception, action)						\
	do {																	\
		if (assertion) ;													\
		else {																\
			__DebuggerBreak;												\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif	DEBUGLEVEL == DEBUGON

#define require_action(assertion, exception, action)						\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"					\
									"Exception \"%s\" Raised",				\
			#assertion, #exception);										\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGFULL

#define require_action(assertion, exception, action)						\
	do {																	\
		if (assertion) ;													\
		else {																\
			dprintf(notrace,	"Assertion \"%s\" Failed\n"					\
									"Exception \"%s\" Raised\n"				\
									"File: %s\n"							\
									"Line: %d",								\
				#assertion, #exception, __FILE__, __LINE__);				\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#else

#define require_action(assertion, exception, action)						\
	do {																	\
		if (assertion) ;													\
		else {																\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#endif

/*<FF>*/
/*********************************************************************

MACRO
	nrequire_action(assertion, exception, action)

DESCRIPTION
	nrequire_action will test !assertion and if it fails:
		break into the debugger if debugging is on.
		execute action.
		goto exception.

*********************************************************************/

#if	__DEBUGSMALL

#define nrequire_action(assertion, exception, action)						\
	do {																	\
		if (assertion) {													\
			__DebuggerBreak;												\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGON

#define nrequire_action(assertion, exception, action)						\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"Exception \"%s\" Raised",				\
				#assertion, __privateAssertion, #exception);				\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#elif DEBUGLEVEL == DEBUGFULL

#define nrequire_action(assertion, exception, action)						\
	do {																	\
		void*	__privateAssertion	= (void*)(assertion);					\
																			\
		if (__privateAssertion) {											\
			dprintf(notrace,	"Assertion \"!(%s [= %#08X])\" Failed\n"	\
									"Exception \"%s\" Raised\n"				\
									"File: %s\n"							\
									"Line: %d",								\
				#assertion, __privateAssertion, #exception, __FILE__,		\
				__LINE__);													\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#else

#define nrequire_action(assertion, exception, action)						\
	do {																	\
		if (assertion) {													\
			{ action }														\
			goto exception;													\
			resumeLabel(exception);											\
		}																	\
	} while (false)

#endif
	
/*<FF>*/
/*********************************************************************

MACRO
	resume(exception)

DESCRIPTION
	resume will resume execution after the n/require/_action statement
	specified by exception. Resume lables must be on (the default) in
	order to use resume. If an action form of require was used then the
	action will not be re-executed.

*********************************************************************/


#define resume(exception)													\
	do {																	\
		goto resume_ ## exception;											\
	} while (false)


/*<FF>*/
/********************************************************************/
#endif
