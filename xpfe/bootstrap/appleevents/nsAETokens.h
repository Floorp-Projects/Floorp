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


#ifndef __AETOKENS__
#define __AETOKENS__

#include "nsAEUtils.h"


// ----------------------------------------------------------------------------
// This is the token record used for all tokens in the CORE Suite
// This uses the "kitchen sink" token metaphor: a single token is used to represent
// all objects, and some fields are only appropriate for some objects.

struct CoreTokenRecord
{
	DescType				dispatchClass;		// the class that will dispatch this class
	DescType				objectClass;		// the actual class of the tokenized object
	DescType				propertyCode;		// property code, or typeNull if not a property token
	long					documentID;
	TAEListIndex			elementNumber;
	WindowPtr			window;			// only used for window objects
	
	CoreTokenRecord()
	:	dispatchClass(typeNull)
	,	objectClass(typeNull)
	,	propertyCode(typeNull)
	,	documentID(0)
	,	elementNumber(0)
	,	window(nil)
	{
	}
};


typedef struct CoreTokenRecord CoreTokenRecord, *CoreTokenPtr, **CoreTokenHandle;


// AETokenDesc
// A utility class designed to give easy access to the contents of the token

class AETokenDesc
{
public:
						AETokenDesc(const AEDesc* token);
						~AETokenDesc();

	DescType			GetDispatchClass() const;
	DescType 			GetObjectClass() const;
	Boolean				UsePropertyCode() const;
	DescType			GetPropertyCode() const;
		
	long				GetDocumentID() const;
	WindowPtr			GetWindowPtr() const;
	TAEListIndex		GetElementNumber() const;
	
	void				SetDispatchClass(DescType dispatchClass);
	void				SetObjectClass(DescType objectClass);
	void				SetPropertyCode(DescType propertyCode);
	void				SetElementNumber(TAEListIndex number);
	void				SetWindow(WindowPtr wind);
	
protected:
	CoreTokenRecord     mTokenData;
	Boolean             mTokenValid;
};

#endif /* __AETOKENS__ */
