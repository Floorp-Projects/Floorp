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
/* pw_win.cpp
 * Windows implementation of progress interface
 */

#ifdef _WINDOWS
#include "tooltip.h"
#include <afxwin.h>
#include "resource.h"
#include "pw_public.h"
#include "pw_win.h"
#include "winli.h"
#include "cast.h"

pw_ptr PW_Create( MWContext * parent, 		/* Parent window, can be NULL */
				PW_WindowType type			/* What kind of window ? Modality, etc */
				)
{
	CXPProgressDialog * dlg;	
		   
	dlg = new CXPProgressDialog();

#ifndef XP_WIN16
	/* REMIND: need to fix for win16 */
	if (dlg) dlg->Create(IDD_XPPROGRESS,NULL);
#endif

	return dlg;
}

void PW_SetCancelCallback(pw_ptr pw,
							PW_CancelCallback cancelcb,
							void * cancelClosure)
{}


void PW_Show(pw_ptr pw)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->ShowWindow(SW_SHOW);
	}
}

void PW_Hide(pw_ptr pw)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->ShowWindow(SW_HIDE);
	}
}

void PW_Destroy(pw_ptr pw)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->DestroyWindow();
	}
}
 
void PW_SetWindowTitle(pw_ptr pw, const char * title)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->SetWindowText(title);
	}
}

void PW_SetLine1(pw_ptr pw, const char * text)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->GetDlgItem(IDC_LINE1)->SetWindowText(text);
	}

}

void PW_SetLine2(pw_ptr pw, const char * text)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->GetDlgItem(IDC_LINE2)->SetWindowText(text);
	}
}

void PW_SetProgressText(pw_ptr pw, const char * text)
{
	CXPProgressDialog * dlg;

	if (pw) dlg = (CXPProgressDialog *)pw;
}

void PW_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

		dlg->m_Min = minimum;
		dlg->m_Max = maximum;
		dlg->m_Range = 100;
	}
}

void PW_SetProgressValue(pw_ptr pw, int32 value)
{
	CXPProgressDialog * dlg;

	if (pw) {
		dlg = (CXPProgressDialog *)pw;

//		dlg->m_ProgressMeter.StepItTo(CASTINT((value*100)/dlg->m_Range));

//		char szPercent[10];
//		PR_snprintf(szPercent,sizeof(szPercent),"%ld%%",(value*100)/dlg->m_Range);
//		dlg->m_PercentComplete.SetWindowText(szPercent);
	}
}

#endif 

