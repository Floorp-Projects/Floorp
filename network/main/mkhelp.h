/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
#ifndef MKHELP_H
#define MKHELP_H

/* EA: Help Begin */

/* The following structure stores the URL of the current help topic and project
   file of a help window.  Because this information is accessed by a javascript
   context sometime after the URL is delivered to the window, we needed a persistent
   way in which the data would remain associated with a given window, and,
   despite a great desire to not muck around with the MWContext, that was the
   only thing that was in scope at the time.  Lou Montoulli suggested using
   fields that are part of the URL_struct, such as the fe_data and Chrome
   fields, but these were not in scope at the time that the Javascript calls
   were being evaluated. */

typedef struct HelpInfoStruct_ {
	char		*topicURL;
	char		*projectURL;
} HelpInfoStruct;

#define HELP_INFO_PTR(MW_CTX)		((HelpInfoStruct *) (MW_CTX).pHelpInfo)

/* Called by netlib to parse a nethelp URL.  ParseNetHelpURL alters the
   URL struct passed in to point to the location of the help project file
   specified by the nethelp URL. */
   
extern int
NET_ParseNetHelpURL(URL_Struct *URL_s);

/* stream converter to parse an HTML help mapping file
 * and load the url associated with the id
 */
extern NET_StreamClass *
NET_HTMLHelpMapToURL(int         format_out,
                     void       *data_object,
                     URL_Struct *URL_s,
                     MWContext  *window_id);

#endif /* MKHELP_H */

