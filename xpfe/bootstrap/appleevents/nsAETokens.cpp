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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
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



#include "nsAETokens.h"

// ---------------------------------------------------------------------------
ConstAETokenDesc::ConstAETokenDesc(const AEDesc* token)
{
	mTokenWasNull = (token->descriptorType == typeNull);
	
	if (!mTokenWasNull)
	{
		if (::AEGetDescDataSize(token) != sizeof(CoreTokenRecord))
			ThrowOSErr(paramErr);			// invalid token

		ThrowIfOSErr(::AEGetDescData(token, &mTokenRecord, sizeof(CoreTokenRecord)));
	}
}


// ---------------------------------------------------------------------------

DescType ConstAETokenDesc::GetDispatchClass() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.dispatchClass;
}

// ---------------------------------------------------------------------------

DescType ConstAETokenDesc::GetObjectClass() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.objectClass;
}

// ---------------------------------------------------------------------------
// The field usePropertyCode has been removed from the token record,
// so we emulate it here by seeing if the propertyCode field is typeNull,
// which is interpreted to mean that this is NOT a property token

Boolean ConstAETokenDesc::UsePropertyCode() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return (mTokenRecord.propertyCode != typeNull);
}

// ---------------------------------------------------------------------------

DescType ConstAETokenDesc::GetPropertyCode() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.propertyCode;
}

// ---------------------------------------------------------------------------

long ConstAETokenDesc::GetDocumentID() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.documentID;
}

// ---------------------------------------------------------------------------

WindowPtr ConstAETokenDesc::GetWindowPtr() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.window;
}


// ---------------------------------------------------------------------------

TAEListIndex ConstAETokenDesc::GetElementNumber() const
{
	ThrowErrIfTrue(mTokenWasNull, paramErr);
	return mTokenRecord.elementNumber;
}



#pragma mark -

// ---------------------------------------------------------------------------
AETokenDesc::AETokenDesc(AEDesc* token)
:	ConstAETokenDesc(token)
,	mTokenDesc(token)
{
	if (mTokenWasNull)			// we cannot wrap a null token
		ThrowOSErr(paramErr);
}

// ---------------------------------------------------------------------------
AETokenDesc::~AETokenDesc()
{
	UpdateDesc();		// update the AEDesc that we wrap
}

// ---------------------------------------------------------------------------

void AETokenDesc::SetPropertyCode(DescType propertyCode)
{
	mTokenRecord.propertyCode = propertyCode;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetDispatchClass(DescType dispatchClass)
{
	mTokenRecord.dispatchClass = dispatchClass;
}


// ---------------------------------------------------------------------------
void AETokenDesc::SetObjectClass(DescType objectClass)
{
	mTokenRecord.objectClass = objectClass;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetElementNumber(TAEListIndex number)
{
	mTokenRecord.elementNumber = number;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetWindow(WindowPtr wind)
{
	mTokenRecord.window = wind;
}

// ---------------------------------------------------------------------------
void AETokenDesc::UpdateDesc()
{
	OSErr	err = ::AEReplaceDescData(mTokenDesc->descriptorType, &mTokenRecord, sizeof(CoreTokenRecord), mTokenDesc);
	ThrowIfOSErr(err);
}
