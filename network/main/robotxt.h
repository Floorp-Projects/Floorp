/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*** robotxt.h ****************************************************/
/*   description:		parses the robots.txt file                */
/*                      - not dependent on the crawler            */
  

 /********************************************************************
	See the robots.txt specification at:

	http://info.webcrawler.com/mak/projects/robots/norobots.html (original spec)
	http://info.webcrawler.com/mak/projects/robots/norobots-rfc.html

	Note: the original spec says that at least one Disallow field must be present
	in a record. That is what I am following.

  $Revision: 1.1 $
  $Date: 1998/04/30 20:53:33 $

 *********************************************************************/

#ifndef robotctl_h___
#define robotctl_h___

#include "prtypes.h"
#include "ntypes.h"
#include "net.h"

typedef uint8 CRAWL_RobotControlStatus;
#define CRAWL_ROBOT_DISALLOWED			((CRAWL_RobotControlStatus)0x00)
#define CRAWL_ROBOT_ALLOWED				((CRAWL_RobotControlStatus)0x01)
#define CRAWL_ROBOTS_TXT_NOT_QUERIED	((CRAWL_RobotControlStatus)0x02)

typedef struct _CRAWL_RobotControlStruct *CRAWL_RobotControl;

/*
 * Typedef for function callback called after robots.txt is read.
 */
 typedef void
(PR_CALLBACK *CRAWL_RobotControlStatusFunc)(void *data);

/* stream function */
PUBLIC NET_StreamClass*
CRAWL_RobotsTxtConverter(int format_out,
						void *data_object,
						URL_Struct *URL_s,
						MWContext  *window_id);

/****************************************************************************************/
/* public API																			*/
/****************************************************************************************/

NSPR_BEGIN_EXTERN_C

/* Creates a robot control for the site. 
 Parameters:
	context - context for libnet
	site - protocol and host portion of url. "/robots.txt" will be appended to this to get the
		location of robots.txt.
*/
PR_EXTERN(CRAWL_RobotControl) 
CRAWL_MakeRobotControl(MWContext *context, char *site);

/* Destroys a robot control and all memory associated with it (except for the context or the
   opaque data supplied to CRAWL_ReadRobotControlFile)
*/
PR_EXTERN(void) 
CRAWL_DestroyRobotControl(CRAWL_RobotControl control);

/* Parses the robots.txt at the site specified in the control, and performs a callback when
   it is done. This function returns after issuing a request to netlib.
   Parameters:
	control - the robot control for the site
	func - completion callback
	data - data to provide to the callback which is opaque to the robots.txt parser
	freeData - if true, frees data (previous param) on completion
*/
PR_EXTERN(PRBool) 
CRAWL_ReadRobotControlFile(CRAWL_RobotControl control, CRAWL_RobotControlStatusFunc func, void *data, PRBool freeData);

/* Returns a status code indicating the robot directive for the url supplied */
PR_EXTERN(CRAWL_RobotControlStatus) 
CRAWL_GetRobotControl(CRAWL_RobotControl, char *url);

NSPR_END_EXTERN_C

#endif

