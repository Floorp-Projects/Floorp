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

#ifndef _HTML_H_
#define _HTML_H_

#include "io.h"

typedef struct HTMLAttribute
{
	unsigned char		*name;
	unsigned char		*value;
	struct HTMLAttribute	*next;
} HTMLAttribute;

typedef struct HTML
{
	unsigned char	*base;
	unsigned char	*url;
	unsigned char	*tag;
	int		tagIsKnown;
	HTMLAttribute	*attributes;
	HTMLAttribute	*currentAttribute;
	int		currentAttributeIsURL;
} HTML;

typedef void (*HTMLHandler)(App *app, HTML *html);

void htmlRead(App *app, Input *input, unsigned char *base);
void htmlRegister(char *tag, char *attributeName, HTMLHandler handler);
void htmlRegisterTagHandler(HTMLHandler handler);
void htmlRegisterURLHandler(HTMLHandler handler);
unsigned char *toHTML(unsigned char *str);
unsigned char *escapeHTML(unsigned char *str);

#endif /* _HTML_H_ */
