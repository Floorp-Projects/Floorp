/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* 
   This is some glue code for feeding data into the xml parser.
   It also has glue code for using CSS to specify display properties
   for an xml file.

   For more info on XML in navigator, look at the XML section
   of www.mozilla.org.

   If you have questions, send them to guha@netscape.com
*/
#include "xmlglue.h"

#ifdef MOZILLA_CLIENT 

 
#include "xmlparse.h"
#include "xmlss.h"
#include "layers.h"



unsigned int
xml_write_ready(NET_StreamClass *stream)
{
   return XML_LINE_SIZE;

}


 
int xml_parse_abort (NET_StreamClass *stream) {
  XMLFile obj= (XMLFile) stream->data_object;
  NET_StreamClass *newstream;
  URL_Struct *urls = obj->urls;
  const char* error = XML_ErrorString(XML_GetErrorCode(obj->parser));
  char*  buff  = getMem(150);
  int n = XML_GetErrorLineNumber(obj->parser);
  URL_Struct *nurls =   NET_CreateURLStruct(copyString(urls->address), NET_DONT_RELOAD);
  StrAllocCopy(nurls->content_type, TEXT_HTML);
  newstream = NET_StreamBuilder(1,  nurls, (MWContext*)obj->mwcontext);
  sprintf(buff, "<B>Bad XML at line %i :</B><BR><P> ERROR = ", n);
   (*(newstream->put_block))(newstream, buff, strlen(buff)); 
  (*(newstream->put_block))(newstream, error, strlen(error)); 
  newstream->complete(newstream);
  freeMem(buff);
  NET_FreeURLStruct(nurls); 
  return -1;
}

void
xml_abort(NET_StreamClass *stream, int status)
{
	XMLFile obj= (XMLFile) stream->data_object;
	if (obj->status != 1) {
	   xml_parse_abort(stream);
	   } else {
    XML_Parse(obj->parser, NULL, 0, 1);
	   }
    XML_ParserFree(obj->parser);
}

 
int
xml_write(NET_StreamClass *stream, char* data, int32 len)
{
	XMLFile obj= (XMLFile) stream->data_object;
	char* ndata = getMem(len+1);
	memcpy(ndata, data, len);
    if (obj->status == 1) obj->status = XML_Parse(obj->parser,  ndata, len, 0);	
    return 0;
}


void
xml_complete (NET_StreamClass *stream)
{
  NET_StreamClass *newstream;
  XMLFile obj= (XMLFile) stream->data_object;
  URL_Struct *urls = obj->urls;
   if (obj->status != 1) {
	   xml_parse_abort(stream);
	   return;
  } 
  XML_Parse(obj->parser, NULL, 0, 1);
  XML_ParserFree(obj->parser); 
  obj->numOpenStreams--;
  
  if (obj->numOpenStreams < 1) {
  /* direct the stream to the html parser */
    URL_Struct *nurls =   NET_CreateURLStruct(copyString(urls->address), NET_DONT_RELOAD);
    StrAllocCopy(nurls->content_type, TEXT_HTML);

    newstream = NET_StreamBuilder(1,  nurls, (MWContext*)obj->mwcontext);
    obj->stream = newstream;
    convertToHTML(obj);
    outputToStream(obj, NULL);
    newstream->complete(newstream);
    NET_FreeURLStruct(nurls); 
  }
}

#define OUTPUT_BUFFER_SIZE (4096*16)

void
outputToStream (XMLFile f, char* s)
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
  int ans = 0;
  NET_StreamClass *stream = (NET_StreamClass*) f->stream;
  if (s == NULL) {
	  if (f->outputBuffer) {
        XP_Rect rect;
		CL_OffscreenMode save_offscreen_mode;
        CL_Compositor *comp = ((MWContext*)f->mwcontext)->compositor;
		rect.left = rect.top = 0;
		rect.right = 1600;
		rect.bottom = 16000;
        /*        CL_SetCompositorEnabled(comp, 0); */
		CL_SetCompositorOffscreenDrawing(comp, CL_OFFSCREEN_ENABLED);
        (*(stream->put_block))(stream, f->outputBuffer, strlen(f->outputBuffer));
        /*        CL_SetCompositorEnabled(comp, 1);
		CL_SetCompositorOffscreenDrawing(comp, CL_OFFSCREEN_ENABLED); */
        CL_UpdateDocumentRect(comp,&rect, 0);
        freeMem(f->outputBuffer);
        f->outputBuffer = NULL;
	  } 
     return;
  }
