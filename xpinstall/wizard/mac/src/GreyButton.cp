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
 *     Michael C. Amorose
 */

#include "MacInstallWizard.h"


/*---------------------------------------------------------------------*
 *	Routines for drawing standard AGA pushbutton.
 *	Loosely based on code by Zig Zichterman.
 *---------------------------------------------------------------------*/


pascal void FrameGreyButton( Rect *buttonFrame )
{
	InsetRect( buttonFrame, 1, 1 );

	PenNormal();
	PenSize( 1, 1);
	SetFrameGreyButtonColor(0x0000);
	MoveTo( buttonFrame->left	+ 0, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->top		+ 0 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 0 );
	LineTo( buttonFrame->right	- 1, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 1, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->right	- 4, buttonFrame->bottom	- 1 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 1 );
	LineTo( buttonFrame->left	+ 0, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 0, buttonFrame->top		+ 3 );
	SetFrameGreyButtonColor( 0x2222 );
	MoveTo( buttonFrame->left	+ 0, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->left	+ 0, buttonFrame->top		+ 3 );
	MoveTo( buttonFrame->left	+ 3, buttonFrame->top		+ 0 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->top		+ 0 );
	MoveTo( buttonFrame->right	- 4, buttonFrame->top		+ 0 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 0 );
	MoveTo( buttonFrame->right	- 1, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 1, buttonFrame->top		+ 3 );
	MoveTo( buttonFrame->right	- 1, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->right	- 1, buttonFrame->bottom	- 4 );
	MoveTo( buttonFrame->right	- 4, buttonFrame->bottom	- 1 );
	LineTo( buttonFrame->right	- 4, buttonFrame->bottom	- 1 );
	MoveTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 1 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 1 );
	MoveTo( buttonFrame->left	+ 0, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 0, buttonFrame->bottom	- 4 );
	SetFrameGreyButtonColor( 0xDDDD );
	MoveTo( buttonFrame->left	+ 1, buttonFrame->bottom	- 5 );
	LineTo( buttonFrame->left	+ 1, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->top		+ 1 );
	MoveTo( buttonFrame->left	+ 2, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->left	+ 4, buttonFrame->top		+ 1 );
	LineTo( buttonFrame->right	- 5, buttonFrame->top		+ 1 );
	SetFrameGreyButtonColor( 0xAAAA );
	MoveTo( buttonFrame->left	+ 2, buttonFrame->top		+ 4 );
	LineTo( buttonFrame->left	+ 4, buttonFrame->top		+ 2 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 2 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 3, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 3, buttonFrame->bottom	- 5 );
	LineTo( buttonFrame->right	- 5, buttonFrame->bottom	- 3 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 3 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 2, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 2, buttonFrame->top		+ 4 );
	SetFrameGreyButtonColor( 0x7777 );
	MoveTo( buttonFrame->right	- 2, buttonFrame->top		+ 4 );
	LineTo( buttonFrame->right	- 2, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->right	- 4, buttonFrame->bottom	- 2 );
	LineTo( buttonFrame->left	+ 4, buttonFrame->bottom	- 2 );
	SetFrameGreyButtonColor( 0x7777 );
	MoveTo( buttonFrame->left	+ 3, buttonFrame->top		+ 4 );
	LineTo( buttonFrame->left	+ 4, buttonFrame->top		+ 3 );
	MoveTo( buttonFrame->right	- 5, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 4 );
	MoveTo( buttonFrame->right	- 4, buttonFrame->bottom	- 5 );
	LineTo( buttonFrame->right	- 5, buttonFrame->bottom	- 4 );
	MoveTo( buttonFrame->left	+ 4, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 5 );
	SetFrameGreyButtonColor( 0xCCCC );
	MoveTo( buttonFrame->right	- 4, buttonFrame->top		+ 1 );
	LineTo( buttonFrame->right	- 4, buttonFrame->top		+ 1 );
	MoveTo( buttonFrame->left	+ 1, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->left	+ 1, buttonFrame->bottom	- 4 );
	SetFrameGreyButtonColor( 0xBBBB );
	MoveTo( buttonFrame->right	- 3, buttonFrame->top		+ 2 );
	LineTo( buttonFrame->right	- 3, buttonFrame->top		+ 2 );
	MoveTo( buttonFrame->left	+ 2, buttonFrame->bottom	- 3 );
	LineTo( buttonFrame->left	+ 2, buttonFrame->bottom	- 3 );
	SetFrameGreyButtonColor( 0x8888 );
	MoveTo( buttonFrame->right	- 2, buttonFrame->top		+ 3 );
	LineTo( buttonFrame->right	- 2, buttonFrame->top		+ 3 );
	MoveTo( buttonFrame->right	- 3, buttonFrame->bottom	- 4 );
	LineTo( buttonFrame->right	- 4, buttonFrame->bottom	- 3 );
	MoveTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 2 );
	LineTo( buttonFrame->left	+ 3, buttonFrame->bottom	- 2 );
	SetFrameGreyButtonColor( 0x0000 );
}

void SetFrameGreyButtonColor( short color )
{
	RGBColor	grey;
	grey.red	= 
	grey.green	= 
	grey.blue	= color;
	
	RGBForeColor( &grey );
}