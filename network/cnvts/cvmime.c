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

/* Please leave outside of ifdef for window precompiled headers */
#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "prmem.h"
#include "plstr.h"
#include "net_xp_file.h"

#ifdef MOZILLA_CLIENT


#include "mkstream.h"
#include "cvextcon.h"
#include "mkformat.h"
#include "il_strm.h"            /* Image Library stream converters. */

#include "mime.h"
#include "cvactive.h"
#include "cvunzip.h"
#include "cvchunk.h"
#include "gui.h"
#include "msgcom.h"
#include "msgnet.h"
#include "mkautocf.h"	/* Proxy auto-config */
#include "cvjscfg.h"	/* Javascript config */
#include "mkhelp.h"

#include "xlate.h"		/* Text and PostScript converters */
#include "libi18n.h"		/* For INTL_ConvCharCode()   */
#include "edt.h"
#include "secnav.h"
#include "intl_csi.h"


#include "m_cvstrm.h"
#include "cvmime.h"

#include "mimeenc.h"

#include "xpgetstr.h"
extern int XP_EDITOR_NON_HTML;

#ifdef CRAWLER
/* crawler converters */
#include "pagescan.h"
#include "crawler.h"
#include "robotxt.h"
#endif

#ifdef MOZ_MAIL_NEWS
/* #### defined in libmsg/msgutils.c */
extern NET_StreamClass * 
msg_MakeRebufferingStream (NET_StreamClass *next_stream,
						   URL_Struct *url,
						   MWContext *context);
#endif /* MOZ_MAIL_NEWS */

/* defined at the bottom of this file */
NET_StreamClass *
NET_PrintRawToDisk(int format_out, 
				   void *data_obj,
                   URL_Struct *url_struct, 
				   MWContext *context);

#ifdef MOZ_MAIL_NEWS
typedef struct MIME_DataObject {
  MimeDecoderData *decoder;		/* State used by the decoder */  
  NET_StreamClass *next_stream;	/* Where the output goes */
  PRBool partial_p;			    /* Whether we should close that stream */
} MIME_DataObject;





static int
net_mime_decoder_cb (const char *buf, int32 size, void *closure)
{
  MIME_DataObject *obj = (MIME_DataObject *) closure;
  NET_StreamClass *stream = (obj ? obj->next_stream : 0);
  if (stream)
	return stream->put_block (stream, (char *) buf, size);
  else
	return 0;
}

static int
net_MimeEncodingConverterWrite (NET_StreamClass *stream, CONST char* buffer,
								int32 length)
{
  MIME_DataObject *obj = (MIME_DataObject *) stream->data_object;  
  return MimeDecoderWrite (obj->decoder, (char *) buffer, length);
}


PRIVATE unsigned int net_MimeEncodingConverterWriteReady (NET_StreamClass *stream)
{
#if 1
  return (MAX_WRITE_READY);
#else
  MIME_DataObject *data = (MIME_DataObject *) stream->data_object;
  if(data->next_stream)
    return ((*data->next_stream->is_write_ready)
			(data->next_stream));
  else
    return (MAX_WRITE_READY);
#endif
}

PRIVATE void net_MimeEncodingConverterComplete (NET_StreamClass *stream)
{
  MIME_DataObject *data = (MIME_DataObject *) stream->data_object;  

  if (data->decoder)
	{
	  MimeDecoderDestroy(data->decoder, PR_FALSE);
	  data->decoder = 0;
	}

  /* complete the next stream */
  if (!data->partial_p && data->next_stream)
    {
      (*data->next_stream->complete) (data->next_stream);
      PR_Free (data->next_stream);
    }
  PR_Free (data);
}

PRIVATE void net_MimeEncodingConverterAbort (NET_StreamClass *stream, int status)
{
  MIME_DataObject *data = (MIME_DataObject *) stream->data_object;  

  if (data->decoder)
	{
	  MimeDecoderDestroy(data->decoder, PR_TRUE);
	  data->decoder = 0;
	}

  /* abort the next stream */
  if (!data->partial_p && data->next_stream)
    {
      (*data->next_stream->abort) (data->next_stream, status);
      PR_Free (data->next_stream);
    }
  PR_Free (data);
}
#endif /* MOZ_MAIL_NEWS */

