/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* PtRawDrawContainer.c - widget source file */

#include "PtRawDrawContainer.h"
#include <stdio.h>
#include "nslog.h"

NS_IMPL_LOG(PtRawDrawContainerLog, 0)
#define PRINTF NS_LOG_PRINTF(PtRawDrawContainerLog)
#define FLUSH  NS_LOG_FLUSH(PtRawDrawContainerLog)

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
	PtContainerWidget_t		  *container = ( PtContainerWidget_t * ) widget;
	PtBasicWidget_t		      *basic = ( PtBasicWidget_t * ) widget;
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
    PRINTF("raw_draw_container_draw NULL widget!\n");
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
		RDC_DRAW_FUNC, Pt_CHANGE_INVISIBLE, 0, 
			Pt_ARG_IS_POINTER( RawDrawContainerWidget, draw_f ), 0,
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
