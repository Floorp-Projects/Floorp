/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
#include "PtRawDrawContainer.h"
#include <stdio.h>

/* prototype declarations */
PtWidgetClass_t *CreateRawDrawContainerClass( void );

/* widget class pointer - class structure, create function */
PtWidgetClassRef_t WRawDrawContainer = { NULL, CreateRawDrawContainerClass };
PtWidgetClassRef_t *PtRawDrawContainer = &WRawDrawContainer; 

//
// Defaults function
//
static void raw_draw_container_dflts( PtWidget_t *widget )
{
	RawDrawContainerWidget	*rdc = ( RawDrawContainerWidget * ) widget;
     widget->flags |= ( Pt_OPAQUE );
	rdc->draw_f = NULL;
}

//
// Draw function
//
static void raw_draw_container_draw( PtWidget_t *widget, PhTile_t *damage )
{
  RawDrawContainerWidget	*rdc = ( RawDrawContainerWidget * ) widget;


  if (widget == NULL)
  {
    printf("raw_draw_container_draw NULL widget!\n");
    return;	
  }
  if( rdc->draw_f )
    rdc->draw_f( widget, damage );
}

//
// Class creation function
//
PtWidgetClass_t *CreateRawDrawContainerClass( void )
{
	// define our resources
	static PtResourceRec_t resources[] = {
		{ RDC_DRAW_FUNC, Pt_CHANGE_INVISIBLE, 0, 
			Pt_ARG_IS_POINTER( RawDrawContainerWidget, draw_f ), 0 }
		};

	// set up our class member values
	static PtArg_t args[] = {
		{ Pt_SET_VERSION, 110},
		{ Pt_SET_STATE_LEN, sizeof( RawDrawContainerWidget ) },
		{ Pt_SET_DFLTS_F, (long)raw_draw_container_dflts },
		{ Pt_SET_DRAW_F, (long)raw_draw_container_draw },
		{ Pt_SET_FLAGS, Pt_RECTANGULAR | Pt_OPAQUE, Pt_RECTANGULAR | Pt_OPAQUE },
		{ Pt_SET_NUM_RESOURCES, sizeof( resources ) / sizeof( resources[0] ) },
		{ Pt_SET_RESOURCES, (long)resources, sizeof( resources ) / sizeof( resources[0] ) },
		};

	// create the widget class
	return( PtRawDrawContainer->wclass = PtCreateWidgetClass(
		PtContainer, 0, sizeof( args )/sizeof( args[0] ), args ) );
}