PRIVATE NET_StreamClass * 
NET_MimeEncodingConverter_1 (int          format_out,
							 void        *data_obj,
							 URL_Struct  *URL_s,
							 MWContext   *window_id,
							 PRBool partial_p,
							 NET_StreamClass *next_stream)
{
#ifdef MOZ_MAIL_NEWS
   MIME_DataObject* obj;
   MimeDecoderData *(*fn) (int (*) (const char*, int32, void*), void*) = 0;

    NET_StreamClass* stream;
	char *type = (char *) data_obj;

    TRACEMSG(("Setting up encoding stream. Have URL: %s\n", URL_s->address));

    stream = PR_NEW(NET_StreamClass);
    if(stream == NULL) 
	  return(NULL);

	memset(stream, 0, sizeof(NET_StreamClass));

    obj = PR_NEW(MIME_DataObject);
    if (obj == NULL) 
	  return(NULL);
	memset(obj, 0, sizeof(MIME_DataObject));

	if (!PL_strcasecmp (type, ENCODING_QUOTED_PRINTABLE))
	  fn = &MimeQPDecoderInit;
	else if (!PL_strcasecmp (type, ENCODING_BASE64))
	  fn = &MimeB64DecoderInit;
	else if (!PL_strcasecmp (type, ENCODING_UUENCODE) ||
			 !PL_strcasecmp (type, ENCODING_UUENCODE2) ||
			 !PL_strcasecmp (type, ENCODING_UUENCODE3) ||
			 !PL_strcasecmp (type, ENCODING_UUENCODE4))
	  fn = &MimeUUDecoderInit;
	else
	  abort ();
	obj->decoder = fn (net_mime_decoder_cb, obj);
	if (!obj->decoder)
	  {
		PR_Free(obj);
		return 0;
	  }

    stream->put_block      = net_MimeEncodingConverterWrite;

    stream->name           = "Mime Stream";
    stream->complete       = net_MimeEncodingConverterComplete;
    stream->abort          = net_MimeEncodingConverterAbort;
    stream->is_write_ready = net_MimeEncodingConverterWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

	if (partial_p)
	  {
		PR_ASSERT (next_stream);
		obj->next_stream = next_stream;
		TRACEMSG(("Using existing stream in NET_MimeEncodingConverter\n"));
	  }
	else
	  {
		PR_ASSERT (!next_stream);

		/* open next stream
		 */
		PR_FREEIF (URL_s->content_encoding);
		obj->next_stream = NET_StreamBuilder (format_out, URL_s, window_id);

		if (!obj->next_stream)
		  return (NULL);

		/* When uudecoding, we tend to come up with tiny chunks of data
		   at a time.  Make a stream to put them back together, so that
		   we hand bigger pieces to the image library.
		 */
		{
		  NET_StreamClass *buffer =
			msg_MakeRebufferingStream (obj->next_stream, URL_s, window_id);
		  if (buffer)
			obj->next_stream = buffer;
		}

		TRACEMSG(("Returning stream from NET_MimeEncodingConverter\n"));
	  }

    return stream;
#else
   PR_ASSERT(0);
   return(NULL);
#endif /* MOZ_MAIL_NEWS */
}


PUBLIC NET_StreamClass * 
NET_MimeEncodingConverter (int          format_out,
						   void        *data_obj,
						   URL_Struct  *URL_s,
						   MWContext   *window_id)
{
  return NET_MimeEncodingConverter_1 (format_out, data_obj, URL_s, window_id,
									  PR_FALSE, 0);
}

NET_StreamClass * 
NET_MimeMakePartialEncodingConverterStream (int          format_out,
											void        *data_obj,
											URL_Struct  *URL_s,
											MWContext   *window_id,
											NET_StreamClass *next_stream)
{
  return NET_MimeEncodingConverter_1 (format_out, data_obj, URL_s, window_id,
									  PR_TRUE, next_stream);
}

/* Registers the default things that should be registered cross-platform.
   Really this doesn't belong in this file.
 */


PRIVATE
NET_StreamClass *
EDT_ErrorOut (int format_out,
			  void *data_obj,	
			  URL_Struct *URL_s,
			  MWContext  *window_id)
{

 	FE_Alert(window_id, XP_GetString(XP_EDITOR_NON_HTML));

	return(NULL);
}
		

