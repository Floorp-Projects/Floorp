/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _CHECKBOXLDEF_H_
#define _CHECKBOXLDEF_H_


#ifdef __cplusplus
extern "C" {
#endif

pascal void main(SInt16 message,Boolean selected,Rect *cellRect,Cell theCell,
	 			 SInt16 dataOffset,SInt16 dataLen,ListHandle theList);
void  		Draw(Boolean selected,Rect *cellRect,Cell theCell,SInt16 dataLen,
		   		 ListHandle theList);
void 		Highlight(Rect *cellRect, Boolean selected);
Rect		DrawEmptyCheckbox(Rect *cellRect);
void		DrawCheckedCheckbox(Rect *cellRect);

#ifdef __cplusplus
}
#endif


#endif