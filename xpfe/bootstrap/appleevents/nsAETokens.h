/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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



// ConstAETokenDesc
// This is a read-only wrapper for an AEDesc* that can be used
// to read the contents of the token record.

class ConstAETokenDesc
{
public:
						ConstAETokenDesc(const AEDesc* token);


	DescType			GetDispatchClass() const;
	DescType 			GetObjectClass() const;
	Boolean				UsePropertyCode() const;
	DescType			GetPropertyCode() const;
		
	long				GetDocumentID() const;
	WindowPtr			GetWindowPtr() const;
	TAEListIndex		GetElementNumber() const;

protected:

	CoreTokenRecord     mTokenRecord;
	Boolean				mTokenWasNull;		// true if we were passed an empty AEDesc
};



// AETokenDesc
// A read-write wrapper for an AEDesc*. Use this if you want to
// update the contents of the AEDesc's data handle

class AETokenDesc : public ConstAETokenDesc
{
public:
						AETokenDesc(AEDesc* token);
						~AETokenDesc();
	
	void				SetDispatchClass(DescType dispatchClass);
	void				SetObjectClass(DescType objectClass);
	void				SetPropertyCode(DescType propertyCode);
	void				SetElementNumber(TAEListIndex number);
	void				SetWindow(WindowPtr wind);

	void				UpdateDesc();			// update the AEDesc wrapped by this class

	CoreTokenRecord&	GetTokenRecord()	{ return mTokenRecord; }
	
protected:
	
	AEDesc*				mTokenDesc;
	
};

#endif /* __AETOKENS__ */