PRIVATE void
net_RegisterDefaultDecoders (void)
{
  static PA_InitData parser_data;

#ifdef XP_UNIX
  NET_ClearExternalViewerConverters ();
#endif /* XP_UNIX */

  /* for the parser/layout functionality
   */
#ifdef EDITOR
  parser_data.output_func = EDT_ProcessTag;
#else
  parser_data.output_func = LO_ProcessTag;
#endif

  /* Convert the charsets of HTML into the canonical internal form.
   */
  NET_RegisterContentTypeConverter (TEXT_HTML, FO_PRESENT,
									NULL, INTL_ConvCharCode);

  NET_RegisterContentTypeConverter (TEXT_HTML, FO_PRESENT_INLINE,
									NULL, INTL_ConvCharCode);

  /* send all HTML to the editor, everything else as error
   */
  NET_RegisterContentTypeConverter (TEXT_HTML, FO_EDIT,
  									NULL, INTL_ConvCharCode);

  /* send file listings to html converter */
  NET_RegisterContentTypeConverter (APPLICATION_HTTP_INDEX, FO_PRESENT,
  									NULL, NET_HTTPIndexFormatToHTMLConverter);

  NET_RegisterContentTypeConverter ("*", FO_EDIT,
  									NULL, EDT_ErrorOut);

  NET_RegisterContentTypeConverter("*", FO_LOAD_HTML_HELP_MAP_FILE,
								   NULL, NET_HTMLHelpMapToURL);

#ifdef CRAWLER
  NET_RegisterContentTypeConverter(TEXT_HTML, FO_CRAWL_PAGE, 
									NULL, INTL_ConvCharCode);

  NET_RegisterContentTypeConverter("*", FO_CRAWL_PAGE, 
									NULL, CRAWL_CrawlerConverter);

  NET_RegisterContentTypeConverter("*", FO_CRAWL_RESOURCE, 
									NULL, CRAWL_CrawlerResourceConverter);

  NET_RegisterContentTypeConverter("*", FO_ROBOTS_TXT, 
									NULL, CRAWL_RobotsTxtConverter);
#endif /* CRAWLER */

	/* this should be windows and mac soon too... */
#ifdef XP_UNIX
	/* the view source converter */
  NET_RegisterContentTypeConverter ("*", FO_VIEW_SOURCE,
									TEXT_PLAIN, net_ColorHTMLStream);
  NET_RegisterContentTypeConverter (TEXT_HTML, FO_VIEW_SOURCE,
									NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_VIEW_SOURCE,
									NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_VIEW_SOURCE,
									NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_VIEW_SOURCE,
									TEXT_HTML, net_ColorHTMLStream);
  NET_RegisterContentTypeConverter (APPLICATION_HTTP_INDEX, FO_VIEW_SOURCE,
  									NULL, NET_HTTPIndexFormatToHTMLConverter);
#endif /* XP_UNIX */

  NET_RegisterContentTypeConverter (TEXT_HTML, FO_SAVE_AS_TEXT,
								    NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (TEXT_HTML, FO_QUOTE_MESSAGE,
								    NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (APPLICATION_HTTP_INDEX, FO_QUOTE_MESSAGE,
  									NULL, NET_HTTPIndexFormatToHTMLConverter);
  NET_RegisterContentTypeConverter (APPLICATION_HTTP_INDEX, FO_SAVE_AS_TEXT,
  									NULL, NET_HTTPIndexFormatToHTMLConverter);

#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (TEXT_HTML, FO_SAVE_AS_POSTSCRIPT,
									NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (APPLICATION_HTTP_INDEX,
 									FO_SAVE_AS_POSTSCRIPT,
  									NULL, NET_HTTPIndexFormatToHTMLConverter);
#endif /* XP_UNIX */

  /* And MDL too, sigh. */
  NET_RegisterContentTypeConverter (TEXT_MDL, FO_PRESENT,
									NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (TEXT_MDL, FO_SAVE_AS_TEXT,
								    NULL, INTL_ConvCharCode);
  NET_RegisterContentTypeConverter (TEXT_MDL, FO_QUOTE_MESSAGE,
								    NULL, INTL_ConvCharCode);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (TEXT_MDL, FO_SAVE_AS_POSTSCRIPT,
								    NULL, INTL_ConvCharCode);
#endif /* XP_UNIX */

  /* Convert the charsets of plain text into the canonical internal form.
   */
  NET_RegisterContentTypeConverter (TEXT_PLAIN, FO_PRESENT,
									NULL, NET_PlainTextConverter);
  NET_RegisterContentTypeConverter (TEXT_PLAIN, FO_EDIT,
									NULL, NET_PlainTextConverter);
  NET_RegisterContentTypeConverter (TEXT_PLAIN, FO_QUOTE_MESSAGE,
								    NULL, NET_PlainTextConverter);
	/* don't register TEXT_PLAIN for FO_SAVE_AS_TEXT
     * since it is already text
	 */
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (TEXT_PLAIN, FO_SAVE_AS_POSTSCRIPT,
									NULL, NET_PlainTextConverter);
#endif /* XP_UNIX */

  /* always treat unknown content types as text/plain */
  NET_RegisterContentTypeConverter (UNKNOWN_CONTENT_TYPE, FO_PRESENT,
									NULL, NET_PlainTextConverter);
  NET_RegisterContentTypeConverter (UNKNOWN_CONTENT_TYPE, FO_QUOTE_MESSAGE,
								    NULL, NET_PlainTextConverter);
  /* let mail view forms sent via web browsers */
  NET_RegisterContentTypeConverter (APPLICATION_WWW_FORM_URLENCODED, 
									FO_PRESENT,
									NULL, 
									NET_PlainTextConverter);

#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
#if defined(XP_MAC)
  NET_RegisterContentTypeConverter (MULTIPART_APPLEDOUBLE, FO_SAVE_AS,
								    NULL, fe_MakeAppleDoubleDecodeStream_1);
  NET_RegisterContentTypeConverter (APPLICATION_APPLEFILE, FO_SAVE_AS,
								    NULL, fe_MakeAppleSingleDecodeStream_1);
#endif
#if defined(XP_MAC) || defined(XP_UNIX) || defined(XP_PC)
  /* the new apple single/double and binhex decode.	20oct95 	*/
  NET_RegisterContentTypeConverter (APPLICATION_BINHEX, FO_PRESENT,
								    NULL, fe_MakeBinHexDecodeStream);
  NET_RegisterContentTypeConverter (MULTIPART_APPLEDOUBLE, FO_PRESENT,
								    NULL, fe_MakeAppleDoubleDecodeStream_1);
  NET_RegisterContentTypeConverter (MULTIPART_HEADER_SET, FO_PRESENT,
								    NULL, fe_MakeAppleDoubleDecodeStream_1);
  NET_RegisterContentTypeConverter (APPLICATION_APPLEFILE, FO_PRESENT,
								    NULL, fe_MakeAppleSingleDecodeStream_1);

  NET_RegisterContentTypeConverter (UUENCODE_APPLE_SINGLE, FO_PRESENT,
								    NULL, fe_MakeAppleSingleDecodeStream_1);
  NET_RegisterContentTypeConverter (UUENCODE_APPLE_SINGLE, FO_SAVE_AS,
								    NULL, fe_MakeAppleSingleDecodeStream_1);
								    
#endif /* XP_MAC || XP_UNIX */
#endif /* MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */
	/* don't register UNKNOWN_CONTENT_TYPE for FO_SAVE_AS_TEXT
     * since it is already text
	 */
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (UNKNOWN_CONTENT_TYPE,
									FO_SAVE_AS_POSTSCRIPT,
									NULL, NET_PlainTextConverter);
#endif /* XP_UNIX */


  /* Take the canonical internal form and do layout on it.
	 We do the same thing when the format_out is PRESENT or any of
	 the SAVE_AS types; the different behavior is gotten by the use
	 of a different context rather than a different stream.
   */
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_PRESENT,
									(void *) &parser_data, PA_BeginParseMDL);
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_PRESENT_INLINE,
									(void *) &parser_data, PA_BeginParseMDL);
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_SAVE_AS_TEXT,
									(void *) &parser_data, PA_BeginParseMDL);
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_QUOTE_MESSAGE,
									(void *) &parser_data, PA_BeginParseMDL);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_SAVE_AS_POSTSCRIPT,
									(void *) &parser_data, PA_BeginParseMDL);
