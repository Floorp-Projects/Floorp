#include "rosetta.h"
#include "xp.h"
#include "net.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "netcache.h"
#include "xpgetstr.h"
#include "xplocale.h"
#include "prefapi.h"
#include "secnav.h"
#ifndef MODULAR_NETLIB
#include "libimg.h"
#endif
#include "il_strm.h"
#include "cookies.h"
#include "httpauth.h"
#include "mkstream.h"
#include "glhist.h"
#include "shist.h"
#include "prmem.h"
#include "plstr.h"

#include "abouturl.h"

/* XXX ugh.  FE_GetNetDir returns XP_STRDUP'd result, must be XP_FREE'd */
#include "xp_mem.h"

extern int XP_NETSITE_  ;
extern int XP_LOCATION_ ;
extern int XP_FILE_MIME_TYPE_   ;
extern int XP_CURRENTLY_UNKNOWN;
extern int XP_SOURCE_ ;
extern int XP_CURRENTLY_IN_DISK_CACHE   ;
extern int XP_CURRENTLY_IN_MEM_CACHE;
extern int XP_CURRENTLY_NO_CACHE        ;
extern int XP_LOCAL_CACHE_FILE_ ;
extern int XP_NONE      ;
extern int XP_LOCAL_TIME_FMT ;
extern int XP_LAST_MODIFIED ;
extern int XP_GMT_TIME_FMT      ;
extern int XP_CONTENT_LENGTH_   ;
extern int XP_NO_DATE_GIVEN;
extern int XP_EXPIRES_  ;
extern int XP_MAC_TYPE_ ;
extern int XP_MAC_CREATOR_      ;
extern int XP_CHARSET_  ;
extern int XP_STATUS_UNKNOWN ;
extern int XP_MSG_UNKNOWN ;
HG42784
extern int XP_UNTITLED_DOCUMENT ;
extern int XP_HAS_THE_FOLLOWING_STRUCT  ;
extern int XP_DOCUMENT_INFO ;
extern int XP_THE_WINDOW_IS_NOW_INACTIVE ;
extern int MK_MALFORMED_URL_ERROR;


