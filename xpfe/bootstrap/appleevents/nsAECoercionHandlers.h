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


#ifndef AECoercionHandlers_h_
#define AECoercionHandlers_h_

#ifdef __cplusplus

class AECoercionHandlers
{
public:
			
						AECoercionHandlers();
						~AECoercionHandlers();
				
	enum {
		typePascalString = 'PStr'		/* Descriptor type for a pascal string */
	};


protected:

	static pascal OSErr TextToPascalStringCoercion(const AEDesc *fromDesc, DescType toType, long handlerRefcon, AEDesc *toDesc);
	static pascal OSErr PascalStringToTextCoercion(const AEDesc *fromDesc, DescType toType, long handlerRefcon, AEDesc *toDesc);
	
	
protected:

	AECoerceDescUPP		mTextDescToPascalString;
	AECoerceDescUPP		mPascalStringDescToText;

public:

	static AECoercionHandlers*		GetAECoercionHandlers() { return sAECoercionHandlers; }
	static AECoercionHandlers*		sAECoercionHandlers;

};

#endif	//__cplusplus

#ifdef __cplusplus
extern "C" {
#endif

OSErr CreateCoercionHandlers();
OSErr ShutdownCoercionHandlers();

#ifdef __cplusplus
}
#endif


#endif // AECoercionHandlers_h_
