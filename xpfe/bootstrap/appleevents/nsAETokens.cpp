/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */



#include "nsAETokens.h"

AETokenDesc::AETokenDesc(const AEDesc* token)
    : mTokenValid(false)
{
    mTokenValid = (AEGetDescDataSize(token) == sizeof(CoreTokenRecord));
    if (mTokenValid)
        AEGetDescData(token, &mTokenData, sizeof(CoreTokenRecord));
}

AETokenDesc::~AETokenDesc() {}

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetDispatchClass() const
{
	return (mTokenValid ? mTokenData.dispatchClass : typeNull);
}

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetObjectClass() const
{
	return (mTokenValid ? mTokenData.objectClass : typeNull);
}

// ---------------------------------------------------------------------------
// The field usePropertyCode has been removed from the token record,
// so we emulate it here by seeing if the propertyCode field is typeNull,
// which is interpreted to mean that this is NOT a property token

Boolean AETokenDesc::UsePropertyCode() const
{
	return (mTokenValid ? mTokenData.propertyCode != typeNull : false);
}

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetPropertyCode() const
{
	return (mTokenValid ? mTokenData.propertyCode : typeNull);
}

// ---------------------------------------------------------------------------

long AETokenDesc::GetDocumentID() const
{
	return (mTokenValid ? mTokenData.documentID : typeNull);
}

// ---------------------------------------------------------------------------

WindowPtr AETokenDesc::GetWindowPtr() const
{
	return (mTokenValid ? mTokenData.window : nil);
}


// ---------------------------------------------------------------------------

TAEListIndex AETokenDesc::GetElementNumber() const
{
	return (mTokenValid ? mTokenData.elementNumber : 0);
}



#pragma mark -

// ---------------------------------------------------------------------------

void AETokenDesc::SetPropertyCode(DescType propertyCode)
{
	mTokenData.propertyCode = propertyCode;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetDispatchClass(DescType dispatchClass)
{
	mTokenData.dispatchClass = dispatchClass;
}


// ---------------------------------------------------------------------------
void AETokenDesc::SetObjectClass(DescType objectClass)
{
	mTokenData.objectClass = objectClass;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetElementNumber(TAEListIndex number)
{
	mTokenData.elementNumber = number;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetWindow(WindowPtr wind)
{
	mTokenData.window = wind;
}
