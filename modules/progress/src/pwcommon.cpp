/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/* pwcommon.cpp
 * cross-platform progress routines
 */
#include "pw_public.h"
#include "ntypes.h"
#include "structs.h"
#include "proto.h"
#include "xp_mem.h"
#include "fe_proto.h"
#include "ctxtfunc.h"
#include "xp_thrmo.h"

void pw_Progress(MWContext * cx, const char *msg);
void pw_Alert(MWContext * cx, const char *msg);
void pw_GraphProgressInit(MWContext *context, URL_Struct *URL_s, int32 content_length);
void pw_GraphProgressDestroy(MWContext *context, URL_Struct *URL_s, int32 content_length, int32 total_bytes_read);
void pw_GraphProgress(MWContext *context, URL_Struct *URL_s, int32 bytes_received, int32 bytes_since_last_time, int32 content_length);
XP_Bool pw_Confirm(MWContext * context, const char * Msg);
char* pw_Prompt(MWContext * context, const char * Msg, const char * dflt);
char* pw_PromptWithCaption(MWContext * context, const char *caption, const char * Msg, const char * dflt);
XP_Bool pw_PromptUsernameAndPassword(MWContext *,const char *,char **, char **);
char * pw_PromptPassword(MWContext * context, const char * Msg);
void pw_EnableClicking(MWContext * context);
void pw_AllConnectionsComplete(MWContext * context);
void pw_SetProgressBarPercent(MWContext *context, int32 percent);
void pw_SetCallNetlibAllTheTime(MWContext *context);
void pw_ClearCallNetlibAllTheTime(MWContext *context);

/* This struct encapsulates the data we need to store in our progress context */

struct pw_environment_	{
	pw_ptr progressWindow;
	/* data for progress total calculations */
	int outstandingURLs;	/* How many outstanding URLs do we have */
	XP_Bool hasUnknownSizeURLs;	/* How many outstanding URLs have unknown size */
	unsigned long total_bytes;
	unsigned long bytes_received;
	unsigned long start_time_secs;
} ;

typedef struct pw_environment_ pw_environment;

#ifdef XP_MAC
#pragma export on
#endif

MWContext * PW_CreateProgressContext()
{
	/* After the merge, it'll be XP_NewCntxt */
	
	pw_environment * p_context;
	MWContext * newMWContext;

	p_context = (pw_environment *)XP_CALLOC(sizeof(pw_environment), 1);
	if (p_context == NULL)
		return NULL;

 	/* initialize the new MWContext */
 
	newMWContext = (MWContext *) XP_CALLOC(sizeof( MWContext ), 1);
	if (newMWContext == NULL)
		return NULL;

//	XP_AddContextToList(newMWContext);
	newMWContext->type = MWContextProgressModule;
	XP_InitializeContext(newMWContext);
// Assign all the functions
	newMWContext->funcs = (ContextFuncs*) XP_ALLOC(sizeof(ContextFuncs));
	if (newMWContext->funcs == NULL )
		return NULL;

	newMWContext->funcs->Progress = pw_Progress;
	newMWContext->funcs->Alert = pw_Alert;
	newMWContext->funcs->GraphProgressInit = pw_GraphProgressInit;
	newMWContext->funcs->GraphProgressDestroy = pw_GraphProgressDestroy;
	newMWContext->funcs->GraphProgress = pw_GraphProgress;
	newMWContext->funcs->Confirm = pw_Confirm;
	newMWContext->funcs->Prompt = pw_Prompt;
	newMWContext->funcs->PromptWithCaption = pw_PromptWithCaption;
	newMWContext->funcs->PromptUsernameAndPassword = pw_PromptUsernameAndPassword;
	newMWContext->funcs->PromptPassword = pw_PromptPassword;
	newMWContext->funcs->PromptPassword = pw_PromptPassword;
	newMWContext->funcs->EnableClicking = pw_EnableClicking;
	newMWContext->funcs->AllConnectionsComplete = pw_AllConnectionsComplete;
	newMWContext->funcs->SetProgressBarPercent = pw_SetProgressBarPercent;
	newMWContext->funcs->SetCallNetlibAllTheTime = pw_SetCallNetlibAllTheTime;
	newMWContext->funcs->ClearCallNetlibAllTheTime = pw_ClearCallNetlibAllTheTime;


        newMWContext->mime_data = (struct MimeDisplayData *)p_context;  /* Hackily overloading a part of MWContext */
	return newMWContext;
}

void PW_DestroyProgressContext(MWContext * context)
{
	if (context)
	{
//		XP_RemoveContextFromList(context);
		pw_ptr pw = ((pw_environment *)(context->mime_data))->progressWindow;
		XP_FREEIF( context->funcs);
		XP_FREEIF(context);
	}
}

