/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* pw_public.h
 * public progress module interface
 * Progress module has two main parts:
 * progress window
 *    you can create a standard progress window
 *    that monitors the progress of a download
 * progress context
 *    you can create an MWContext suitable for downloading
 *    with NET_GetURL
 * progress context can be 'attached' to progress window and
 * will display standard progress messages during download while
 * it is attached
 */

#ifndef _PW_PUBLIC_H_
#define _PW_PUBLIC_H_

#include "structs.h"

XP_BEGIN_PROTOS

/***************************************************************
 * PW window interface
 ***************************************************************/

typedef void *  pw_ptr;

typedef enum _PW_WindowType {
	pwApplicationModal,
	pwStandard
} PW_WindowType;

typedef void(*PW_CancelCallback) (void * closure);

/* Creates the progress window */
/* The window is invisible until you call PW_Show */

pw_ptr PW_Create( MWContext * parent, 		/* Parent window, can be NULL */
				PW_WindowType type			/* What kind of window ? Modality, etc */
				);

/* Setup */

void PW_SetCancelCallback(pw_ptr pw,
				PW_CancelCallback cancelcb,	/* Callback function for cancel */
				void * cancelClosure);		/* closure for cancel callback */

/*
 * Window visibility
 */

/* Show the window */
/* brings the window to front if already visible */
void PW_Show(pw_ptr pw);

/* Hide the window. Does NOT destroy it */
void PW_Hide(pw_ptr pw);

/* Destroys the window */
void PW_Destroy(pw_ptr pw);


/**
 * Text progress display
 * all char * arguments can be NULL to erase
 **/
 
void PW_SetWindowTitle(pw_ptr pw, const char * title);

void PW_SetLine1(pw_ptr pw, const char * text);

void PW_SetLine2(pw_ptr pw, const char * text);

/* aka SetLine3, named SetProgressText because this is where the
 * usual "23K of 1235K (5 seconds remaining)" line goes
 */
void PW_SetProgressText(pw_ptr pw, const char * text);

/**
 * Progress bar display
 **/

/* if !(maximum > minimum) displays rotating pylon */
void PW_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum);

void PW_SetProgressValue(pw_ptr pw, int32 value);

/***************************************************************
 * PW context interface
 ***************************************************************/

/* Purpose: create a downloading context anyone can use and destroy */
/* You can associate it with progress window to automatically display */
/* progress window text */

MWContext * PW_CreateProgressContext();

void PW_DestroyProgressContext(MWContext * context);

/* Associates MWContextProgressModule context with a downloading window */
/* If done so, the xp_thermo progress stuff will be displayed in the */
/* ProgressLine */
/* Make sure that you disassociate window from the context when window is */
/* destroyed by passing NULL as pw_ptr */

void PW_AssociateWindowWithContext(MWContext * context, pw_ptr pw);

pw_ptr PW_GetAssociatedWindowForContext(MWContext *context);

XP_END_PROTOS

#endif

