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

#ifndef _JULIANFORM_H
#define _JULIANFORM_H

#include "jdefines.h"
#include "netcburl.h"
#include "fe_proto.h"

XP_BEGIN_PROTOS

typedef	struct Julian_Form_Callback_Struct
{
	/*
	** callbackurl should be set to NET_CallbackURLCreate(), it's
	** in netcburl.h. Can also be set to nil.
	*/
	char*	(*callbackurl)(NET_CallbackURLFunc func, void* closure);

	/*
	** callbackurlfree should be set to NET_CallbackURLFree(), it's
	** in netcburl.h. Can also be set to nil.
	*/
	int		(*callbackurlfree)(NET_CallbackURLFunc func, void* closure);

	/*
	** Should Link to NET_ParseURL()
	*/
	char*	(*ParseURL)(const char *url, int wanted);

	/*
	** Should Link to FE_MakeNewWindow()
	*/
	MWContext*	(*MakeNewWindow)(MWContext *old_context, URL_Struct *url, char *window_name, Chrome *chrome);

	/*
	** Should Link to NET_CreateURLStruct ();
	*/
	URL_Struct* (*CreateURLStruct) (const char *url, NET_ReloadMethod force_reload);

	/*
	** Should Link to NET_StreamBuilder()
	*/
	NET_StreamClass* (*StreamBuilder) (FO_Present_Types format_out,URL_Struct *anchor, MWContext *window_id);

	/*
	** Should Link to NET_SACopy()
	*/
	char* (*SACopy) (char **dest, const char *src);

    /*
    ** Should Link to NET_SendMessageUnattended().  Added by John Sun 4-22-98.
    */
    int (*SendMessageUnattended) (MWContext* context, char* to, char* subject, char* otherheaders, char* body);
    
    /*
    ** Should Link to FE_DestroyWindow.  Added by John Sun 4-22-98.
    */
    void (*DestroyWindow) (MWContext* context);
 
	/*
    ** Should Link to FE_RaiseWindow.
    */
    void (*RaiseWindow) (MWContext* context);

	/*
    ** Should Link to Current MWContext.
    */
    MWContext* my_context;

    /*
    ** Should Link to XP_GetString.
    */
    char* (*GetString) (int i);

	/*
	** Should Link to XP_FindSomeContext()
	*/
	MWContext*	(*FindSomeContext)();

	/*
	** Should Link to XP_FindNamedContextInList()
	*/
	MWContext*	(*FindNamedContextInList)(MWContext* context, char *name);

	/*
	** Should Link to PREF_CopyCharPref()
	*/
	MWContext*	(*CopyCharPref)(const char *pref, char ** return_buf);

	/*
	** Should Link to NET_UnEscape()
	*/
	char*		(*UnEscape)(char *str);

	/*
	** Should Link to NET_PlusToSpace()
	*/
	void		(*PlusToSpace)(char *str);

} Julian_Form_Callback_Struct, *pJulian_Form_Callback_Struct;

/*
** Caller disposes of callbacks.
*/
void JULIAN_PUBLIC jf_Initialize(pJulian_Form_Callback_Struct callbacks);

void JULIAN_PUBLIC *jf_New(char *calendar_mime_data);
void JULIAN_PUBLIC jf_Destroy(void *instdata);
void JULIAN_PUBLIC jf_Shutdown(void);
char JULIAN_PUBLIC *jf_getForm(void *instdata);
void JULIAN_PUBLIC jf_setDetail(int detail_form);
void JULIAN_PUBLIC jf_callback(void *instdata, char* url);
void JULIAN_PUBLIC jf_detail_callback(void *instdata, char *url);

XP_END_PROTOS

#endif