#ifdef DEBUG
  /*  FE_Trace(s); */
#endif
  if (f->outputBuffer == NULL) f->outputBuffer = getMem(OUTPUT_BUFFER_SIZE+1);
  if ((strlen(s) > OUTPUT_BUFFER_SIZE)) {
    char* buff = copyString(s);
    if (buff) (*(stream->put_block))(stream, buff, strlen(s)); 
    CL_CompositeNow(((MWContext*)f->mwcontext)->compositor);
    freeMem(buff);
  } else if (strlen(f->outputBuffer) + strlen(s) > OUTPUT_BUFFER_SIZE) {
     (*(stream->put_block))(stream, f->outputBuffer, strlen(f->outputBuffer));
     CL_CompositeNow(((MWContext*)f->mwcontext)->compositor);
     memset(f->outputBuffer, '\0', OUTPUT_BUFFER_SIZE);
     sprintf(f->outputBuffer, s);
  } else {
    strcat(f->outputBuffer, s);
  }
#endif /* MOZ_NGLAYOUT */
}



#ifdef	XP_MAC
PR_PUBLIC_API(NET_StreamClass *)
#else
PUBLIC NET_StreamClass *
#endif

XML_XMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id)
{
    NET_StreamClass* stream;
    XMLFile  xmlf; 
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    xmlf = (XMLFile) getMem(sizeof(XMLFileStruct));
    xmlf->urls = URL_s;
    xmlf->parser = XML_ParserCreate(NULL) ;
    XML_SetElementHandler(xmlf->parser, (void (*)(void*, const char*, const char**))XMLDOM_StartHandler, (void (*)(void*, const char*))XMLDOM_EndHandler);
    XML_SetCharacterDataHandler(xmlf->parser, (void (*)(void*, const char*, int))XMLDOM_CharHandler);
    XML_SetProcessingInstructionHandler(xmlf->parser, (void (*)(void*, const char*, const char*))XMLDOM_PIHandler);
    XML_SetUserData(xmlf->parser, xmlf);
    xmlf->status = 1;
    xmlf->address = copyString(URL_s->address);
    xmlf->mwcontext = window_id;
    window_id->xmlfile = xmlf;
    xmlf->numOpenStreams = 1;
	/*	URL_s->fedata = xmlf; */
    stream = NET_NewStream("XML", (MKStreamWriteFunc)xml_write, 
			   (MKStreamCompleteFunc)xml_complete,
			   (MKStreamAbortFunc)xml_abort, 
			   (MKStreamWriteReadyFunc)xml_write_ready, xmlf, window_id);
    if (stream == NULL) freeMem(xmlf);
	
    return(stream);
}



/* XML-CSS stuff */



unsigned int
xmlcss_write_ready(NET_StreamClass *stream)
{
   return XML_LINE_SIZE;

}



void
xmlcss_abort(NET_StreamClass *stream, int status)
{
  StyleSheet obj=stream->data_object;
  XMLFile xml = obj->xmlFile;
  xml->numOpenStreams--;
  freeMem(obj->holdOver);
  freeMem(obj->line);
  obj->line = obj->holdOver = NULL;
}



void
xmlcss_complete  (NET_StreamClass *stream)
{
  StyleSheet ss =stream->data_object;
  URL_Struct *urls = ss->urls;
  XMLFile xml = ss->xmlFile;
  freeMem(ss->holdOver);
  freeMem(ss->line);
  ss->line = ss->holdOver = NULL; 
  xml->numOpenStreams--;
  if (xml->numOpenStreams == 0) { 
  /* direct the stream to the html parser */
    NET_StreamClass *newstream;
    URL_Struct *nurls =   NET_CreateURLStruct(copyString(xml->address), NET_DONT_RELOAD);
    StrAllocCopy(nurls->content_type, TEXT_HTML);
    newstream = NET_StreamBuilder(1,  nurls, (MWContext*) xml->mwcontext);
    xml->stream = newstream;
    convertToHTML(xml);
    outputToStream(xml, NULL);
    newstream->complete(newstream);
    NET_FreeURLStruct(nurls); 
  }     
}



#ifdef	XP_MAC
PR_PUBLIC_API(NET_StreamClass *)
#else
PUBLIC NET_StreamClass *
#endif

