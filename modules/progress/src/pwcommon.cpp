/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

	newMWContext->type = MWContextProgressModule;
	XP_AddContextToList(newMWContext);
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

    newMWContext->prInfo = (PrintInfo *)p_context;  /* Hackily overloading a part of MWContext */
	return newMWContext;
}

void PW_DestroyProgressContext(MWContext * context)
{
	if (context)
	{
		XP_RemoveContextFromList(context);
		pw_ptr pw = ((pw_environment *)(context->prInfo))->progressWindow;
		XP_FREEIF( context->funcs);
		XP_FREEIF(context);
	}
}

void PW_AssociateWindowWithContext(MWContext * context, pw_ptr pw)
{
	pw_environment * e = (pw_environment *)(context->prInfo);
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

pw_ptr PW_GetAssociatedWindowForContext(MWContext *context)
{
	pw_environment * e = (pw_environment *)(context->prInfo);

	if (context->type != MWContextProgressModule)
	{
		XP_ASSERT(FALSE);
		return NULL;
	}

	return e->progressWindow;
}

#ifdef XP_MAC
#pragma export off
#endif


void pw_Progress(MWContext * cx, const char *msg)
{
	pw_ptr pw = ((pw_environment *)(cx->prInfo))->progressWindow;
	if (pw)
		PW_SetLine2(pw, msg);
}

void pw_Alert(MWContext * /*cx*/, const char *msg)
{
	FE_Alert(NULL, msg);
}

void pw_GraphProgressInit(MWContext *context, URL_Struct* /*URL_s*/, int32 content_length)
{
	pw_environment * pe = (pw_environment *)context->prInfo;
	
	pe->outstandingURLs += 1;

	if ( content_length > 0 )
		pe->total_bytes += content_length;
	else
		pe->hasUnknownSizeURLs = TRUE;
	
	if (pe->outstandingURLs == 1)	/* First URL got started, set the start time */
		pe->start_time_secs = XP_TIME();

	if ( pe->progressWindow )
	{
		PW_SetProgressRange( pe->progressWindow, 0, pe->total_bytes);
	}			
}

void pw_GraphProgressDestroy(MWContext *context, URL_Struct* /*URL_s*/, int32 /*content_length*/, int32 /*total_bytes_read*/)
{
	pw_environment * pe = (pw_environment *)context->prInfo;
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

void pw_GraphProgress(MWContext *context, URL_Struct* /*URL_s*/, int32 /*bytes_received*/, int32 bytes_since_last_time, int32 content_length)
{
	pw_environment * pe = (pw_environment *)context->prInfo;
	const char * progressText;

	pe->bytes_received += bytes_since_last_time;
	if (pe->progressWindow)
	{
		// should document the use of content length, may need another for li verify.
		if (content_length == -2) {
			char * output = NULL;
			output = (char*) XP_ALLOC( 300 );
			if ( !output ) {
				progressText= NULL;
				PW_SetLine2( pe->progressWindow, progressText);
			} else {
				// REMIND  put the text in the xp strings bucket for l10n
				sprintf(output, "Synchronizing item %d of %d.", pe->bytes_received,pe->total_bytes); 
				PW_SetLine2( pe->progressWindow, output);
				if (pe->total_bytes > 0) {
					sprintf(output, "%d%%.", (pe->bytes_received * 100) / pe->total_bytes); 
					PW_SetProgressText( pe->progressWindow, output);
				}
				XP_FREE(output);
			}
		}
		else {
			progressText = XP_ProgressText( pe->hasUnknownSizeURLs ? 0 : pe->total_bytes, 
									pe->bytes_received, 
									pe->start_time_secs,
									XP_TIME());
			PW_SetProgressText( pe->progressWindow, progressText);
		}

		PW_SetProgressValue( pe->progressWindow, pe->bytes_received );
	}
}

void pw_SetProgressBarPercent(MWContext *context, int32 percent)
{
	pw_environment * pe = (pw_environment *)context->prInfo;
	if (pe->progressWindow)
	{
		PW_SetProgressValue( pe->progressWindow, percent);
	}
}

XP_Bool pw_Confirm(MWContext* context, const char* msg)
{
//#if defined(XP_MAC) || defined(XP_UNIX)
//	return XP_Confirm( context, msg );
//#else
	XP_ASSERT(FALSE);
	return FALSE;
//#endif
}

char* pw_Prompt(MWContext * /*context*/, const char * Msg, const char * dflt)
{
//#if defined(XP_MAC)
//	return XP_Prompt(NULL, Msg, dflt);
//#else
	XP_ASSERT(FALSE);
	return NULL;
//#endif
}

char* pw_PromptWithCaption(MWContext * /*context */, const char * /* caption */, const char * /*Msg*/, const char * /*dflt*/)
{
	XP_ASSERT(FALSE);
	return NULL;	// FE_PromptWithCaption(NULL, caption, Msg, dflt);
}

XP_Bool pw_PromptUsernameAndPassword(MWContext * c,const char * prompt,char ** username, char ** password)
{
//#if defined(XP_MAC) || defined(XP_UNIX)
//	return XP_PromptUsernameAndPassword(c, prompt, username, password);
//#else
	XP_ASSERT(FALSE);
	return FALSE;
//#endif
}

char * pw_PromptPassword(MWContext *context, const char * Msg)
{
//#if defined(XP_MAC) || defined(XP_UNIX)
//	return XP_PromptPassword(context, Msg);
//#else
	XP_ASSERT(FALSE);
	return NULL;
//#endif
}
void pw_EnableClicking(MWContext * /*context*/)
{
}

void pw_AllConnectionsComplete(MWContext * /*context*/)
{
}

