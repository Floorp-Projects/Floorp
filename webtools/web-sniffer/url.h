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

#ifndef _URL_H_
#define _URL_H_

/* <scheme>://<net_loc>/<path>;<params>?<query>#<fragment> */

/* <net_loc> = <login>:<password>@<host>:<port> */

typedef struct URL
{
	/* standard components */
	unsigned char		*scheme;
	unsigned char		*net_loc;
	unsigned char		*path;
	unsigned char		*params;
	unsigned char		*query;
	unsigned char		*fragment;

	/* the whole url */
	unsigned char		*url;

	/* for convenience */
	unsigned char		*file;
	unsigned char		*host;
	unsigned char		*login;
	unsigned char		*password;
	unsigned char		*pathWithoutFile;
	int			port;

	/* for linked list */
	struct URL		*next;
} URL;

void urlDecode(unsigned char *url);
void urlFree(URL *url);
URL *urlParse(const unsigned char *url);
URL *urlRelative(const unsigned char *baseURL,
	const unsigned char *relativeURL);

#endif /* _URL_H_ */
