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
// mkmarimb.cpp -- Handles "castanet:" URLs for core Navigator, without
//               requiring libmsg. 
//

#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#include "marimurl.h"
#include "xpgetstr.h"
#include "net.h"
#include "xp_str.h"
#include "client.h"

/* a stub */

MODULE_PRIVATE void
NET_InitMarimbaProtocol(void)
{
	return;
}


/* this is a stub converter

 */
PUBLIC NET_StreamClass *
NET_DoMarimbaApplication(int format_out, 
                         void *data_obj,
                             URL_Struct *url_struct, 
                         MWContext *context)
{
  return(NULL);

}

