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

/* 
   This is some glue code for a "stop-gap" xml parser that is being shipped with the free
   source. A real xml parser from James Clark will replace this 
   within the next few weeks.

   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
   For more info on XML in navigator, look at the XML section
   of www.mozilla.org.
*/

#include "xmlglue.h"
#include "xmlparse.h"
#include "xmlss.h"



unsigned int
xml_write_ready(NET_StreamClass *stream)
{
   return XML_LINE_SIZE;

}



void
xml_abort(NET_StreamClass *stream, int status)
{
	void *obj=stream->data_object;
  freeMem(((XMLFile)obj)->holdOver);
  freeMem(((XMLFile)obj)->line);
  ((XMLFile)obj)->line = ((XMLFile)obj)->holdOver = NULL;
}



void
xml_complete (NET_StreamClass *stream)
{
  NET_StreamClass *newstream;
  void *obj=stream->data_object;
  URL_Struct *urls = ((XMLFile)obj)->urls;
  freeMem(((XMLFile)obj)->holdOver);
  freeMem(((XMLFile)obj)->line);
  ((XMLFile)obj)->line = ((XMLFile)obj)->holdOver = NULL; 
  ((XMLFile)obj)->numOpenStreams--;
  if (((XMLFile)obj)->numOpenStreams < 1) {
  /* direct the stream to the html parser */
    URL_Struct *nurls =   NET_CreateURLStruct(copyString(urls->address), NET_DONT_RELOAD);
    StrAllocCopy(nurls->content_type, TEXT_HTML);

    newstream = NET_StreamBuilder(1,  nurls, (MWContext*) ((XMLFile)obj)->mwcontext);
    ((XMLFile)obj)->stream = newstream;
    convertToHTML(((XMLFile)obj));
    newstream->complete(newstream);
    NET_FreeURLStruct(nurls); 
  }
}



void
outputToStream (XMLFile f, char* s)
{
  int ans = 0;
  NET_StreamClass *stream = (NET_StreamClass*) f->stream;
#ifdef DEBUG
/*  FE_Trace(s); */
#endif
  (*(stream->put_block))(stream, s, strlen(s)); 
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
    xmlf->holdOver = getMem(XML_BUF_SIZE);
    xmlf->lineSize = XML_LINE_SIZE;
    xmlf->line     = (char*)getMem(XML_LINE_SIZE);
    xmlf->urls = URL_s;
    xmlf->address = copyString(URL_s->address);
    xmlf->mwcontext = window_id;
    xmlf->numOpenStreams = 1;
	/*	URL_s->fedata = xmlf; */
    stream = NET_NewStream("XML", (MKStreamWriteFunc)parseNextXMLBlob, 
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



void
xmlhtml_complete  (NET_StreamClass *stream)
{
  XMLHTMLInclusion ss =stream->data_object;
  XMLFile xml = ss->xml;
  xml->numOpenStreams--;
  if (xml->numOpenStreams == 0) { 
  /* direct the stream to the html parser */
    NET_StreamClass *newstream;
    URL_Struct *nurls =   NET_CreateURLStruct(copyString(xml->address), NET_DONT_RELOAD);
    StrAllocCopy(nurls->content_type, TEXT_HTML);
    newstream = NET_StreamBuilder(1,  nurls, (MWContext*) xml->mwcontext);
    xml->stream = newstream;
    convertToHTML(xml);
    newstream->complete(newstream);
    NET_FreeURLStruct(nurls); 
  }     
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
