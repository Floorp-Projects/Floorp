/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef XMLJS_H
#define XMLJS_H

#include <xmlparse.h>
#include <jsapi.h>

extern JSBool
XML_Init(JSContext *cx, JSObject *obj);

#ifdef XMLJS_INTERNAL
typedef struct {
    JSContext *cx;
    JSObject *obj;
    XML_Parser xml;
    XML_StartElementHandler start;
    XML_EndElementHandler end;
    XML_CharacterDataHandler cdata;
    XML_ProcessingInstructionHandler processing;
    JSNative preParse;
    JSNative postParse;
} XMLCallback;

extern JSBool
XMLParser_Init(JSContext *cx, JSObject *obj, JSObject *parent_proto);

extern JSBool
XMLGraph_Init(JSContext *cx, JSObject *obj, JSObject *parent_proto);

#define xmljs_newObj_str "new Object();"
#define xmljs_newObj_size (sizeof(xmljs_newObj_str) - 1)

#endif /* XMLJS_INTERNAL */

#endif /* XMLJS_H */
