/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 */

#ifndef _MIME_H_
#define _MIME_H_

typedef struct ContentType ContentType;

void mimeFreeContentType(ContentType *contentType);
unsigned char *mimeGetContentType(ContentType *contentType);
unsigned char *mimeGetContentTypeParameter(ContentType *contentType,
	char *parameter);
ContentType *mimeParseContentType(unsigned char *str);

#endif /* _MIME_H_ */
