/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextHelper.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStringUtil.h"
#include <TextView.h>

NS_METHOD nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsTextWidgetInitData* data = (nsTextWidgetInitData *) aInitData;
    mIsPassword = data->mIsPassword;
    mIsReadOnly = data->mIsReadOnly;
  }
  return NS_OK;
}

NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->SetMaxBytes(aChars);
		mTextView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize)
{
	if(mTextView && mTextView->LockLooper())
	{
		aTextBuffer.SetLength(0);
		aTextBuffer.AppendWithConversion(mTextView->Text());
		aActualSize = strlen(mTextView->Text());
		mTextView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD  nsTextHelper::SetText(const nsString &aText, PRUint32& aActualSize)
{ 
	mText = aText;
	
	const char *text;
	text = ToNewCString(aText);
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->SetText(text);
		mTextView->UnlockLooper();
	}
	delete [] text;
	
	aActualSize = aText.Length();
	return NS_OK;
}

NS_METHOD  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize)
{
	const char *text;
	text = ToNewCString(aText);
	if(mTextView)
	{
		if(mTextView->LockLooper())
		{
			mTextView->Insert(aStartPos, text, aActualSize);
			mTextView->UnlockLooper();
		}
		else
			mTextView->Insert(aStartPos, text, aActualSize);
	}
	delete [] text;
	mText.Insert(ToNewUnicode(aText), aStartPos, aText.Length());
	return NS_OK;
}

NS_METHOD  nsTextHelper::RemoveText()
{
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->Delete(0, mTextView->TextLength() - 1);
		mTextView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
if(mIsPassword) printf("nsTextHelper::SetPassword not implemented\n");
  return NS_OK;
}

NS_METHOD nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
	aOldFlag = mIsReadOnly;
	mIsReadOnly = aReadOnlyFlag;

	// Update the widget
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->MakeEditable(false);
		mTextView->UnlockLooper();
	}

	return NS_OK;
}
  
NS_METHOD nsTextHelper::SelectAll()
{
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->SelectAll();
		mTextView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->Select(aStartSel, aEndSel);
		mTextView->UnlockLooper();
	}
	return NS_OK;
}


NS_METHOD  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
	if(mTextView && mTextView->LockLooper())
	{
		mTextView->GetSelection((int32 *)aStartSel, (int32 *)aEndSel);
		mTextView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  SetSelection(aPosition, aPosition);
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetCaretPosition(PRUint32& aPos)
{
  PRUint32 start;
  PRUint32 end;
  GetSelection(&start, &end);
  if (start == end) {
    aPos = start;
  }
  else {
    aPos =  PRUint32(-1);/* XXX is this right??? scary cast! */
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper() : nsWindow(), nsITextWidget()
{
  mIsReadOnly = PR_FALSE;
  mIsPassword = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsTextHelper destructor
//
//-------------------------------------------------------------------------
nsTextHelper::~nsTextHelper()
{
}

//-------------------------------------------------------------------------
//
// Clear window before paint
//
//-------------------------------------------------------------------------

PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}


TextFrameBeOS::TextFrameBeOS(BTextView *child, BRect frame, const char *name, uint32 resizingmode, uint32 flags)
 : BView(frame, name, resizingmode, flags | B_FRAME_EVENTS), tv(child)
{
	SetViewColor(0, 0, 0);
	AddChild(tv);
}

void TextFrameBeOS::FrameResized(float width, float height)
{
	tv->MoveTo(1, 1);
	tv->ResizeTo(width - 2, height - 2);
	BRect r = Bounds();
	r.InsetBy(1, 1);
	tv->SetTextRect(r);
	BView::FrameResized(width, height);
}
