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
/* Please leave outside of ifdef for windows precompiled headers */
#include "xp.h"
#include "plstr.h"
#include "prmem.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#ifdef MOZILLA_CLIENT

/* take an HTML stream.  Escape all the HTML and
 * use <FONT color=> to color the different syntactical parts
 * of the HTML stream
 */
#include "xpgetstr.h"
#include "intl_csi.h"
#define VIEW_SOURCE_TARGET_WINDOW_NAME "%ViewSourceWindow"

typedef enum StatesEnum {
    IN_CONTENT,
    IN_SCRIPT,
    ABOUT_TO_BEGIN_TAG,
    IN_BEGIN_TAG,
    IN_TAG,
    BEGIN_ATTRIBUTE_VALUE,
    IN_QUOTED_ATTRIBUTE_VALUE,
    IN_BROKEN_QUOTED_ATTRIBUTE_VALUE,
    IN_UNQUOTED_ATTRIBUTE_VALUE,
    IN_COMMENT,
    IN_AMPERSAND_THINGY
} StatesEnum;

#define MAXTAGLEN 15

typedef struct _DataObject {
    NET_StreamClass * next_stream;
    StatesEnum state;
    char tag[MAXTAGLEN+1];
    uint tag_index;
    PRBool in_broken_html;
} DataObject;

#define BEGIN_TAG_MARKUP "<B>"
#define END_TAG_MARKUP "</B>"
#define BEGIN_TAG_NAME_MARKUP "<FONT SIZE=+0 COLOR=\"#551A8B\">"
#define END_TAG_NAME_MARKUP "</FONT>"
#define BEGIN_ATTRIBUTE_VALUE_MARKUP "</B><FONT SIZE=+0 COLOR=\"003E98\">"
#define END_ATTRIBUTE_VALUE_MARKUP "</FONT><B>"
#define BEGIN_BROKEN_ATTRIBUTE_MARKUP "<FONT COLOR=#0000FF><BLINK>"
#define END_BROKEN_ATTRIBUTE_MARKUP "</BLINK></FONT>"
#define BEGIN_COMMENT_MARKUP "<I>"
#define END_COMMENT_MARKUP "</I>"
#define BEGIN_AMPERSAND_THINGY_MARKUP "<FONT SIZE=+0 COLOR=\"#2F4F2F\">"
#define END_AMPERSAND_THINGY_MARKUP "</FONT>"

extern int MK_CVCOLOR_SOURCE_OF; 

PRIVATE char *net_BeginColorHTMLTag (DataObject *obj)
{
	char *new_markup = 0;

	StrAllocCat(new_markup, BEGIN_TAG_MARKUP);
	StrAllocCat(new_markup, "&lt;");
	StrAllocCat(new_markup, BEGIN_TAG_NAME_MARKUP);
	obj->state = ABOUT_TO_BEGIN_TAG;
	return new_markup;
}

PRIVATE char *net_EndColorHTMLTag (DataObject *obj)
{
	char *new_markup = 0;

	if(obj->in_broken_html)
	  {
       	StrAllocCopy(new_markup, END_BROKEN_ATTRIBUTE_MARKUP);
		obj->in_broken_html = PR_FALSE;
	  }
	StrAllocCat(new_markup, "&gt;");
	StrAllocCat(new_markup, END_TAG_MARKUP);
	return new_markup;
}