XML_CSSConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id)
{
    StyleSheet  ss = (StyleSheet) URL_s->fe_data;
    TRACEMSG(("Setting up style sheet stream. Have URL: %s\n", URL_s->address));

    return NET_NewStream("XML-CSS", (MKStreamWriteFunc)parseNextXMLCSSBlob, 
			   (MKStreamCompleteFunc)xmlcss_complete,
			   (MKStreamAbortFunc)xmlcss_abort, 
			   (MKStreamWriteReadyFunc)xmlcss_write_ready, ss, window_id);
}



void
xmlcss_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{

}



void
readCSS (StyleSheet ss)
{
  URL_Struct                      *urls;
  urls = NET_CreateURLStruct(ss->address, NET_DONT_RELOAD);
  if (urls == NULL)  {
    ss->xmlFile->numOpenStreams--;
    return;
  }
  urls->fe_data = ss;
  NET_GetURL(urls, FO_CACHE_AND_XMLCSS, ss->xmlFile->mwcontext, xmlcss_GetUrlExitFunc);
}



int
xmlhtml_write(NET_StreamClass *stream, const char *str, int32 len)
{
  XMLHTMLInclusion obj=stream->data_object;
  char* newStr;
  if (obj == NULL  || len < 0) {
      return MK_INTERRUPTED;
  }
 newStr = getMem(len+1);
 memcpy(newStr, str, len);
 *(obj->content + obj->n++) = newStr;
 return (len);
}



unsigned int
xmlhtml_write_ready(NET_StreamClass *stream)
{
   return XML_LINE_SIZE;

}



void
xmlhtml_abort(NET_StreamClass *stream, int status)
{
}




void xmlhtml_complete_int (XMLFile xml) {
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
  MWContext *cx = (MWContext *)xml->mwcontext;
  int16 save_offscreen_mode;

  xml->numOpenStreams--;
  if (xml->numOpenStreams < 1) { 
  /* direct the stream to the html parser */
    NET_StreamClass *newstream;
    URL_Struct *nurls =   NET_CreateURLStruct(copyString(xml->address), NET_DONT_RELOAD);
    StrAllocCopy(nurls->content_type, TEXT_HTML);
    newstream = NET_StreamBuilder(1,  nurls, (MWContext*) xml->mwcontext);
    xml->stream = newstream;
    if (cx->compositor)
      {
        /* Temporarily force drawing to use the offscreen buffering area to reduce
           flicker when outputing html. (If no offscreen store is allocated, this code will
		   have no effect, but it will do no harm.) */
		save_offscreen_mode = CL_GetCompositorOffscreenDrawing(cx->compositor);
		CL_SetCompositorOffscreenDrawing(cx->compositor, CL_OFFSCREEN_ENABLED);
      }
    convertToHTML(xml);
    outputToStream(xml, NULL);    
    if (cx->compositor)
      {
/*		CL_CompositeNow(cx->compositor);
		CL_SetCompositorOffscreenDrawing(cx->compositor, save_offscreen_mode); */
      }
    newstream->complete(newstream);
    NET_FreeURLStruct(nurls); 
  }     
#endif /* MOZ_NGLAYOUT */
}

void
xmlhtml_complete  (NET_StreamClass *stream)
{
  XMLHTMLInclusion ss =stream->data_object;
  XMLFile xml = ss->xml;
  xmlhtml_complete_int(xml);
}



#ifdef	XP_MAC
PR_PUBLIC_API(NET_StreamClass *)
#else
PUBLIC NET_StreamClass *
#endif

XML_HTMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id)
{
    XMLHTMLInclusion  ss = (XMLHTMLInclusion) URL_s->fe_data;

    return NET_NewStream("XML-HTML", (MKStreamWriteFunc)xmlhtml_write, 
			   (MKStreamCompleteFunc)xmlhtml_complete,
			   (MKStreamAbortFunc)xmlhtml_abort, 
			   (MKStreamWriteReadyFunc)xmlhtml_write_ready, ss, window_id);
}



void
xmlhtml_GetUrlExitFunc (URL_Struct *urls, int status, MWContext *cx)
{
}


void setTransclusionProp (XMLFile f, int32 n, char* prop, char* value) {
}

void
readHTML (char* url, XMLHTMLInclusion ss)
{
  URL_Struct                      *urls;
  urls = NET_CreateURLStruct(url, NET_DONT_RELOAD);
  if (urls == NULL)  {
    ss->xml->numOpenStreams--;
    return;
  }
  urls->fe_data = ss;
  NET_GetURL(urls, FO_CACHE_AND_XMLHTML, ss->xml->mwcontext, xmlhtml_GetUrlExitFunc);
}

#endif