#endif /* XP_UNIX */

  /* one for the editor */
  NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_EDIT,
									(void *) &parser_data, PA_BeginParseMDL);

  /* Note that we don't register a converter for "*" to FO_SAVE_AS,
	 because the FE needs to do that specially to set up an output file.
	 (The file I/O stuff for SAVE_AS_TEXT and SAVE_AS_POSTSCRIPT is dealt
	 with by libxlate.a, which could also handle SAVE_AS, but it's probably
	 not worth the effort.)
   */


  /* Do the same for the internally-handled image types when the format_out
	 is SAVE_AS_POSTSCRIPT, because the TEXT->PS code can handle that.  But
	 do not register converters to feed the image types to the HTML->TEXT
	 code, because that doesn't work.
   */
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (IMAGE_GIF, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
  NET_RegisterContentTypeConverter (IMAGE_JPG, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
  NET_RegisterContentTypeConverter (IMAGE_PJPG, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);

  NET_RegisterContentTypeConverter (IMAGE_PNG, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
  NET_RegisterContentTypeConverter (IMAGE_XBM, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
  NET_RegisterContentTypeConverter (IMAGE_XBM2, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
  NET_RegisterContentTypeConverter (IMAGE_XBM3, FO_SAVE_AS_POSTSCRIPT,
									NULL, IL_ViewStream);
#endif /* XP_UNIX */


  /* register default (non)decoders for the text printer
   */
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS_TEXT,
                                    NULL, NET_PrintRawToDisk);
  NET_RegisterContentTypeConverter ("*", FO_QUOTE_MESSAGE,
                                    NULL, NET_PrintRawToDisk);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS_POSTSCRIPT,
                                    NULL, NET_PrintRawToDisk);
#endif /* XP_UNIX */

  /* cache things */
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_PRESENT,
									NULL, NET_CacheConverter);

  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_PRESENT_INLINE,
									NULL, NET_CacheConverter);

  NET_RegisterContentTypeConverter ("*", FO_CACHE_ONLY,
									NULL, NET_CacheConverter);

  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_INTERNAL_IMAGE,
									NULL, NET_CacheConverter);

  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_SAVE_AS,
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_SAVE_AS_TEXT,
									NULL, NET_CacheConverter);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_SAVE_AS_POSTSCRIPT,
									NULL, NET_CacheConverter);
