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

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetDispatchClass() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).dispatchClass : typeNull;
}

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetObjectClass() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).objectClass : typeNull;
}

// ---------------------------------------------------------------------------
// The field usePropertyCode has been removed from the token record,
// so we emulate it here by seeing if the propertyCode field is typeNull,
// which is interpreted to mean that this is NOT a property token

Boolean AETokenDesc::UsePropertyCode() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? ((**tokenData).propertyCode != typeNull) : false;
}

// ---------------------------------------------------------------------------

DescType AETokenDesc::GetPropertyCode() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).propertyCode : typeNull;
}

// ---------------------------------------------------------------------------

long AETokenDesc::GetDocumentID() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).documentID : 0;
}

// ---------------------------------------------------------------------------

WindowPtr AETokenDesc::GetWindowPtr() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).window : nil;
}


// ---------------------------------------------------------------------------

TAEListIndex AETokenDesc::GetElementNumber() const
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	return (tokenData) ? (**tokenData).elementNumber : 0;
}



#pragma mark -

// ---------------------------------------------------------------------------

void AETokenDesc::SetPropertyCode(DescType propertyCode)
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	ThrowIfNil(tokenData);
	(**tokenData).propertyCode = propertyCode;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetDispatchClass(DescType dispatchClass)
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	ThrowIfNil(tokenData);
	(**tokenData).dispatchClass = dispatchClass;
}


// ---------------------------------------------------------------------------
void AETokenDesc::SetObjectClass(DescType objectClass)
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	ThrowIfNil(tokenData);
	(**tokenData).objectClass = objectClass;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetElementNumber(TAEListIndex number)
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	ThrowIfNil(tokenData);
	(**tokenData).elementNumber = number;
}

// ---------------------------------------------------------------------------
void AETokenDesc::SetWindow(WindowPtr wind)
{
	CoreTokenHandle 	tokenData 	= GetTokenHandle();
	ThrowIfNil(tokenData);
	(**tokenData).window = wind;
}


