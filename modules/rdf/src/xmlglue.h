/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef	_RDF_XMLGLUE_H_
#define	_RDF_XMLGLUE_H_

#include "xmlparse.h"

#ifdef XP_UNIX
#include <sys/fcntl.h>
#elif defined(XP_MAC)
#include <fcntl.h>
#endif



/* xmlglue.c data structures and defines */

#define XML_LINE_SIZE 500



/* xmlglue.c function prototypes */

XP_BEGIN_PROTOS

unsigned int			xml_write_ready(NET_StreamClass *stream);
void				xml_abort(NET_StreamClass *stream, int status);
void				xml_complete (NET_StreamClass *stream);
unsigned int			xmlcss_write_ready(NET_StreamClass *stream);
void				xmlcss_abort(NET_StreamClass *stream, int status);
void				xmlcss_complete (NET_StreamClass *stream);
void				outputToStream (XMLFile f, char* s);
void				xmlcss_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
void				readCSS (StyleSheet ss);
int				xmlhtml_write(NET_StreamClass *stream, const char *str, int32 len);
unsigned int			xmlhtml_write_ready(NET_StreamClass *stream);
void				xmlhtml_abort(NET_StreamClass *stream, int status);
void				xmlhtml_complete  (NET_StreamClass *stream);
void				xmlhtml_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx);
void				readHTML (char* url, XMLHTMLInclusion ss);

PUBLIC NET_StreamClass *	XML_XMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
PUBLIC NET_StreamClass *	XML_CSSConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
PUBLIC NET_StreamClass *	XML_HTMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);

XP_END_PROTOS

#endif