#endif /* XP_UNIX */
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_QUOTE_MESSAGE,
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_VIEW_SOURCE,
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_TO,
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_EDIT,
  									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_LOAD_HTML_HELP_MAP_FILE,
								   NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_SOFTUPDATE, 
									NULL, NET_CacheConverter);
#ifdef CRAWLER
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_CRAWL_PAGE, 
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_CRAWL_RESOURCE, 
									NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_ROBOTS_TXT, 
									NULL, NET_CacheConverter);
#endif /* CRAWLER */


#ifndef MCC_PROXY
  NET_RegisterContentTypeConverter(APPLICATION_NS_PROXY_AUTOCONFIG,
								   FO_CACHE_AND_PRESENT,
								   (void *)0, NET_ProxyAutoConfig);
  NET_RegisterContentTypeConverter(APPLICATION_NS_PROXY_AUTOCONFIG,
								   FO_PRESENT,
								   (void *)0, NET_ProxyAutoConfig);
  NET_RegisterContentTypeConverter(APPLICATION_NS_JAVASCRIPT_AUTOCONFIG,
								   FO_CACHE_AND_PRESENT,
								   (void *)0, NET_JavascriptConfig);
  NET_RegisterContentTypeConverter(APPLICATION_NS_JAVASCRIPT_AUTOCONFIG,
								   FO_PRESENT,
								   (void *)0, NET_JavascriptConfig);
#endif

  /*
   * call security library to register all security related content
   * type converters.
   */
  SECNAV_RegisterNetlibMimeConverters();
#ifdef JAVA
  {		/* stuff from ns/sun-java/netscape/net/netStubs.c */
	  extern void  NSN_RegisterJavaConverter(void);
	  NSN_RegisterJavaConverter();
  }
#endif /* JAVA */

  /* Register object data handler (see layobj.c) */
  NET_RegisterContentTypeConverter("*", FO_OBJECT, NULL, LO_NewObjectStream);
  NET_RegisterContentTypeConverter("*", FO_CACHE_AND_OBJECT, NULL, NET_CacheConverter);

#ifdef JAVA
  /* Castanet stuff can't work in non-java enabled environments */
  /* Register Marimba handler only if netcaster installed */
	if (FE_IsNetcasterInstalled()) {
		NET_RegisterContentTypeConverter(APPLICATION_MARIMBA, 
								   FO_PRESENT, 
								   NULL, 
								   NET_DoMarimbaApplication);

		NET_RegisterContentTypeConverter(APPLICATION_XMARIMBA, 
								   FO_PRESENT, 
								   NULL, 
								   NET_DoMarimbaApplication);
	}
