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

#include <xp_mcom.h>
#include "jdefines.h"
#include "julianform.h"

extern void* jform_CreateNewForm(char *, pJulian_Form_Callback_Struct);
extern void  jform_DeleteForm(void *);
extern char* jform_GetForm(void *);
extern void  jform_CallBack(void *, void *);

pJulian_Form_Callback_Struct JulianForm_CallBacks;
XP_Bool bCallbacksSet = PR_FALSE ;

void jf_Initialize(pJulian_Form_Callback_Struct callbacks)
{
	JulianForm_CallBacks = XP_NEW_ZAP(Julian_Form_Callback_Struct);
	if (JulianForm_CallBacks && callbacks)
	{
		*JulianForm_CallBacks = *callbacks;
        bCallbacksSet = PR_TRUE ;
	}
}

void *jf_New(char *calendar_mime_data)
{
	return jform_CreateNewForm(calendar_mime_data, JulianForm_CallBacks);
}

void jf_Destroy(void *instdata)
{
}

void jf_Shutdown()
{
	XP_FREEIF(JulianForm_CallBacks);
}

char *jf_getForm(void *instdata)
{
	return jform_GetForm(instdata);
}

void jf_setDetail(int detail_form)
{
}

void jf_callback(void *instdata, char *url)
{
	if (instdata && url)
	{
		jform_CallBack(instdata, url);
	}
}

/* For some unknown reason there can only be one call back funtion/
** instdate per button
*/
void jf_detail_callback(void *instdata, char *url)
{
	if (instdata && url)
	{
		jform_CallBack(instdata, url);
	}
}
