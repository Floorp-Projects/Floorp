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

#include "html.h"
#include "io.h"
#include "view.h"

enum {
	appTimeConnectSuccess,
	appTimeConnectFailure,
	appTimeGetHostByNameSuccess,
	appTimeGetHostByNameFailure,
	appTimeReadStream,
	appTimeTotal,

	appTimeMax			/* must be last */
};

struct App
{
	void	(*contentType)(App *app, unsigned char *contentType);
	void	(*html)(App *app, Input *input);
	void	(*htmlAttributeName)(App *app, HTML *html, Input *input);
	void	(*htmlAttributeValue)(App *app, HTML *html, Input *input);
	void	(*htmlTag)(App *app, HTML *html, Input *input);
	void	(*htmlText)(App *app, Input *input);
	void	(*http)(App *app, Input *input);
	void	(*httpBody)(App *app, Input *input);
	void	(*httpCharSet)(App *app, unsigned char *charset);
	void	(*httpHeaderName)(App *app, Input *input);
	void	(*httpHeaderValue)(App *app, Input *input, unsigned char *url);
	void	(*status)(App *app, char *message, char *file, int line);
	void	(*time)(App *app, int task, struct timeval *theTime);

	View	view;
	void	*data;
};

extern App appDefault;

App *appAlloc(void);