#endif

    /* RDF */
    {
      /* XXX - move these to a header file soon */
NET_StreamClass * rdf_Converter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
NET_StreamClass * XML_XMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
NET_StreamClass * XML_CSSConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);
NET_StreamClass * XML_HTMLConverter(FO_Present_Types  format_out, void *data_object, URL_Struct *URL_s, MWContext  *window_id);

  NET_RegisterContentTypeConverter( "*", FO_CACHE_AND_RDF, NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter( "*", FO_RDF, NULL, rdf_Converter);

  /* XML for direct presentation */

  NET_RegisterContentTypeConverter (TEXT_XML, FO_CACHE_AND_PRESENT, NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter (TEXT_XML, FO_PRESENT, NULL, XML_XMLConverter);
  NET_RegisterContentTypeConverter (TEXT_XML, FO_EDIT, NULL, XML_XMLConverter);
  NET_RegisterContentTypeConverter ("*", FO_XMLHTML, NULL, XML_HTMLConverter);
  NET_RegisterContentTypeConverter ("*", FO_XMLCSS, NULL, XML_CSSConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_XMLHTML, NULL, NET_CacheConverter);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_XMLCSS, NULL, NET_CacheConverter);
    }

}


PRIVATE void
net_RegisterDefaultEncodingDecoders (void)
{
  /* Register the decompression content-encoding converters for those
	 types which are displayed internally. */
  NET_RegisterAllEncodingConverters (INTERNAL_PARSER, FO_PRESENT);
  NET_RegisterAllEncodingConverters (TEXT_HTML,       FO_PRESENT);
  NET_RegisterAllEncodingConverters (TEXT_MDL,        FO_PRESENT);
  NET_RegisterAllEncodingConverters (TEXT_PLAIN,      FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_GIF,       FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_JPG,       FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_PJPG,      FO_PRESENT);
  
  NET_RegisterAllEncodingConverters (IMAGE_PNG,      FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_XBM,       FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_XBM2,      FO_PRESENT);
  NET_RegisterAllEncodingConverters (IMAGE_XBM3,      FO_PRESENT);
  /* always treat unknown content types as text/plain */
  NET_RegisterAllEncodingConverters (UNKNOWN_CONTENT_TYPE, FO_PRESENT);

  /* when displaying anything in a news mime-multipart image
   * de compress it first
   */
  NET_RegisterAllEncodingConverters ("*",  FO_MULTIPART_IMAGE);

  /* When saving anything as Text or PostScript, it's necessary to
	 uncompress it first. */
  NET_RegisterAllEncodingConverters ("*",  FO_SAVE_AS_TEXT);
# ifdef XP_UNIX
  NET_RegisterAllEncodingConverters ("*",  FO_SAVE_AS_POSTSCRIPT);
# endif /* XP_UNIX */

  /* Whenever we save text or HTML to disk, we uncompress it first,
	 because the rules we decided upon is:

    - When Netscape saves text/plain or text/html documents to a
      user-specified file, they are always uncompressed first.

    - When Netscape saves any other kind of document to a user-specified
      file, it is always saved in its compressed state.

    - When Netscape saves a document to a temporary file before handing
      it off to an external viewer, it is always uncompressed first.
   */
  NET_RegisterAllEncodingConverters (TEXT_HTML,       FO_SAVE_AS);
  NET_RegisterAllEncodingConverters (TEXT_MDL,        FO_SAVE_AS);
  NET_RegisterAllEncodingConverters (TEXT_PLAIN,      FO_SAVE_AS);
  NET_RegisterAllEncodingConverters (INTERNAL_PARSER, FO_SAVE_AS);
  /* always treat unknown content types as text/plain */
  NET_RegisterAllEncodingConverters (UNKNOWN_CONTENT_TYPE, FO_SAVE_AS);
}