PRIVATE void
net_OutputURLDocInfo(MWContext *ctxt, char *which, char **data, int32 *length)
{
	char *output=0;
	URL_Struct *URL_s = NET_CreateURLStruct(which, NET_DONT_RELOAD);
	struct tm *tm_struct_p;
	char buf[64];
	char *tmp=0;
	char *sec_msg, *il_msg;

	NET_FindURLInCache(URL_s, ctxt);

#define CELL_TOP                                                        \
	StrAllocCat(output,                                     \
				"<TR><TD VALIGN=BASELINE ALIGN=RIGHT><B>");     
#define CELL_TITLE(title)                                       \
	StrAllocCat(output, title);
#define CELL_MIDDLE                                                     \
	StrAllocCat(output,                                     \
				"</B></TD>"                                     \
				"<TD>");
#define CELL_BODY(body)                                         \
	StrAllocCat(output, body);
#define CELL_END                                                        \
	StrAllocCat(output,                                     \
				"</TD></TR>");
#define ADD_CELL(c_title, c_body)                       \
	CELL_TOP;                                                               \
	CELL_TITLE(c_title);                                    \
	CELL_MIDDLE;                                                    \
	CELL_BODY(c_body);                                              \
	CELL_END;

	StrAllocCopy(output, "<TABLE>");

	StrAllocCopy(tmp, "<A HREF=\"");
	StrAllocCat(tmp, URL_s->address);
	StrAllocCat(tmp, "\">");
	StrAllocCat(tmp, URL_s->address);
	StrAllocCat(tmp, "</a>");
	if(URL_s->is_netsite)
	  {
		ADD_CELL(XP_GetString(XP_NETSITE_), tmp);
	  }
	else
	  {
		ADD_CELL(XP_GetString(XP_LOCATION_), tmp);
	  }
	PR_Free(tmp);

	ADD_CELL(XP_GetString(XP_FILE_MIME_TYPE_), 
			 URL_s->content_type ? URL_s->content_type : XP_GetString(XP_CURRENTLY_UNKNOWN)); 
	ADD_CELL(XP_GetString(XP_SOURCE_), URL_s->cache_file
							? XP_GetString(XP_CURRENTLY_IN_DISK_CACHE)
							  : URL_s->memory_copy ? 
								XP_GetString(XP_CURRENTLY_IN_MEM_CACHE) 
								: XP_GetString(XP_CURRENTLY_NO_CACHE));

	ADD_CELL(XP_GetString(XP_LOCAL_CACHE_FILE_), URL_s->cache_file 
								 ? URL_s->cache_file
									:  XP_GetString(XP_NONE));

	tm_struct_p = localtime(&URL_s->last_modified);

	if(tm_struct_p)
	  {
#ifdef XP_UNIX
		strftime(buf, 64, XP_GetString(XP_LOCAL_TIME_FMT),
									   tm_struct_p);
#else
       char tmpbuf[64];
       XP_StrfTime(ctxt,tmpbuf,64,XP_LONG_DATE_TIME_FORMAT,
		   tm_struct_p);
       /* NOTE: XP_LOCAL_TIME_FMT is different depend on platform */
       PR_snprintf(buf, 64, XP_GetString(XP_LOCAL_TIME_FMT), tmpbuf);
#endif
      }
	else
	  {
		buf[0] = 0;
	  }
	ADD_CELL(XP_GetString(XP_LAST_MODIFIED), URL_s->last_modified ? buf :  XP_GetString(XP_MSG_UNKNOWN));

	tm_struct_p = gmtime(&URL_s->last_modified);

	if(tm_struct_p)
	  {
#ifdef XP_UNIX
		strftime(buf, 64, XP_GetString(XP_GMT_TIME_FMT),
									   tm_struct_p);
#else
       char tmpbuf[64];
       XP_StrfTime(ctxt,tmpbuf,64,XP_LONG_DATE_TIME_FORMAT,
				   tm_struct_p);
       /* NOTE: XP_GMT_TIME_FMT is different depend on platform */
       PR_snprintf(buf, 64, XP_GetString(XP_GMT_TIME_FMT), tmpbuf);
#endif
	  }
	else
	  {
		buf[0] = 0;
	  }

	ADD_CELL(XP_GetString(XP_LAST_MODIFIED), URL_s->last_modified ? buf :  XP_GetString(XP_MSG_UNKNOWN));

#ifdef DEBUG
	PR_snprintf(buf, 64, "%lu seconds since 1-1-70 GMT", URL_s->last_modified);
	ADD_CELL("Last Modified:", URL_s->last_modified ? buf :  "Unknown");
#endif

	sprintf(buf, "%lu", URL_s->content_length);
	ADD_CELL(XP_GetString(XP_CONTENT_LENGTH_), URL_s->content_length 
								 ? buf
									:  XP_GetString(XP_MSG_UNKNOWN));
	ADD_CELL(XP_GetString(XP_EXPIRES_), URL_s->expires > 0
								 ? INTL_ctime(ctxt, &URL_s->expires)
									:  XP_GetString(XP_NO_DATE_GIVEN));

	if (URL_s->x_mac_type && *URL_s->x_mac_type)
	  {
		ADD_CELL(XP_GetString(XP_MAC_TYPE_), URL_s->x_mac_type);
	  }
	if (URL_s->x_mac_creator && *URL_s->x_mac_creator)
	  {
		ADD_CELL(XP_GetString(XP_MAC_CREATOR_), URL_s->x_mac_creator);
	  }

	if (URL_s->charset)
	  {
		ADD_CELL(XP_GetString(XP_CHARSET_), URL_s->charset);
	  }
	else 
	  {
		ADD_CELL(XP_GetString(XP_CHARSET_), XP_GetString(XP_MSG_UNKNOWN));
	  }

	HG76363

	StrAllocCat(output, "</TABLE>");

	if(URL_s->content_type 
		&& !PL_strncasecmp(URL_s->content_type, "image", 5))
	  {
	/* Have a seat. Lie down.  Tell me about yourself. */
#ifdef MODULAR_NETLIB
	il_msg = NULL;
#else
	il_msg = IL_HTMLImageInfo(URL_s->address);
#endif
	if (il_msg) {
	  StrAllocCat(output, "<HR>\n");
	  StrAllocCat(output, il_msg);
	  PR_Free(il_msg);
	}
	  }
		
	*data = output;
	*length = PL_strlen(output);

	NET_FreeURLStruct(URL_s);
	return;
}

/* prints information about a context usint HTML.
 *
 * The context passed in as arg2 is the context that
 * we are printing info about and the context that
 * is in the ActiveEntry is the window we are displaying
 * within
 */
