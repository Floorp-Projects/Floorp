/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define XMLJS_INTERNAL
#include "xmljs.h"
#undef XMLJS_INTERNAL
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * Parse a given string of XML.
 * If we fail, we (try to) set an error property on the given obj, and
 * return JSVAL_FALSE.
 * If all is well, we return JSVAL_TRUE.
 */

JSBool
xmljs_parse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    XMLCallback *cb;
    XML_Parser xml;
    JSString *str;
    JSBool ok;

    cb = (XMLCallback *)JS_GetPrivate(cx, obj);
    if (!cb) {
	JS_ReportError(cx, "XMLParser object has no parser!");
	return JS_FALSE;
    }
    if (cb->preParse &&
	!cb->preParse(cx, obj, argc, argv, rval))
	return JS_FALSE;
    if (argc < 1) {
	*rval = JSVAL_TRUE;
	return JS_TRUE;
    }

    xml = XML_ParserCreate(NULL);
    if (!xml)
	return JS_FALSE;	/* XXX report error */
    /* after this point, have to leave via out: */

    XML_SetElementHandler(xml, cb->start, cb->end);
    XML_SetCharacterDataHandler(xml, cb->cdata);
    XML_SetProcessingInstructionHandler(xml, cb->processing);
    XML_SetUserData(xml, (void *)cb);
    cb->xml = xml;
    str = JS_ValueToString(cx, argv[0]);
    if (!XML_Parse(xml, JS_GetStringBytes(str), JS_GetStringLength(str), 1)) {
	str = JS_NewStringCopyZ(cx, XML_ErrorString(XML_GetErrorCode(xml)));
	if (!str) {
	    ok = JS_FALSE;
	    goto out;
	}
	if (!JS_DefineProperty(cx, obj, "error", STRING_TO_JSVAL(str),
			       NULL, NULL, JSPROP_ENUMERATE)) {
	    ok = JS_FALSE;
	    goto out;
	}
	*rval = JSVAL_FALSE;
    } else {
	*rval = JSVAL_TRUE;
    }
 out:
    XML_ParserFree(xml);
    cb->xml = NULL;
    return ok;
}

/**
 * Get a file into a string and call the above.
 * XXX when File object is present, we don't need this mess.
 */

static JSBool
xmljs_parseFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSString *str, *filename;
    struct stat sbuf;
    char * buf;
    int fd, len;

    *rval = JSVAL_TRUE;
    if (argc < 1)
	return JS_TRUE;
    filename = JS_ValueToString(cx, argv[0]);
    if (!filename)
	return JS_TRUE;

    argv[0] = STRING_TO_JSVAL(filename);

    fd = open(JS_GetStringBytes(filename), O_RDONLY);
    if (fd < 0) {
	JS_ReportError(cx,
		       "XMLParser.prototype.parseFile: open of %s failed!\n",
		       JS_GetStringBytes(filename));
	return JS_FALSE;
    }
    
    if (fstat(fd, &sbuf) < 0) {
	JS_ReportError(cx,
		       "XMLParser.prototype.parseFile: stat of %s failed!\n",
		       JS_GetStringBytes(filename));
	close(fd);
	return JS_FALSE;
    }

    len = sbuf.st_size;
    buf = (char *)JS_malloc(cx, len);
    if (read(fd, (void *)buf, (size_t)len) < len) {
	JS_ReportError(cx,
		       "XMLParser.prototype.parseFile: read of %s failed!\n",
		       JS_GetStringBytes(filename));
	JS_free(cx, buf);
	close(fd);
	return JS_FALSE;
    }
    close(fd);
    str = JS_NewString(cx, buf, len); /* hand buf off to engine; don't free! */
    if (!str) {
	JS_free(cx, buf);
	return JS_FALSE;
    }
    argv[0] = STRING_TO_JSVAL(str);
    return JS_CallFunctionName(cx, obj, "parse", argc, argv, rval);
}

JSBool
XML_Init(JSContext *cx, JSObject *obj)
{
    jsval rval;
    JSObject *proto;

    if (!JS_EvaluateScript(cx, JS_GetGlobalObject(cx), xmljs_newObj_str,
			   xmljs_newObj_size,
			   "XMLJS internal", 0, &rval))
	return JS_FALSE;

    proto = JSVAL_TO_OBJECT(rval);
    if (!JS_DefineFunction(cx, proto, "parse", xmljs_parse,
			   1, 0) ||
	!JS_DefineFunction(cx, proto, "parseFile", xmljs_parseFile,
			   1, 0))
	return JS_FALSE;

    return XMLParser_Init(cx, obj, proto) && XMLGraph_Init(cx, obj, proto);
}