void PW_AssociateWindowWithContext(MWContext * context, pw_ptr pw)
{
	pw_environment * e = (pw_environment *)(context->mime_data);
	XP_Bool doReset;

	if (context->type != MWContextProgressModule)
	{
		XP_ASSERT(FALSE);
		return;
	}
	doReset = (e->progressWindow != pw);
	e->progressWindow = pw;
	if (doReset && 
		(pw != NULL))
	{
		PW_SetProgressText( pw, NULL);
	}
}

#ifdef XP_MAC
#pragma export off
#endif


void pw_Progress(MWContext * cx, const char *msg)
{
	pw_ptr pw = ((pw_environment *)(cx->mime_data))->progressWindow;
	if (pw)
		PW_SetLine2(pw, msg);
}

void pw_Alert(MWContext * /*cx*/, const char *msg)
{
	FE_Alert(NULL, msg);
}

void pw_GraphProgressInit(MWContext *context, URL_Struct* /*URL_s*/, int32 content_length)
{
	pw_environment * pe = (pw_environment *)context->mime_data;
	
	pe->outstandingURLs += 1;

	if ( content_length > 0 )
		pe->total_bytes += content_length;
	else
		pe->hasUnknownSizeURLs = TRUE;
	
	if (pe->outstandingURLs == 1)	/* First URL got started, set the start time */
		pe->start_time_secs = XP_TIME();
}

void pw_GraphProgressDestroy(MWContext *context, URL_Struct* /*URL_s*/, int32 /*content_length*/, int32 /*total_bytes_read*/)
{
	pw_environment * pe = (pw_environment *)context->mime_data;
	pe->outstandingURLs -= 1;
		
	if ( pe->outstandingURLs == 0)
	{
		if ( pe->progressWindow )
		{
			PW_SetProgressText( pe->progressWindow, NULL);
			PW_SetProgressRange( pe->progressWindow, 0, 0);
			PW_SetProgressValue( pe->progressWindow, 0 );
		}			
		pe->bytes_received = 0;
		pe->total_bytes = 0;
		pe->start_time_secs = 0;
		pe->hasUnknownSizeURLs = FALSE; 
	}
}

void pw_GraphProgress(MWContext *context, URL_Struct* /*URL_s*/, int32 /*bytes_received*/, int32 bytes_since_last_time, int32 /*content_length*/)
{
	pw_environment * pe = (pw_environment *)context->mime_data;
	const char * progressText;

	pe->bytes_received += bytes_since_last_time;
	if (pe->progressWindow)
	{
		progressText = XP_ProgressText( pe->hasUnknownSizeURLs ? 0 : pe->total_bytes, 
									pe->bytes_received, 
									pe->start_time_secs,
									XP_TIME());
		PW_SetProgressText( pe->progressWindow, progressText);
	}
}

void pw_SetProgressBarPercent(MWContext *context, int32 percent)
{
	pw_environment * pe = (pw_environment *)context->mime_data;
	if (pe->progressWindow)
	{
		PW_SetProgressValue( pe->progressWindow, percent);
	}
}

XP_Bool pw_Confirm(MWContext* /*context*/, const char* /*Msg*/)
{
	XP_ASSERT(FALSE);
	return FALSE;
//	return FE_Confirm(NULL, Msg);
}

char* pw_Prompt(MWContext * /*context*/, const char * /*Msg*/, const char * /*dflt*/)
{
	XP_ASSERT(FALSE);
	return NULL;
//	return FE_Prompt(NULL, Msg, dflt);
}

char* pw_PromptWithCaption(MWContext * /*context */, const char * /* caption */, const char * /*Msg*/, const char * /*dflt*/)
{
	return NULL;	// FE_PromptWithCaption(NULL, caption, Msg, dflt);
}

XP_Bool pw_PromptUsernameAndPassword(MWContext *,const char * /* prompt */,char ** /* username */ , char ** /* password */)
{
	//return FE_PromptUsernameAndPassword(NULL, prompt, username, password);
	return FALSE;
}

char * pw_PromptPassword(MWContext * /*context*/, const char * /*Msg*/)
{
	// return FE_PromptPassword(NULL, Msg);
	return NULL;
}

void pw_EnableClicking(MWContext * /*context*/)
{
}

void pw_AllConnectionsComplete(MWContext * /*context*/)
{
}

void pw_SetCallNetlibAllTheTime(MWContext * /*context*/)
{
	XP_ASSERT(FALSE);
}

void pw_ClearCallNetlibAllTheTime(MWContext * /*context*/)
{
	XP_ASSERT(FALSE);
}