PRIVATE
int
net_PrintContextInfo(ActiveEntry *cur_entry, MWContext *context)
{
	History_entry *his;
	NET_StreamClass *stream;
	char buf[64];
	char *tmp;

    /* get the current history entry and
     * extract the URL from it.  That has
     * the top level document URL and the title
     */
    his = SHIST_GetCurrent (&context->hist);
   
	cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
	StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);
    stream = NET_StreamBuilder(cur_entry->format_out,
				   cur_entry->URL_s, cur_entry->window_id);

    if(!stream)
	return(MK_UNABLE_TO_CONVERT);

    PL_strcpy(buf, "<FONT SIZE=+2><b>");
	(*stream->put_block)(stream, buf, PL_strlen(buf));

    if(his && his->title)
		tmp = PL_strdup(his->title);
	else
		tmp = PL_strdup(XP_GetString(XP_UNTITLED_DOCUMENT));

	(*stream->put_block)(stream, tmp, PL_strlen(tmp));

    PL_strcpy(buf, XP_GetString(XP_HAS_THE_FOLLOWING_STRUCT));
	(*stream->put_block)(stream, buf, PL_strlen(buf));

	LO_DocumentInfo(context, stream);

	(*stream->complete)(stream);
	cur_entry->status = MK_DATA_LOADED;
	cur_entry->status = MK_DATA_LOADED;

	return(-1);
}

PRIVATE char * 
net_gen_pics_document(ActiveEntry *cur_entry)
{
    char *rv = NULL;
    char *help_dir = FE_GetNetHelpDir();

    if(help_dir)
    {
		if (help_dir[PL_strlen(help_dir)-1] != '/')
			StrAllocCat(help_dir, "/");
		
		if (!help_dir)
			return NULL;
			
#define PICS_HTML " \
<TITLE>XXXXXXXXXXXXXXX</TITLE> \
<FRAMESET border=no cols=1,100%%> \
<FRAME src=about:blank> \
<FRAME src='%spicsfail.htm'> \
</FRAMESET>"

	rv = PR_smprintf(PICS_HTML, help_dir);

	XP_FREE(help_dir);
    }


    return rv;
}

/* print out silly about type URL
 */
