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
