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

#include "buf.h"

typedef struct View
{
	int	backslash;
	FILE	*out;
} View;

void viewHTML(App *app, Buf *buf);
void viewHTMLAttributeName(App *app, Buf *buf);
void viewHTMLAttributeValue(App *app, Buf *buf);
void viewHTMLDeclaration(App *app, Buf *buf);
void viewHTMLProcessingInstruction(App *app, Buf *buf);
void viewHTMLTag(App *app, Buf *buf);
void viewHTMLText(App *app, Buf *buf);
void viewHTTP(App *app, Buf *buf);
void viewHTTPHeaderName(App *app, Buf *buf);
void viewHTTPHeaderValue(App *app, Buf *buf);
void viewPrintHTML(App *app, char *str);
void viewReport(App *app, char *str);
void viewReportHTML(App *app, char *str);
void viewVerbose(void);

#endif /* _VIEW_H_ */
