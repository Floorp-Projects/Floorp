/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _VIEW_H_
#define _VIEW_H_

#include "io.h"

typedef struct View
{
	int	backslash;
	FILE	*out;
} View;

View *viewAlloc(void);
void viewHTML(View *view, Input *input);
void viewHTMLAttributeName(View *view, Input *input);
void viewHTMLAttributeValue(View *view, Input *input);
void viewHTMLTag(View *view, Input *input);
void viewHTMLText(View *view, Input *input);
void viewHTTP(View *view, Input *input);
void viewHTTPHeaderName(View *view, Input *input);
void viewHTTPHeaderValue(View *view, Input *input);
void viewReport(View *view, char *str);
void viewVerbose(void);

#endif /* _VIEW_H_ */