PRIVATE int net_output_about_url(ActiveEntry * cur_entry)
{
    NET_StreamClass * stream;
	char * data=0;
	char * content_type=0;
	int32 length;
	char * which = cur_entry->URL_s->address;
	char * colon = strchr (which, ':');
	void * fe_data = NULL;
	Bool   uses_fe_data=FALSE;

	
	if (colon)
	  which = colon + 1;

	if(0)
	  {
		/* placeholder to make ifdef's easier */
	  }
#ifdef MOZILLA_CLIENT
	else if(NET_URL_Type(which))
	  {
	    /* this url is asking for document info about this
		 * URL.
		 * Call a NET function to generate the HTML block
		 */
		net_OutputURLDocInfo(cur_entry->window_id, which, &data, &length);
		StrAllocCopy(content_type, TEXT_HTML);
	  }
	else if(!PL_strncasecmp(which, "pics", 4))
	  {
		data = net_gen_pics_document(cur_entry);
   		if (data) {
			length = PL_strlen(data);
			content_type = PL_strdup(TEXT_HTML);
            uses_fe_data = FALSE;
        } 
	  }
	else if(!PL_strcasecmp(which, "cookies"))
	{
		NET_DisplayCookieInfoAsHTML(cur_entry);
		return(-1);
	}
#if defined(SingleSignon)
        else if(!PL_strcasecmp(which, "signons"))
	{
		extern void SI_DisplaySignonInfoAsHTML(ActiveEntry * cur_entry);
		SI_DisplaySignonInfoAsHTML(cur_entry);
		return(-1);
	}
#endif
	else if(!PL_strncasecmp(which, "cache", 5))
	  {
		NET_DisplayCacheInfoAsHTML(cur_entry);
		return(-1);
	  }
	else if(!PL_strcasecmp(which, "memory-cache"))
	  {
		NET_DisplayMemCacheInfoAsHTML(cur_entry);
		return(-1);
	  }
	else if(!PL_strcasecmp(which, "logout"))
	  {
		NET_RemoveAllAuthorizations();
		return(-1);
	  }
#ifndef MODULAR_NETLIB
	else if(!PL_strcasecmp(which, "image-cache"))
	  {
	IL_DisplayMemCacheInfoAsHTML(cur_entry->format_out, cur_entry->URL_s,
				     cur_entry->window_id);
		return(-1);
	  }
#endif
#ifdef DEBUG
    else if(!PL_strcasecmp(which, "streams"))
      {
	NET_DisplayStreamInfoAsHTML(cur_entry);
	return(-1);
      }
#endif /* DEBUG */
	else if(!PL_strcasecmp(which, "document"))
	  {
		char buf[64];
		History_entry *his;
		MWContext *context;

		/* output a Grid here with the first grid
		 * cell containing an about: url with the
		 * context pointer as the argument.
		 * 
		 * also put the context pointer in as the post data
		 * if it's not there already so that history will
		 * do the right thing.
		 *
		 * also output blank string for all FO's not view source
		 * and save
		 */

		/* if there is post data then a context ID is hiding
		 * in there.  Get it and verify it
		 */
		if(cur_entry->URL_s->post_data)
		  {
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
			sscanf(cur_entry->URL_s->post_data, "%lu", &context);
#else
			sscanf(cur_entry->URL_s->post_data, "%p", &context);
#endif
			if(!XP_IsContextInList(context))
				context = cur_entry->window_id;
		  }
		else
		  {
			context = cur_entry->window_id;
		  }
			
		/* get the current history entry and
		 * extract the URL from it.  That has
		 * the top level document URL 
		 */
		his = SHIST_GetCurrent (&context->hist);

		StrAllocCat(data, "<TITLE>");
		StrAllocCat(data, XP_GetString(XP_DOCUMENT_INFO));
		StrAllocCat(data,       "</TITLE>"
							"<frameset rows=\"40%,60%\">"
							"<frame src=about:");

		StrAllocCopy(cur_entry->URL_s->window_target, "%DocInfoWindow");
		/* add a Chrome Struct so that we can control the window */
		cur_entry->URL_s->window_chrome = PR_NEW(Chrome);

		if(cur_entry->URL_s->window_chrome)
		  {
			memset(cur_entry->URL_s->window_chrome, 0, sizeof(Chrome));
			/* it should be MWContextDialog
			 * but X isn't ready for it yet...
			 */
/* X is ready for MWContextDialog now...
 *#ifdef XP_UNIX
 *                      cur_entry->URL_s->window_chrome->type = MWContextBrowser;
 *#else
 */
			cur_entry->URL_s->window_chrome->type = MWContextDialog;
/*
 *#endif
 */
			cur_entry->URL_s->window_chrome->show_scrollbar = TRUE;
			cur_entry->URL_s->window_chrome->allow_resize = TRUE;
			cur_entry->URL_s->window_chrome->allow_close = TRUE;
		  }

		/* don't let people really view the source of this... */
		if(FO_PRESENT != CLEAR_CACHE_BIT(cur_entry->format_out))        
		  {
			StrAllocCat(data, "blank");
		  }
		else
		  {
			StrAllocCat(data, "FeCoNtExT=");

		    if(!cur_entry->URL_s->post_data)
		      {
			    PR_snprintf(buf, 64, "%p", context);
			    StrAllocCat(data, buf);
			    StrAllocCopy(cur_entry->URL_s->post_data, buf);
		      }
		    else
		      {
			    StrAllocCat(data, cur_entry->URL_s->post_data);
		      }
		  }

		StrAllocCat(data, "><frame name=Internal_URL_Info src=about:");
		
		if(his && his->address)
			StrAllocCat(data, his->address);
		else
			StrAllocCat(data, "blank");

		StrAllocCat(data, "></frameset>");

		length = PL_strlen(data);
		StrAllocCopy(content_type, TEXT_HTML);
	  }
	else if(!PL_strncasecmp(which, "Global", 6))
	  {
		cur_entry->status = NET_DisplayGlobalHistoryInfoAsHTML(
													cur_entry->window_id,
									cur_entry->URL_s,
									cur_entry->format_out);

		return(-1);

	  }
	else if(!PL_strncmp(which, "FeCoNtExT=", 10))
	  {
		MWContext *old_ctxt;
		
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
		sscanf(which+10, "%lu", &old_ctxt);
#else
		sscanf(which+10, "%p", &old_ctxt);
#endif

		/* verify that we have the right context 
		 *
		 */
		if(!XP_IsContextInList(old_ctxt))
		  {
			StrAllocCopy(data, XP_GetString(XP_THE_WINDOW_IS_NOW_INACTIVE));
		  }
		else    
		  {
			return(net_PrintContextInfo(cur_entry, old_ctxt));
		  }             
    
	    length = PL_strlen(data);
	    StrAllocCopy(content_type, TEXT_HTML);

      }
#endif /* MOZILLA_CLIENT */
	else if(!PL_strncasecmp(which, "authors", 7))
	  {
		static const char *d =
		  ("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html\" "
		   "CHARSET=\"iso-8859-1\"><TITLE>Mozilla &Uuml;ber Alles</TITLE>"
		   "<BODY BGCOLOR=\"#C0C0C0\" TEXT=\"#000000\">"
		   "<TABLE width=\"100%\" height=\"100%\"><TR><TD ALIGN=CENTER>"
		   "<FONT SIZE=5><FONT COLOR=\"#0000EE\">about:authors</FONT> "
		   "removed.</FONT></TD></TR></TABLE>");
	    data = PL_strdup(d);
		length = (d ? PL_strlen(d) : 0);
	    content_type = PL_strdup(TEXT_HTML);
	    uses_fe_data = FALSE;
	  }
	/* Admin Kit/xp prefs diagnostic support */
	else if (!PL_strncasecmp(which, "config", 6))
	{
		data = PREF_AboutConfig();
		if (data) {
			length = PL_strlen(data);
			content_type = PL_strdup(TEXT_HTML);
		}
	}
#if defined(XP_UNIX) && !defined(MODULAR_NETLIB)
	else if (!PL_strncasecmp(which, "minibuffer", 10))
	{
		extern void FE_ShowMinibuffer(MWContext *);

		FE_ShowMinibuffer(cur_entry->window_id);
	}
#endif
#ifdef WEBFONTS
	else if (!PL_strncasecmp(which, "fonts", 5))
	{
		NF_AboutFonts(cur_entry->window_id, which);
		return (-1);
	}
#endif /* WEBFONTS */
    else
      {
	    fe_data = FE_AboutData(which, &data, &length, &content_type);
	    uses_fe_data=TRUE;
      }

	if (!data || !content_type)
	  {
		cur_entry->status = MK_MALFORMED_URL_ERROR;
	  }
	else
	  {
		int status;
		StrAllocCopy(cur_entry->URL_s->content_type, content_type);

		cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);

		stream = NET_StreamBuilder(cur_entry->format_out,
								   cur_entry->URL_s, cur_entry->window_id);

		if(!stream)
		  {
			cur_entry->status = MK_UNABLE_TO_CONVERT;
			return(MK_UNABLE_TO_CONVERT);
		  }

		status = (*stream->put_block)(stream, data, length);

		if (status >= 0) {
			/* Append optional Admin Kit-specified about page text */
			char* custom_text = NULL;
			if (PL_strcmp(which, "") == 0 &&
				PREF_CopyConfigString("about_text", &custom_text) == PREF_NOERROR) {
				(*stream->put_block)(stream, custom_text, PL_strlen(custom_text));
			}
		
			(*stream->complete) (stream);
			
			PR_FREEIF(custom_text);
		}
		else
		  (*stream->abort) (stream, status);

		cur_entry->status = status;

		PR_Free(stream);
	  }

	if(uses_fe_data)
	  {
#if defined(XP_MAC) || defined(XP_UNIX) || defined (XP_WIN)
		FE_FreeAboutData (fe_data, which);
#endif /* XP_MAC || XP_UNIX */
	  }
	else
	  {
		PR_Free(data);
		PR_Free(content_type);
	  }


    return(-1);
}


