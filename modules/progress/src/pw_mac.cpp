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

/* pw_mac.cpp
 * author: atotic
 * Mac implementation of progress interface
 */

#define OLDROUTINENAMES 0		// Why is this here?
#include <PP_ClassHeaders.cp>	// pchen: put <PP_ClassHeaders.cp> ahead of pw_public.h
								// to avoid TRUE and FALSE redefine error
#include "pw_public.h"
#include <TextEdit.h>
#include "uapp.h"
#include "PP_Messages.h"
#include "CPatternProgressBar.h"
#include "UDesktop.h"

#define kProgressStandardWindowID	5050
#define kProgressModalWindowID		5051

class CProgressMac: public LListener {

public:
	LWindow 		*fWindow;
	LCaption 		*fLine1, *fLine2, *fLine3;
	CPatternProgressBar * fProgress;
	
	PW_CancelCallback fCancelcb;
	void * fCancelClosure;
	
// Constructors
	CProgressMac(PW_WindowType type);

	virtual ~CProgressMac();
	
// PW interface
	void SetCancelCallback(PW_CancelCallback cancelcb,	/* Callback function for cancel */
				void * cancelClosure);

	void Show();
	
	void Hide();
	
	void SetWindowTitle(const char * title);
	
	void SetLine1(const char * text);

	void SetLine2(const char * text);
	
	void SetProgressText(const char * text);
	
	void SetProgressRange(int32 minimum, int32 maximum);
	
	void SetProgressValue(int32 value);
	
	void ListenToMessage( MessageT inMessage, void* ioParam );
};

CProgressMac::CProgressMac(PW_WindowType type)
{
	ResIDT		windowID;
	
	fCancelcb = NULL;
	fCancelClosure = NULL;
		
	switch (type)
	{
		case pwApplicationModal:
			windowID = kProgressModalWindowID;
			break;
		case pwStandard:
			windowID = kProgressStandardWindowID;
			break;
		default:
			XP_ASSERT(false);		// invalid window type
	}
	
	fWindow = LWindow::CreateWindow( windowID, (type == pwApplicationModal) ? NULL : LCommander::GetTopCommander());
	ThrowIfNil_(fWindow);
	
	fLine1 = (LCaption*)fWindow->FindPaneByID('LIN1');
	fLine2 = (LCaption*)fWindow->FindPaneByID('LIN2');
	fLine3 = (LCaption*)fWindow->FindPaneByID('LIN3');
	fProgress = (CPatternProgressBar *)fWindow->FindPaneByID('PtPb');
	ThrowIfNil_(fLine1);
	ThrowIfNil_(fLine2);
	ThrowIfNil_(fLine3);
	ThrowIfNil_(fProgress);
}

CProgressMac::~CProgressMac()
{
	if (fWindow)
		delete fWindow;
}

void CProgressMac::SetCancelCallback(PW_CancelCallback incancelcb,	/* Callback function for cancel */
				void * incancelClosure)
{
	fCancelcb = incancelcb;
	fCancelClosure = incancelClosure;
	
	LButton * cancelButton = (LButton *)fWindow->FindPaneByID('CNCL');
	cancelButton->AddListener(this);
}

void CProgressMac::ListenToMessage( MessageT inMessage, void* ioParam )
{
#pragma unused (ioParam)
	if ( inMessage == msg_Cancel)
		if (fCancelcb)
			fCancelcb(fCancelClosure);
}

void CProgressMac::Show()
{
	fWindow->Show();
	UDesktop::SelectDeskWindow(fWindow);
}
	
void CProgressMac::Hide()
{
	fWindow->Hide();
}
	
void CProgressMac::SetWindowTitle(const char * title)
{
	LStr255 ptitle;
	if (title)
		ptitle = title; 
	fWindow->SetDescriptor(ptitle);
}

void CProgressMac::SetLine1(const char * text)
{
	LStr255 ptext;
	if (text)
		ptext = text;
	fLine1->SetDescriptor(ptext);
}

void CProgressMac::SetLine2(const char * text)
{
	LStr255 ptext;
	if (text)
		ptext = text;
	fLine2->SetDescriptor(ptext);
}

void CProgressMac::SetProgressText(const char * text)
{
	LStr255 ptext;
	if (text)
		ptext = text;
	fLine3->SetDescriptor(ptext);
}

void CProgressMac::SetProgressRange(int32 minimum, int32 maximum)
{
	if (maximum > minimum)
	{
		Assert_(minimum==0);
		//Range32T	r(minimum, maximum);
		//fProgress->SetValueRange(r);
		fProgress->SetValue(minimum);
		fProgress->SetValueRange(maximum - minimum);
	}
	else
	{
		fProgress->SetToIndefinite();
	}
}

void CProgressMac::SetProgressValue(int32 value)
{
	fProgress->SetValue(value);
}


#pragma export on

pw_ptr PW_Create( MWContext * /*parent*/, 		/* Parent window, can be NULL */
				PW_WindowType type			/* What kind of window ? Modality, etc */
				)
{
	/*volatile*/ CProgressMac * mac = NULL;
	
	try
	{
		mac = new CProgressMac(type);
	}
	catch(OSErr err)
	{
		XP_ASSERT(FALSE);
	}
	if (mac)
	{ // Clean up the progress text
		PW_SetWindowTitle(mac, NULL);
		PW_SetLine1(mac, NULL);
		PW_SetLine2(mac, NULL);
	}
	return mac;
}

void PW_SetCancelCallback(pw_ptr pw,
							PW_CancelCallback cancelcb,
							void * cancelClosure)
{
	if (pw)
		((CProgressMac *)pw)->SetCancelCallback(cancelcb, cancelClosure);
}


void PW_Show(pw_ptr pw)
{
	if (pw)
		((CProgressMac *)pw)->Show();
}

void PW_Hide(pw_ptr pw)
{
	if (pw)
		((CProgressMac *)pw)->Hide();
}

void PW_Destroy(pw_ptr pw)
{
	if (pw)
		delete (CProgressMac*)pw;
}
 
void PW_SetWindowTitle(pw_ptr pw, const char * title)
{
	if (pw)
		((CProgressMac *)pw)->SetWindowTitle(title);
}

void PW_SetLine1(pw_ptr pw, const char * text)
{
	if (pw)
		((CProgressMac *)pw)->SetLine1(text);
}

void PW_SetLine2(pw_ptr pw, const char * text)
{
	if (pw)
		((CProgressMac *)pw)->SetLine2(text);
}

void PW_SetProgressText(pw_ptr pw, const char * text)
{
	if (pw)
		((CProgressMac *)pw)->SetProgressText(text);
}

void PW_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum)
{
	if (pw)
		((CProgressMac *)pw)->SetProgressRange(minimum, maximum);			
}

void PW_SetProgressValue(pw_ptr pw, int32 value)
{
	if (pw)
		((CProgressMac *)pw)->SetProgressValue(value);			
}

#pragma export off