PUBLIC void
NET_RegisterMIMEDecoders (void)
{
  net_RegisterDefaultDecoders ();

  NET_RegisterEncodingConverter (ENCODING_GZIP,
                                 (void *) ENCODING_GZIP,
                                 NET_UnZipConverter);
  NET_RegisterEncodingConverter (ENCODING_GZIP2,
                                 (void *) ENCODING_GZIP2,
                                 NET_UnZipConverter);
  NET_RegisterEncodingConverter (ENCODING_QUOTED_PRINTABLE,
								 (void *) ENCODING_QUOTED_PRINTABLE,
								 NET_MimeEncodingConverter);
  NET_RegisterEncodingConverter (ENCODING_BASE64,
								 (void *) ENCODING_BASE64,
								 NET_MimeEncodingConverter);
#ifndef IRIX_STARTUP_SPEEDUPS
  NET_RegisterEncodingConverter (ENCODING_UUENCODE,
								 (void *) ENCODING_UUENCODE,
								 NET_MimeEncodingConverter);
  NET_RegisterEncodingConverter (ENCODING_UUENCODE2,
								 (void *) ENCODING_UUENCODE2,
								 NET_MimeEncodingConverter);
#endif
  NET_RegisterEncodingConverter (ENCODING_UUENCODE3,
								 (void *) ENCODING_UUENCODE3,
								 NET_MimeEncodingConverter);
#ifndef IRIX_STARTUP_SPEEDUPS
  NET_RegisterEncodingConverter (ENCODING_UUENCODE4,
								 (void *) ENCODING_UUENCODE4,
								 NET_MimeEncodingConverter);
#endif  

#if defined(SMART_MAIL) || defined(XP_UNIX)
  /* #### This should really be done all the time, because all versions of
     Mozilla should be able to sensibly display documents of type
     message/rfc822.  But I don't know what Makefile arcanity has happened on
     Windows, so for now, this only gets called if SMART_MAIL is defined, or
     if we're on Unix.  Someone please fix this on non-SmartMail Windows, and
     on Mac.
   */

  /* Decoders for libmime/mimemoz.c */
  MIME_RegisterConverters();
#endif /* SMART_MAIL */

#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
  /* Decoders for libmsg/compose.c */
  MSG_RegisterConverters ();
#endif  

  NET_RegisterUniversalEncodingConverter("chunked",
                                 NULL,
                                 NET_ChunkedDecoderStream);

  NET_RegisterContentTypeConverter("multipart/x-mixed-replace", 
								 FO_CACHE_AND_PRESENT, 
                                 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
                                 CV_MakeMultipleDocumentStream);
  NET_RegisterContentTypeConverter("multipart/x-mixed-replace", 
								 FO_CACHE_AND_PRINT, 
                                 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
                                 CV_MakeMultipleDocumentStream);
  NET_RegisterContentTypeConverter("multipart/x-mixed-replace", 
								 FO_CACHE_AND_INTERNAL_IMAGE, 
                                 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
                                 CV_MakeMultipleDocumentStream);
  NET_RegisterContentTypeConverter("multipart/x-byteranges", 
								 FO_CACHE_AND_PRESENT, 
                                 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
                                 CV_MakeMultipleDocumentStream);
  NET_RegisterContentTypeConverter("multipart/byteranges", 
								 FO_CACHE_AND_PRESENT, 
                                 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
                                 CV_MakeMultipleDocumentStream);
  NET_RegisterContentTypeConverter("multipart/mixed", 
						 		 FO_CACHE_AND_PRESENT, 
						 		 (void *) CVACTIVE_SIGNAL_AT_END_OF_MULTIPART, 
						 		 CV_MakeMultipleDocumentStream);

  net_RegisterDefaultEncodingDecoders ();
}


#if 0
void
main ()
{
  unsigned char *s;
  int32 i;
#define TEST(S) \
  /*MIME_EncodeBase64String*/ \
  MIME_EncodeQuotedPrintableString \
   (S, strlen(S), &s, &i); \
  printf ("---------\n" \
		  "%s\n" \
		  "---------\n" \
		  "%s\n" \
		  "---------\n", \
		  S, s); \
  free(s)

  TEST("ThisThisThisThis\n"
	   "This is a line with a c\238ntr\001l character in it.\n"
	   "This line has whitespace at EOL.    \n"
	   "This is a long line.  All work and no play makes Jack a dull boy.  "
	   "All work and no play makes Jack a dull boy.  "
	   "All work and no play makes Jack a dull boy.  "
	   "All work and no play makes Jack a dull boy.  \n"
	   "\n"
	   "Here's an equal sign: =\n"
	   "and here's a return\r..."
	   "Now make it realloc:"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
	   );
}
#endif

typedef struct _NET_PrintRawToDiskStruct {
    XP_File    fp;
    MWContext *context;
    int32      content_length;
    int32      bytes_read;
	int16	   mail_csid;
	int16	   win_csid;
	CCCDataObject conv;
	PRBool	   doConvert;
} NET_PrintRawToDiskStruct;

/* this converter uses the
 * (context->prSetup->filename) variable to
 * open the filename specified and save all data raw to
 * the file.
 */