PRIVATE int net_ColorHTMLWrite (NET_StreamClass *stream, CONST char *s, int32 l)
{
	int32 i;
	int32 last_output_point;
	char *new_markup=0;
	char *tmp_markup=0;
	char  tiny_buf[4];
	CONST char *cp;
	int   status;
	DataObject *obj=stream->data_object;	

	last_output_point = 0;

	for(i = 0, cp = s; i < l; i++, cp++)
	  {
	    switch(obj->state)
	      {
		    case IN_CONTENT:
			    /* do nothing until you find a '<' "<!--" or '&' */
				if(*cp == '<')
				  {
					/* XXX we can miss a comment spanning a block boundary */
					if(i+4 <= l && !PL_strncmp(cp, "<!--", 4))
					  {
						StrAllocCopy(new_markup, BEGIN_COMMENT_MARKUP);
						StrAllocCat(new_markup, "&lt;");
						obj->state = IN_COMMENT;
					  }
					else
					  {
						new_markup = net_BeginColorHTMLTag(obj);
					  }
				  }
				else if(*cp == '&')
				  {
					StrAllocCopy(new_markup, BEGIN_AMPERSAND_THINGY_MARKUP);
					StrAllocCat(new_markup, "&amp;");
					obj->state = IN_AMPERSAND_THINGY;
				  }
			    break;
			case IN_SCRIPT:
				/* do nothing until you find '</SCRIPT>' */
				if(*cp == '<')
				  {
					/* XXX we can miss a </SCRIPT> spanning a block boundary */
					if(i+8 <= l && !PL_strncasecmp(cp, "</SCRIPT", 8))
					  {
						new_markup = net_BeginColorHTMLTag(obj);
					  }
				  }
				break;
		    case ABOUT_TO_BEGIN_TAG:
				/* we have seen the first '<'
				 * once we see a non-whitespace character
				 * we will be in the tag identifier
				 */
				if(*cp == '>')
				  {
					StrAllocCopy(new_markup, END_TAG_NAME_MARKUP);
					tmp_markup = net_EndColorHTMLTag(obj);
					StrAllocCat(new_markup, tmp_markup);
					PR_FREEIF(tmp_markup);
					tmp_markup = NULL;
				  }
				else if(!NET_IS_SPACE(*cp))
				  {
					obj->state = IN_BEGIN_TAG;
					obj->tag_index = 0;
					obj->tag[obj->tag_index++] = *cp;
					if(*cp == '<')
						StrAllocCopy(new_markup, "&lt;");

				  }
			    break;
		    case IN_BEGIN_TAG:
				/* go to the IN_TAG state when we see
				 * the first whitespace
				 */
				if(NET_IS_SPACE(*cp))
				  {
					StrAllocCopy(new_markup, END_TAG_NAME_MARKUP);
					sprintf(tiny_buf, "%c", *cp);
					StrAllocCat(new_markup, tiny_buf);
					obj->state = IN_TAG;
					obj->tag[obj->tag_index] = '\0';
				  }
				else if(*cp == '>')
				  {
					StrAllocCopy(new_markup, END_TAG_NAME_MARKUP);
					tmp_markup = net_EndColorHTMLTag(obj);
					StrAllocCat(new_markup, tmp_markup);
					PR_FREEIF(tmp_markup);
					tmp_markup = NULL;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					if(!obj->in_broken_html)
					  {
						obj->in_broken_html = PR_TRUE;
						StrAllocCopy(new_markup, BEGIN_BROKEN_ATTRIBUTE_MARKUP);
						StrAllocCat(new_markup, "&lt;");
					  }
					else
					  {
					    StrAllocCopy(new_markup, "&lt;");
					  }
				  }
				else
				  {
					if (obj->tag_index < MAXTAGLEN)
						obj->tag[obj->tag_index++] = *cp;
				  }
			    break;
		    case IN_TAG:
			    /* do nothing until you find a opening '=' or end '>' */
				if(*cp == '=')
				  {
					StrAllocCopy(new_markup, "=");
					StrAllocCat(new_markup, BEGIN_ATTRIBUTE_VALUE_MARKUP);
					obj->state = BEGIN_ATTRIBUTE_VALUE;
				  }
				else if(*cp == '>')
				  {
					new_markup = net_EndColorHTMLTag(obj);
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
			    break;
		    case BEGIN_ATTRIBUTE_VALUE:
				/* when we reach the first non-whitespace
				 * we will enter the UNQUOTED or the QUOTED
				 * ATTRIBUTE state
				 */
				if(!NET_IS_SPACE(*cp))
				  {
					if(*cp == '"')
                    {
		    			obj->state = IN_QUOTED_ATTRIBUTE_VALUE;
                        /* no need to jump to the quoted attr handler
                         * since this char can't be a dangerous char
                         */
                    }
					else
                    {
		    			obj->state = IN_UNQUOTED_ATTRIBUTE_VALUE;
                        /* need to jump to the unquoted attr handler
                         * since this char can be a dangerous character
                         */
                        goto unquoted_attribute_jump_point;
                    }
				  }
				else if(*cp == '>')
				  {
					StrAllocCopy(new_markup, END_ATTRIBUTE_VALUE_MARKUP);
					tmp_markup = net_EndColorHTMLTag(obj);
					StrAllocCat(new_markup, tmp_markup);
					PR_FREEIF(tmp_markup);
					tmp_markup = NULL;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
			    break;
		    case IN_UNQUOTED_ATTRIBUTE_VALUE:
unquoted_attribute_jump_point:
			    /* do nothing until you find a whitespace */
				if(NET_IS_SPACE(*cp))
				  {
					StrAllocCopy(new_markup, END_ATTRIBUTE_VALUE_MARKUP);
					sprintf(tiny_buf, "%c", *cp);
					StrAllocCat(new_markup, tiny_buf);
					obj->state = IN_TAG;
				  }
				else if(*cp == '>')
				  {
					StrAllocCopy(new_markup, END_ATTRIBUTE_VALUE_MARKUP);
					tmp_markup = net_EndColorHTMLTag(obj);
					StrAllocCat(new_markup, tmp_markup);
					PR_FREEIF(tmp_markup);
					tmp_markup = NULL;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
				else if(*cp == '&')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&amp;");
				  }
			    break;
		    case IN_QUOTED_ATTRIBUTE_VALUE:
			    /* do nothing until you find a closing '"' */
				if(*cp == '\"')
				  {
					if(obj->in_broken_html)
					  {
                    	StrAllocCopy(new_markup, END_BROKEN_ATTRIBUTE_MARKUP);
						obj->in_broken_html = PR_FALSE;
					  }
					StrAllocCat(new_markup, "\"");
					StrAllocCat(new_markup, END_ATTRIBUTE_VALUE_MARKUP);
					obj->state = IN_TAG;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
				else if(*cp == '&')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&amp;");
				  }
				else if(*cp == '>')
				  {
					/* probably a broken attribute value */
					if(!obj->in_broken_html)
					  {
						obj->in_broken_html = PR_TRUE;
						StrAllocCopy(new_markup, BEGIN_BROKEN_ATTRIBUTE_MARKUP);
						StrAllocCat(new_markup, ">");
					  }
				  }
			    break;
			case IN_COMMENT:
			    /* do nothing until you find a closing '-->' */
				if(!PL_strncmp(cp, "-->", 3))
				  {
					StrAllocCopy(new_markup, "&gt;");
					cp += 2;
					i += 2;
					StrAllocCat(new_markup, END_COMMENT_MARKUP);
					obj->state = IN_CONTENT;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
			    break;
			case IN_AMPERSAND_THINGY:
			    /* do nothing until you find a ';' or space */
				if(*cp == ';' || NET_IS_SPACE(*cp))
				  {
					sprintf(tiny_buf, "%c", *cp);
					StrAllocCopy(new_markup, tiny_buf);
					StrAllocCat(new_markup, END_AMPERSAND_THINGY_MARKUP);
					obj->state = IN_CONTENT;
				  }
				else if(*cp == '<')
				  {
					/* protect ourselves from markup */
					StrAllocCopy(new_markup, "&lt;");
				  }
			    break;
		    default:
			    PR_ASSERT(0);
			    break;
		  }

		if(new_markup)
		  {
			/* push all the way up to but not including *cp */
			status = (*obj->next_stream->put_block)
											(obj->next_stream,
    										&s[last_output_point],
    										i-last_output_point);
    		last_output_point = i+1;

			if(status < 0)
			  {
				PR_Free(new_markup);
				return(status);
			  }

			/* add new markup */
    		status = (*obj->next_stream->put_block)
											(obj->next_stream,
        									new_markup, PL_strlen(new_markup));
			if(status < 0)
			  {
				PR_Free(new_markup);
				return(status);
			  }

    		PR_FREEIF(new_markup);
    		new_markup = NULL;
		  }
	  }

	if(last_output_point < l)
		return((*obj->next_stream->put_block)(obj->next_stream,
    									  &s[last_output_point],
    									  (l-last_output_point)));
	else
		return(0);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_ColorHTMLWriteReady (NET_StreamClass * stream)
{
	DataObject *obj=stream->data_object;
	return((*obj->next_stream->is_write_ready)(obj->next_stream));
}


PRIVATE void net_ColorHTMLComplete (NET_StreamClass *stream)
{
	DataObject *obj=stream->data_object;	
	(*obj->next_stream->complete)(obj->next_stream);
}

PRIVATE void net_ColorHTMLAbort (NET_StreamClass *stream, int status)
{
	DataObject *obj=stream->data_object;	
	(*obj->next_stream->abort)(obj->next_stream, status);
}

PUBLIC NET_StreamClass *
net_ColorHTMLStream (int         format_out,
                     void       *data_obj,
                     URL_Struct *URL_s,
                     MWContext  *window_id)
{
    DataObject* obj;
	char *new_markup=0;
	char *new_url=0;
	char *old_url;
	int status, type;
	NET_StreamClass *next_stream, *new_stream;
	PRBool is_html_stream = PR_FALSE;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(window_id);
	INTL_CharSetInfo next_csi;

    TRACEMSG(("Setting up ColorHTML stream. Have URL: %s\n", URL_s->address));

	/* treat the stream as html if the closure data says
	 * it's HTML and it is also not a mail or news message
	 */
	type = NET_URL_Type(URL_s->address);
	if(data_obj 
		&& !PL_strcmp((char *)data_obj, TEXT_HTML)
		&& type != MAILBOX_TYPE_URL
		&& type != IMAP_TYPE_URL
		&& type != NEWS_TYPE_URL)
		is_html_stream = PR_TRUE;

	/* use a new named window */
	StrAllocCopy(URL_s->window_target, VIEW_SOURCE_TARGET_WINDOW_NAME);

	/* add the url address to the name so that there can be
     * one view source window per url
	 */
	StrAllocCat(URL_s->window_target, URL_s->address);

    /* zero position_tag to prevent hash lossage */
    URL_s->position_tag = 0;

	/* alloc a new chrome struct and stick it in the URL
  	 * so that we can turn off the relavent stuff
	 */
	URL_s->window_chrome = PR_NEW(Chrome);
	if(URL_s->window_chrome)
	  {
		/* zero everything to turn off all chrome */
		memset(URL_s->window_chrome, 0, sizeof(Chrome));
		URL_s->window_chrome->type = MWContextDialog;
		URL_s->window_chrome->show_scrollbar = PR_TRUE;
		URL_s->window_chrome->allow_resize = PR_TRUE;
		URL_s->window_chrome->allow_close = PR_TRUE;
	  }

	/* call the HTML parser */
	StrAllocCopy(URL_s->content_type, INTERNAL_PARSER);

	/* use the view-source: url instead */
	StrAllocCopy(new_url, VIEW_SOURCE_URL_PREFIX);
	StrAllocCat(new_url, URL_s->address);
	old_url = URL_s->address;
	URL_s->address = new_url;

	format_out = FO_PRESENT;

	/* open next stream */
	next_stream = NET_StreamBuilder(format_out, URL_s, window_id);

	if(!next_stream)
	  {
		PR_Free(old_url);
		return(NULL);
	  }
	next_csi = LO_GetDocumentCharacterSetInfo(next_stream->window_id);

	/* jliu: for international's reason,
		set the value ASAP, so the following stream can share it */
	INTL_SetCSIWinCSID(next_csi, INTL_GetCSIWinCSID(csi));
	INTL_SetCSIDocCSID(next_csi, INTL_GetCSIDocCSID(csi));


#define DEF_PICS_LABEL "<META http-equiv=PICS-Label content='(PICS-1.0 \"http://home.netscape.com/default_rating\" l gen true r (s 0))'>"

	/* add a PICS label */
	StrAllocCopy(new_markup, DEF_PICS_LABEL);
	StrAllocCat(new_markup, "<TITLE>");
	StrAllocCat(new_markup, XP_GetString(MK_CVCOLOR_SOURCE_OF));
	StrAllocCat(new_markup, old_url);
	StrAllocCat(new_markup, "</TITLE><BODY BGCOLOR=#C0C0C0>");


	if(!is_html_stream)
		StrAllocCat(new_markup, "<PLAINTEXT>");
	else
		StrAllocCat(new_markup, "<PRE>");

	PR_Free(old_url);

  	status = (*next_stream->put_block)(next_stream,
        									new_markup,
        									PL_strlen(new_markup));
	PR_Free(new_markup);

	if(status < 0)
	  {
  		(*next_stream->abort)(next_stream, status);
		PR_Free(next_stream);
		return(NULL);
	  }

	if(!is_html_stream)
		return(next_stream);

	/* else; continue on and build up this stream module
	 * and attach the next stream to it
	 */

    new_stream = PR_NEW(NET_StreamClass);
    if(new_stream == NULL)
	  {
  		(*next_stream->abort)(next_stream, status);
		PR_Free(next_stream);
        return(NULL);
	  }

    obj = PR_NEW(DataObject);

    if (obj == NULL)
	  {
  		(*next_stream->abort)(next_stream, status);
		PR_Free(next_stream);
		PR_Free(new_stream);
        return(NULL);
	  }

	memset(obj, 0, sizeof(DataObject));

	obj->state = IN_CONTENT;

	obj->next_stream = next_stream;

    new_stream->name           = "HTML Colorer";
    new_stream->complete       = (MKStreamCompleteFunc) net_ColorHTMLComplete;
    new_stream->abort          = (MKStreamAbortFunc) net_ColorHTMLAbort;
    new_stream->put_block      = (MKStreamWriteFunc) net_ColorHTMLWrite;
    new_stream->is_write_ready = (MKStreamWriteReadyFunc)
													net_ColorHTMLWriteReady;
    new_stream->data_object    = (void *) obj;  /* document info object */
    new_stream->window_id      = window_id;

    TRACEMSG(("Returning stream from HTMLColorConverter\n"));

    return new_stream;
}
#endif /* MOZILLA_CLIENT */