PRIVATE Bool net_about_kludge(URL_Struct *URL_s)
{
  unsigned char *url = (unsigned char *) URL_s->address;
  unsigned char *user = (unsigned char *) PL_strdup ((char*)(url + 6));
  unsigned char *tmp;
 
  if (user == NULL)
	return FALSE;

  for (tmp = user; *tmp; tmp++) *tmp += 23;

  if (	 !PL_strcmp((char*)user, "\170\202\202\170\205\170") ||	/* akkana */
	 !PL_strcmp((char*)user, "\170\211\200") ||				/* ari */
	 !PL_strcmp((char*)user, "\170\211\212\177\170\173") ||			/* arshad */
	 !PL_strcmp((char*)user, "\170\213\206\213\200\172") ||			/* atotic */
	 !PL_strcmp((char*)user, "\171\176\170\220") ||				/* bgay */
	 !PL_strcmp((char*)user, "\171\201\206\205\174\212") ||			/* bjones */
	 !PL_strcmp((char*)user, "\171\203\220\213\177\174") ||			/* blythe */
	 !PL_strcmp((char*)user, "\171\205\174\203\212\206\205") ||		/* bnelson */
	 !PL_strcmp((char*)user, "\171\206\171") ||				/* bob */
	 !PL_strcmp((char*)user, "\171\206\171\201") ||				/* bobj */
	 !PL_strcmp((char*)user, "\171\211\170\173\174") ||			/* brade */
	 !PL_strcmp((char*)user, "\171\211\200\170\205\206") ||			/* briano */
	 !PL_strcmp((char*)user, "\171\211\200\173\176\174") ||			/* bridge */
	 !PL_strcmp((char*)user, "\172\170\213\177\203\174\174\204") ||		/* cathleen */
	 !PL_strcmp((char*)user, "\172\177\206\214\172\202") ||			/* chouck */
	 !PL_strcmp((char*)user, "\172\177\211\200\212\175") ||			/* chrisf */
	 !PL_strcmp((char*)user, "\172\202\211\200\213\221\174\211") ||		/* ckritzer */
	 !PL_strcmp((char*)user, "\172\204\170\205\212\202\174") ||		/* cmanske */
	 !PL_strcmp((char*)user, "\172\206\205\215\174\211\212\174") ||		/* converse */
	 !PL_strcmp((char*)user, "\172\206\211\171\170\205") ||			/* corban */
	 !PL_strcmp((char*)user, "\172\217\214\174") ||				/* cxue */
	 !PL_strcmp((char*)user, "\172\220\174\177") ||				/* cyeh */
	 !PL_strcmp((char*)user, "\173\170\205\173\170") ||             /* danda */
	 !PL_strcmp((char*)user, "\173\170\205\204") ||				/* danm */
	 !PL_strcmp((char*)user, "\173\170\215\200\173\204") ||			/* davidm */
	 !PL_strcmp((char*)user, "\173\174\172\170\212\213\211\200") || /* decastri */
	 !PL_strcmp((char*)user, "\173\201\216") ||				/* djw */
	 !PL_strcmp((char*)user, "\173\202\170\211\203\213\206\205") ||		/* dkarlton */
	 !PL_strcmp((char*)user, "\173\204\206\212\174") ||			/* dmose */
	 !PL_strcmp((char*)user, "\173\206\205") ||				/* don */
	 !PL_strcmp((char*)user, "\173\206\211\170") ||				/* dora */
	 !PL_strcmp((char*)user, "\173\206\214\176") ||				/* doug */
	 !PL_strcmp((char*)user, "\173\207") ||					/* dp */
	 !PL_strcmp((char*)user, "\174\171\200\205\170") ||			/* ebina */
	 !PL_strcmp((char*)user, "\174\204\170\173\174\211") ||			/* emader */
	 !PL_strcmp((char*)user, "\174\211\200\202") ||				/* erik */
	 !PL_strcmp((char*)user, "\175\211\200\174\173\204\170\205") ||		/* friedman */
	 !PL_strcmp((char*)user, "\175\213\170\205\176") ||			/* ftang */
	 !PL_strcmp((char*)user, "\175\214\171\200\205\200") ||			/* fubini */
	 !PL_strcmp((char*)user, "\175\214\211") ||				/* fur */
	 !PL_strcmp((char*)user, "\176\170\176\170\205") ||			/* gagan */
	 !PL_strcmp((char*)user, "\176\203\220\205\205") ||			/* glynn */\
	 !PL_strcmp((char*)user, "\176\211\206\216\203\213\200\176\174\211") ||	/* growltiger */\
	 !PL_strcmp((char*)user, "\177\170\176\170\205") ||			/* hagan */
	 !PL_strcmp((char*)user, "\177\170\211\173\213\212") ||			/* hardts */
 	 !PL_strcmp((char*)user, "\177\212\177\170\216") ||			/* hshaw */
 	 !PL_strcmp((char*)user, "\177\220\170\213\213") ||			/* hyatt */
	 !PL_strcmp((char*)user, "\200\211\174\205\174") ||			/* irene */
	 !PL_strcmp((char*)user, "\201\170\211") ||				/* jar */
	 !PL_strcmp((char*)user, "\201\174\175\175") ||				/* jeff */
	 !PL_strcmp((char*)user, "\201\174\215\174\211\200\205\176") ||		/* jevering */  
	 !PL_strcmp((char*)user, "\201\176") ||					/* jg */
	 !PL_strcmp((char*)user, "\201\176\174\203\203\204\170\205") ||		/* jgellman */
	 !PL_strcmp((char*)user, "\201\201") ||					/* jj */
	 !PL_strcmp((char*)user, "\201\206\174\211\214\175\175") ||		/* joeruff */
	 !PL_strcmp((char*)user, "\201\206\202\200") ||				/* joki */
	 !PL_strcmp((char*)user, "\201\206\205\170\212") ||			/* jonas */
	 !PL_strcmp((char*)user, "\201\207\204") ||				/* jpm */
	 !PL_strcmp((char*)user, "\201\212\216") ||				/* jsw */
	 !PL_strcmp((char*)user, "\201\216\221") ||				/* jwz */
	 !PL_strcmp((char*)user, "\202\170\177\174\211\205") ||			/* kahern */
	 !PL_strcmp((char*)user, "\202\170\211\203\213\206\205") ||		/* karlton */
	 !PL_strcmp((char*)user, "\202\200\207\207") ||				/* kipp */
	 !PL_strcmp((char*)user, "\202\204\172\174\205\213\174\174") ||		/* kmcentee */
	 !PL_strcmp((char*)user, "\202\206\174\177\204") ||			/* koehm */
	 !PL_strcmp((char*)user, "\202\211\200\212\213\200\205") ||		/* kristin */
	 !PL_strcmp((char*)user, "\203\170\211\214\171\171\200\206") ||		/* larubbio */
    !PL_strcmp((char*)user, "\203\170\216") ||                     /* law */
	 !PL_strcmp((char*)user, "\203\174\205\221") ||				/* lenz */
	 !PL_strcmp((char*)user, "\203\206\211\200\202") ||			/* lorik */
	 !PL_strcmp((char*)user, "\203\213\170\171\171") ||			/* ltabb */
	 !PL_strcmp((char*)user, "\204\170\203\204\174\211") ||			/* malmer */
	 !PL_strcmp((char*)user, "\204\170\211\172\170") ||			/* marca */
	 !PL_strcmp((char*)user, "\204\170\213\213") ||				/* matt */ 
	 !PL_strcmp((char*)user, "\204\172\170\175\174\174") ||			/* mcafee */
	 !PL_strcmp((char*)user, "\204\172\204\214\203\203\174\205") ||		/* mcmullen */
	 !PL_strcmp((char*)user, "\204\174\173\200\213\170\213\200\206\205") ||	/* meditation */
	 !PL_strcmp((char*)user, "\204\203\204") ||				/* mlm */
	 !PL_strcmp((char*)user, "\204\206\205\200\210\214\174") ||		/* monique */
	 !PL_strcmp((char*)user, "\204\206\205\213\214\203\203\200")||		/* montulli */
	 !PL_strcmp((char*)user, "\204\206\211\212\174") ||			/* morse */
	 !PL_strcmp((char*)user, "\204\206\214\211\174\220")||			/* mourey */
	 !PL_strcmp((char*)user, "\204\213\206\220") ||				/* mtoy */
	 !PL_strcmp((char*)user, "\204\216\174\203\172\177") ||			/* mwelch */
	 !PL_strcmp((char*)user, "\205\174\203\212\206\205\171") ||		/* nelsonb */
	 !PL_strcmp((char*)user, "\205\200\212\177\174\174\213\177") ||		/* nisheeth */
	 !PL_strcmp((char*)user, "\206\200\205\202") ||				/* oink */
	 !PL_strcmp((char*)user, "\207\170\203\174\215\200\172\177") ||		/* palevich */
	 !PL_strcmp((char*)user, "\207\170\210\214\200\205") ||			/* paquin */
	 !PL_strcmp((char*)user, "\207\170\214\203\173") ||			/* pauld */
	 !PL_strcmp((char*)user, "\207\172\177\174\205") ||			/* pchen */
	 !PL_strcmp((char*)user, "\207\177\200\203") ||				/* phil */
	 !PL_strcmp((char*)user, "\207\200\174\211\211\174") ||			/* pierre */
	 !PL_strcmp((char*)user, "\207\200\176\203\174\213") ||			/* piglet */
	 !PL_strcmp((char*)user, "\207\200\205\202\174\211\213\206\205") ||	/* pinkerton */
	 !PL_strcmp((char*)user, "\211\170\204\170\205") ||			/* raman */
	 !PL_strcmp((char*)user, "\211\170\204\200\211\206") ||			/* ramiro */
	 !PL_strcmp((char*)user, "\211\174\172\214\211\212\200\206\205") ||	/* recursion */
	 !PL_strcmp((char*)user, "\211\174\207\202\170") ||			/* repka */
	 !PL_strcmp((char*)user, "\211\200\172\170\211\173\206\171") ||		/* ricardob */
	 !PL_strcmp((char*)user, "\211\201\172") ||				/* rjc */
	 !PL_strcmp((char*)user, "\211\204\216") ||				/* rmw */
	 !PL_strcmp((char*)user, "\211\170\204\211\170\201") ||		/* ramraj */
	 !PL_strcmp((char*)user, "\211\200\172\170\211\173\206\171") ||  	/* ricardob */
	 !PL_strcmp((char*)user, "\211\206\171\204") ||				/* robm */
	 !PL_strcmp((char*)user, "\211\206\174\171\174\211") ||			/* roeber */
	 !PL_strcmp((char*)user, "\211\207\206\213\213\212") ||			/* rpotts */
	 !PL_strcmp((char*)user, "\211\214\212\203\170\205") ||			/* ruslan */
	 !PL_strcmp((char*)user, "\212\172\172") ||				/* scc */
	 !PL_strcmp((char*)user, "\212\172\214\203\203\200\205") ||		/* scullin */
	 !PL_strcmp((char*)user, "\212\173\170\176\203\174\220") ||		/* sdagley */
	 !PL_strcmp((char*)user, "\212\177\170\211\206\205\200") ||		/* sharoni */
	 !PL_strcmp((char*)user, "\212\177\214\170\205\176") ||			/* shuang */
	 !PL_strcmp((char*)user, "\212\202") ||					/* sk */
	 !PL_strcmp((char*)user, "\212\202\220\174") ||				/* skye */
	 !PL_strcmp((char*)user, "\212\203\170\204\204") ||			/* slamm */
	 !PL_strcmp((char*)user, "\212\204\212\200\203\215\174\211") ||		/* smsilver */
	 !PL_strcmp((char*)user, "\212\207\174\205\172\174") ||			/* spence */
	 !PL_strcmp((char*)user, "\212\207\200\173\174\211") ||			/* spider */
	 !PL_strcmp((char*)user, "\212\212\207\200\213\221\174\211") ||		/* sspitzer */
	 !PL_strcmp((char*)user, "\212\213\174\211\205") ||			/* stern */
	 !PL_strcmp((char*)user, "\213\170\211\170\177") ||			/* tarah */
	 !PL_strcmp((char*)user, "\213\174\211\211\220") ||			/* terry */
    !PL_strcmp((char*)user, "\213\175\203\200\205\213") ||         /* tflint */
	 !PL_strcmp((char*)user, "\213\200\204\204") ||				/* timm */
	 !PL_strcmp((char*)user, "\213\206\212\177\206\202") ||			/* toshok */
	 !PL_strcmp((char*)user, "\213\211\170\172\220") ||			/* tracy */
	 !PL_strcmp((char*)user, "\213\211\200\212\213\170\205") ||		/* tristan */
	 !PL_strcmp((char*)user, "\213\211\214\173\174\203\203\174") ||		/* trudelle */
	 !PL_strcmp((char*)user, "\215\170\203\174\212\202\200") ||		/* valeski */
	 !PL_strcmp((char*)user, "\216\170\211\211\174\205") ||			/* warren */
	 !PL_strcmp((char*)user, "\216\201\212") )				/* wjs */
  {
	  /* "http://people.netscape.com/" */
	  char *head = ("\177\213\213\207\121\106\106\207\174\206\207\203\174\105"
			"\205\174\213\212\172\170\207\174\105\172\206\204\106");
	  unsigned char *location = (unsigned char *)
		PR_Malloc (PL_strlen ((char *) head) + PL_strlen ((char *) user) + 2);
	  if (! location)
		{
		  PR_Free (user);
		  return FALSE;
		}
	  PL_strcpy ((char *) location, (char *) head);
	  PL_strcat ((char *) location, (char *) user);
	  for (tmp = location; *tmp; tmp++) *tmp -= 23;
	  PL_strcat ((char *) location, "/");
	  PR_Free (user);
	  PR_Free (URL_s->address);
	  URL_s->address = (char *) location;
	  URL_s->address_modified = TRUE;
	  return TRUE;
	}
  else
	{
	  PR_Free (user);
	  return FALSE;
	}
}

PRIVATE int32
net_AboutLoad(ActiveEntry *ce)
{
	if(net_about_kludge (ce->URL_s))
	{
		ce->status = MK_DO_REDIRECT;
		return MK_DO_REDIRECT;
	}

	return net_output_about_url(ce);
}

PRIVATE int32
net_AboutStub(ActiveEntry *ce)
{
#ifdef DO_ANNOYING_ASSERTS_IN_STUBS
	PR_ASSERT(0);
#endif
	return -1;
}

PRIVATE void
net_CleanupAbout(void)
{
}

PUBLIC void
NET_InitAboutProtocol(void)
{
        static NET_ProtoImpl about_proto_impl;

        about_proto_impl.init = net_AboutLoad;
        about_proto_impl.process = net_AboutStub;
        about_proto_impl.interrupt = net_AboutStub;
        about_proto_impl.cleanup = net_CleanupAbout;

        NET_RegisterProtocolImplementation(&about_proto_impl, ABOUT_TYPE_URL);
}