PRIVATE
int
net_PrintRawToDiskWrite (NET_StreamClass *stream, CONST char *str, int32 len)
{
  NET_PrintRawToDiskStruct *obj = (NET_PrintRawToDiskStruct *) stream->data_object;
  char *newStr = NULL;
  int32 origLen = len;
  int32 rv = 0;

  if (obj->doConvert)
  {
	  char *dupStr = (char *) PR_Malloc(len+1);

	  if (dupStr)
	  {
		  memcpy(dupStr, str, len);
		  *(dupStr + len) = 0;
		  newStr = (char *) INTL_CallCharCodeConverter(obj->conv,
											  (unsigned char *)dupStr,
											  len);
		  if (!newStr)
			  newStr = dupStr;
		  else if (newStr != dupStr)
			  PR_Free(dupStr);

		  if (newStr)
		  {
			  str = newStr;
			  len = INTL_GetCCCLen(obj->conv);
		  }
	  }
  }

/*  int32 rv = fwrite ((char *) str, 1, len, obj->fp);*/
  rv = NET_XP_FileWrite(str, len, obj->fp);

  if (newStr)
	  PR_Free(newStr);

  obj->bytes_read += origLen;

  if (obj->content_length > 0)
    FE_SetProgressBarPercent (obj->context,
                  (obj->bytes_read * 100) /
                  obj->content_length);

  if(rv == len)
    return(0);
  else
    return(-1);
}

PRIVATE unsigned int
net_PrintRawToDiskIsWriteReady (NET_StreamClass *stream)
{
  return(MAX_WRITE_READY);
}

PRIVATE void
net_PrintRawToDiskComplete (NET_StreamClass *stream)
{
  NET_PrintRawToDiskStruct *obj = (NET_PrintRawToDiskStruct *) stream->data_object;

  if (obj->conv) {
	INTL_DestroyCharCodeConverter(obj->conv);
	obj->conv = NULL;
  }

  /* NET_XP_FileClose(obj->fp); DONT DO THIS the FE's do this */
}

PRIVATE void
net_PrintRawToDiskAbort (NET_StreamClass *stream, int status)
{
  NET_PrintRawToDiskStruct *obj = (NET_PrintRawToDiskStruct *) stream->data_object;

  if (obj->conv) {
	INTL_DestroyCharCodeConverter(obj->conv);
	obj->conv = NULL;
  }

  /* NET_XP_FileClose(obj->fp); DONT DO THIS the FE's do this */
}

/* this converter uses the
 * (context->prSetup->out) file pointer variable to
 * save all data raw to the file.
 */
PUBLIC NET_StreamClass *
NET_PrintRawToDisk(int format_out, 
				   void *data_obj,
                   URL_Struct *url_struct, 
				   MWContext *context)
{
  NET_PrintRawToDiskStruct *obj = PR_NEW(NET_PrintRawToDiskStruct);
  NET_StreamClass * stream;
  INTL_CharSetInfo csi = NULL;
  char *mime_charset = NULL;

  if(!obj)
	return(NULL);

  memset(obj, 0, sizeof(NET_PrintRawToDiskStruct));

  if(!context->prSetup || !context->prSetup->out)
	{
	  PR_Free(obj);
	  return NULL;
	}

#ifdef NOT /* file ptr should already be open in context->prSetup->out */
  if(!(obj->fp = NET_XP_FileOpen(context->prSetup->filename, 
						xpTemporary, 
						XP_FILE_WRITE_BIN)))
	{
	  PR_Free(obj);
	  return NULL;
	}
#endif

  obj->fp = context->prSetup->out;

  obj->context = context;
  obj->content_length = url_struct->content_length;

  stream = (NET_StreamClass *) PR_NEW(NET_StreamClass);
  if (!stream) 
	{
	  PR_Free(obj);
	  return NULL;
	}

  memset(stream, 0, sizeof(NET_StreamClass));

  stream->name           = "PrintRawToDisk";
  stream->complete       = net_PrintRawToDiskComplete;
  stream->abort          = net_PrintRawToDiskAbort;
  stream->put_block      = net_PrintRawToDiskWrite;
  stream->is_write_ready = net_PrintRawToDiskIsWriteReady;
  stream->data_object    = obj;
  stream->window_id      = context;

  csi = LO_GetDocumentCharacterSetInfo(context);
  mime_charset = INTL_GetCSIMimeCharset(csi);
  if (mime_charset && *(mime_charset))
  {
	  obj->conv = INTL_CreateCharCodeConverter();
	  obj->mail_csid = INTL_CharSetNameToID(mime_charset);
	  obj->win_csid = INTL_DocToWinCharSetID(obj->mail_csid);
		  
	  if (obj->conv)
		  obj->doConvert = INTL_GetCharCodeConverter(obj->mail_csid, 
													 obj->win_csid,
													 obj->conv);
  }

  return(stream);
}




#endif /*  MOZILLA_CLIENT */
