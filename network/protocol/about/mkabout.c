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
#include "libimg.h"
#include "il_strm.h"
#include "cookies.h"
#include "httpauth.h"
#include "mkstream.h"
#include "glhist.h"

#include "abouturl.h"

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
extern int XP_SECURITY_ ;
extern int XP_CERTIFICATE_      ;
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
	XP_FREE(tmp);

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

	XP_SPRINTF(buf, "%lu", URL_s->content_length);
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

	if(URL_s->cache_file || URL_s->memory_copy)
		sec_msg = SECNAV_PrettySecurityStatus(URL_s->security_on, 
                                              URL_s->sec_info);
	else
		sec_msg = XP_STRDUP(XP_GetString(XP_STATUS_UNKNOWN));

	if(sec_msg)
	  {
		ADD_CELL(XP_GetString(XP_SECURITY_), sec_msg);
		XP_FREE(sec_msg);
	  }

    sec_msg = SECNAV_SSLSocketCertString(URL_s->sec_info);

	if(sec_msg)
	  {
		ADD_CELL(XP_GetString(XP_CERTIFICATE_), sec_msg);
		XP_FREE(sec_msg);
	  }
	StrAllocCat(output, "</TABLE>");

	if(URL_s->content_type 
		&& !strncasecomp(URL_s->content_type, "image", 5))
	  {
	/* Have a seat. Lie down.  Tell me about yourself. */
	il_msg = IL_HTMLImageInfo(URL_s->address);
	if (il_msg) {
	  StrAllocCat(output, "<HR>\n");
	  StrAllocCat(output, il_msg);
	  XP_FREE(il_msg);
	}
	  }
		
	*data = output;
	*length = XP_STRLEN(output);

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

    XP_STRCPY(buf, "<FONT SIZE=+2><b>");
	(*stream->put_block)(stream, buf, XP_STRLEN(buf));

    if(his && his->title)
		tmp = XP_STRDUP(his->title);
	else
		tmp = XP_STRDUP(XP_GetString(XP_UNTITLED_DOCUMENT));

	(*stream->put_block)(stream, tmp, XP_STRLEN(tmp));

    XP_STRCPY(buf, XP_GetString(XP_HAS_THE_FOLLOWING_STRUCT));
	(*stream->put_block)(stream, buf, XP_STRLEN(buf));

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
		if (help_dir[XP_STRLEN(help_dir)-1] != '/')
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
	else if(!strncasecomp(which, "pics", 4))
	  {
		data = net_gen_pics_document(cur_entry);
   		if (data) {
			length = XP_STRLEN(data);
			content_type = XP_STRDUP(TEXT_HTML);
            uses_fe_data = FALSE;
        } 
	  }
	else if(!strcasecomp(which, "cookies"))
	{
		NET_DisplayCookieInfoAsHTML(cur_entry);
		return(-1);
	}
	else if(!strncasecomp(which, "cache", 5))
	  {
		NET_DisplayCacheInfoAsHTML(cur_entry);
		return(-1);
	  }
	else if(!strcasecomp(which, "memory-cache"))
	  {
		NET_DisplayMemCacheInfoAsHTML(cur_entry);
		return(-1);
	  }
	else if(!strcasecomp(which, "logout"))
	  {
		NET_RemoveAllAuthorizations();
		return(-1);
	  }
	else if(!strcasecomp(which, "image-cache"))
	  {
	IL_DisplayMemCacheInfoAsHTML(cur_entry->format_out, cur_entry->URL_s,
				     cur_entry->window_id);
		return(-1);
	  }
#ifdef DEBUG
    else if(!strcasecomp(which, "streams"))
      {
	NET_DisplayStreamInfoAsHTML(cur_entry);
	return(-1);
      }
#endif /* DEBUG */
	else if(!strcasecomp(which, "document"))
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
		cur_entry->URL_s->window_chrome = XP_NEW(Chrome);

		if(cur_entry->URL_s->window_chrome)
		  {
			XP_MEMSET(cur_entry->URL_s->window_chrome, 0, sizeof(Chrome));
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
#if defined(__sun) && !defined(SVR4) /* sun 4.1.3 */
			    XP_SPRINTF(buf, "%lu", context);
#else
			    XP_SPRINTF(buf, "%p", context);
#endif
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

		length = XP_STRLEN(data);
		StrAllocCopy(content_type, TEXT_HTML);
	  }
	else if(!strncasecomp(which, "Global", 6))
	  {
		cur_entry->status = NET_DisplayGlobalHistoryInfoAsHTML(
													cur_entry->window_id,
									cur_entry->URL_s,
									cur_entry->format_out);

		return(-1);

	  }
	else if(!XP_STRNCMP(which, "FeCoNtExT=", 10))
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
    
	    length = XP_STRLEN(data);
	    StrAllocCopy(content_type, TEXT_HTML);

      }
#endif /* MOZILLA_CLIENT */
	else if(!strncasecomp(which, "authors", 7))
	  {
		static const char *d =
		  ("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html\" "
		   "CHARSET=\"iso-8859-1\"><TITLE>Mozilla &Uuml;ber Alles</TITLE>"
		   "<BODY BGCOLOR=\"#C0C0C0\" TEXT=\"#000000\">"
		   "<TABLE width=\"100%\" height=\"100%\"><TR><TD ALIGN=CENTER>"
		   "<FONT SIZE=5><FONT COLOR=\"#0000EE\">about:authors</FONT> "
		   "removed.</FONT></TD></TR></TABLE>");
	    data = XP_STRDUP(d);
		length = (d ? XP_STRLEN(d) : 0);
	    content_type = XP_STRDUP(TEXT_HTML);
	    uses_fe_data = FALSE;
	  }
	/* Admin Kit/xp prefs diagnostic support */
	else if (!strncasecomp(which, "config", 6))
	{
		data = PREF_AboutConfig();
		if (data) {
			length = XP_STRLEN(data);
			content_type = XP_STRDUP(TEXT_HTML);
		}
	}
#ifdef XP_UNIX
	else if (!strncasecomp(which, "minibuffer", 10))
	{
		extern void FE_ShowMinibuffer(MWContext *);

		FE_ShowMinibuffer(cur_entry->window_id);
	}
#endif
#ifdef WEBFONTS
	else if (!strncasecomp(which, "fonts", 5))
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
			if (XP_STRCMP(which, "") == 0 &&
				PREF_CopyConfigString("about_text", &custom_text) == PREF_NOERROR) {
				(*stream->put_block)(stream, custom_text, XP_STRLEN(custom_text));
			}
		
			(*stream->complete) (stream);
			
			XP_FREEIF(custom_text);
		}
		else
		  (*stream->abort) (stream, status);

		cur_entry->status = status;

		XP_FREE(stream);
	  }

	if(uses_fe_data)
	  {
#if defined(XP_MAC) || defined(XP_UNIX) || defined (XP_WIN)
		FE_FreeAboutData (fe_data, which);
#endif /* XP_MAC || XP_UNIX */
	  }
	else
	  {
		XP_FREE(data);
		XP_FREE(content_type);
	  }


    return(-1);
}


PRIVATE Bool net_about_kludge(URL_Struct *URL_s)
{
  unsigned char *url = (unsigned char *) URL_s->address;
  unsigned char *user = (unsigned char *) XP_STRDUP ((char*)(url + 6));
  unsigned char *tmp;
 
  if (user == NULL)
	return FALSE;

  for (tmp = user; *tmp; tmp++) *tmp += 23;

  if (	 !XP_STRCMP((char*)user, "\170\202\202\170\205\170") ||	/* akkana */
	 !XP_STRCMP((char*)user, "\170\211\200") ||				/* ari */
	 !XP_STRCMP((char*)user, "\170\211\212\177\170\173") ||			/* arshad */
	 !XP_STRCMP((char*)user, "\170\213\206\213\200\172") ||			/* atotic */
	 !XP_STRCMP((char*)user, "\171\176\170\220") ||				/* bgay */
	 !XP_STRCMP((char*)user, "\171\201\206\205\174\212") ||			/* bjones */
	 !XP_STRCMP((char*)user, "\171\203\220\213\177\174") ||			/* blythe */
	 !XP_STRCMP((char*)user, "\171\205\174\203\212\206\205") ||		/* bnelson */
	 !XP_STRCMP((char*)user, "\171\206\171") ||				/* bob */
	 !XP_STRCMP((char*)user, "\171\206\171\201") ||				/* bobj */
	 !XP_STRCMP((char*)user, "\171\211\170\173\174") ||			/* brade */
	 !XP_STRCMP((char*)user, "\171\211\200\170\205\206") ||			/* briano */
	 !XP_STRCMP((char*)user, "\171\211\200\173\176\174") ||			/* bridge */
	 !XP_STRCMP((char*)user, "\172\170\213\177\203\174\174\204") ||		/* cathleen */
	 !XP_STRCMP((char*)user, "\172\177\206\214\172\202") ||			/* chouck */
	 !XP_STRCMP((char*)user, "\172\177\211\200\212\175") ||			/* chrisf */
	 !XP_STRCMP((char*)user, "\172\202\211\200\213\221\174\211") ||		/* ckritzer */
	 !XP_STRCMP((char*)user, "\172\204\170\205\212\202\174") ||		/* cmanske */
	 !XP_STRCMP((char*)user, "\172\206\205\215\174\211\212\174") ||		/* converse */
	 !XP_STRCMP((char*)user, "\172\206\211\171\170\205") ||			/* corban */
	 !XP_STRCMP((char*)user, "\172\217\214\174") ||				/* cxue */
	 !XP_STRCMP((char*)user, "\172\220\174\177") ||				/* cyeh */
	 !XP_STRCMP((char*)user, "\173\170\205\173\170") ||             /* danda */
	 !XP_STRCMP((char*)user, "\173\170\205\204") ||				/* danm */
	 !XP_STRCMP((char*)user, "\173\170\215\200\173\204") ||			/* davidm */
	 !XP_STRCMP((char*)user, "\173\174\172\170\212\213\211\200") || /* decastri */
	 !XP_STRCMP((char*)user, "\173\201\216") ||				/* djw */
	 !XP_STRCMP((char*)user, "\173\202\170\211\203\213\206\205") ||		/* dkarlton */
	 !XP_STRCMP((char*)user, "\173\204\206\212\174") ||			/* dmose */
	 !XP_STRCMP((char*)user, "\173\206\205") ||				/* don */
	 !XP_STRCMP((char*)user, "\173\206\211\170") ||				/* dora */
	 !XP_STRCMP((char*)user, "\173\206\214\176") ||				/* doug */
	 !XP_STRCMP((char*)user, "\173\207") ||					/* dp */
	 !XP_STRCMP((char*)user, "\174\171\200\205\170") ||			/* ebina */
	 !XP_STRCMP((char*)user, "\174\204\170\173\174\211") ||			/* emader */
	 !XP_STRCMP((char*)user, "\174\211\200\202") ||				/* erik */
	 !XP_STRCMP((char*)user, "\175\211\200\174\173\204\170\205") ||		/* friedman */
	 !XP_STRCMP((char*)user, "\175\213\170\205\176") ||			/* ftang */
	 !XP_STRCMP((char*)user, "\175\214\171\200\205\200") ||			/* fubini */
	 !XP_STRCMP((char*)user, "\175\214\211") ||				/* fur */
	 !XP_STRCMP((char*)user, "\176\170\176\170\205") ||			/* gagan */
	 !XP_STRCMP((char*)user, "\176\203\220\205\205") ||			/* glynn */\
	 !XP_STRCMP((char*)user, "\176\211\206\216\203\213\200\176\174\211") ||	/* growltiger */\
	 !XP_STRCMP((char*)user, "\177\170\176\170\205") ||			/* hagan */
	 !XP_STRCMP((char*)user, "\177\170\211\173\213\212") ||			/* hardts */
 	 !XP_STRCMP((char*)user, "\177\212\177\170\216") ||			/* hshaw */
 	 !XP_STRCMP((char*)user, "\177\220\170\213\213") ||			/* hyatt */
	 !XP_STRCMP((char*)user, "\200\211\174\205\174") ||			/* irene */
	 !XP_STRCMP((char*)user, "\201\170\211") ||				/* jar */
	 !XP_STRCMP((char*)user, "\201\174\175\175") ||				/* jeff */
	 !XP_STRCMP((char*)user, "\201\174\215\174\211\200\205\176") ||		/* jevering */  
	 !XP_STRCMP((char*)user, "\201\176") ||					/* jg */
	 !XP_STRCMP((char*)user, "\201\176\174\203\203\204\170\205") ||		/* jgellman */
	 !XP_STRCMP((char*)user, "\201\201") ||					/* jj */
	 !XP_STRCMP((char*)user, "\201\206\174\211\214\175\175") ||		/* joeruff */
	 !XP_STRCMP((char*)user, "\201\206\202\200") ||				/* joki */
	 !XP_STRCMP((char*)user, "\201\206\205\170\212") ||			/* jonas */
	 !XP_STRCMP((char*)user, "\201\207\204") ||				/* jpm */
	 !XP_STRCMP((char*)user, "\201\212\216") ||				/* jsw */
	 !XP_STRCMP((char*)user, "\201\216\221") ||				/* jwz */
	 !XP_STRCMP((char*)user, "\202\170\177\174\211\205") ||			/* kahern */
	 !XP_STRCMP((char*)user, "\202\170\211\203\213\206\205") ||		/* karlton */
	 !XP_STRCMP((char*)user, "\202\200\207\207") ||				/* kipp */
	 !XP_STRCMP((char*)user, "\202\204\172\174\205\213\174\174") ||		/* kmcentee */
	 !XP_STRCMP((char*)user, "\202\206\174\177\204") ||			/* koehm */
	 !XP_STRCMP((char*)user, "\202\211\200\212\213\200\205") ||		/* kristin */
	 !XP_STRCMP((char*)user, "\203\170\211\214\171\171\200\206") ||		/* larubbio */
    !XP_STRCMP((char*)user, "\203\170\216") ||                     /* law */
	 !XP_STRCMP((char*)user, "\203\174\205\221") ||				/* lenz */
	 !XP_STRCMP((char*)user, "\203\206\211\200\202") ||			/* lorik */
	 !XP_STRCMP((char*)user, "\203\213\170\171\171") ||			/* ltabb */
	 !XP_STRCMP((char*)user, "\204\170\203\204\174\211") ||			/* malmer */
	 !XP_STRCMP((char*)user, "\204\170\211\172\170") ||			/* marca */
	 !XP_STRCMP((char*)user, "\204\170\213\213") ||				/* matt */ 
	 !XP_STRCMP((char*)user, "\204\172\170\175\174\174") ||			/* mcafee */
	 !XP_STRCMP((char*)user, "\204\172\204\214\203\203\174\205") ||		/* mcmullen */
	 !XP_STRCMP((char*)user, "\204\174\173\200\213\170\213\200\206\205") ||	/* meditation */
	 !XP_STRCMP((char*)user, "\204\203\204") ||				/* mlm */
	 !XP_STRCMP((char*)user, "\204\206\205\200\210\214\174") ||		/* monique */
	 !XP_STRCMP((char*)user, "\204\206\205\213\214\203\203\200")||		/* montulli */
	 !XP_STRCMP((char*)user, "\204\206\211\212\174") ||			/* morse */
	 !XP_STRCMP((char*)user, "\204\206\214\211\174\220")||			/* mourey */
	 !XP_STRCMP((char*)user, "\204\213\206\220") ||				/* mtoy */
	 !XP_STRCMP((char*)user, "\204\216\174\203\172\177") ||			/* mwelch */
	 !XP_STRCMP((char*)user, "\205\174\203\212\206\205\171") ||		/* nelsonb */
	 !XP_STRCMP((char*)user, "\205\200\212\177\174\174\213\177") ||		/* nisheeth */
	 !XP_STRCMP((char*)user, "\206\200\205\202") ||				/* oink */
	 !XP_STRCMP((char*)user, "\207\170\203\174\215\200\172\177") ||		/* palevich */
	 !XP_STRCMP((char*)user, "\207\170\210\214\200\205") ||			/* paquin */
	 !XP_STRCMP((char*)user, "\207\170\214\203\173") ||			/* pauld */
	 !XP_STRCMP((char*)user, "\207\172\177\174\205") ||			/* pchen */
	 !XP_STRCMP((char*)user, "\207\200\174\211\211\174") ||			/* pierre */
	 !XP_STRCMP((char*)user, "\207\200\176\203\174\213") ||			/* piglet */
	 !XP_STRCMP((char*)user, "\207\200\205\202\174\211\213\206\205") ||	/* pinkerton */
	 !XP_STRCMP((char*)user, "\211\170\204\170\205") ||			/* raman */
	 !XP_STRCMP((char*)user, "\211\170\204\200\211\206") ||			/* ramiro */
	 !XP_STRCMP((char*)user, "\211\174\172\214\211\212\200\206\205") ||	/* recursion */
	 !XP_STRCMP((char*)user, "\211\174\207\202\170") ||			/* repka */
	 !XP_STRCMP((char*)user, "\211\200\172\170\211\173\206\171") ||		/* ricardob */
	 !XP_STRCMP((char*)user, "\211\201\172") ||				/* rjc */
	 !XP_STRCMP((char*)user, "\211\204\216") ||				/* rmw */
	 !XP_STRCMP((char*)user, "\211\170\204\211\170\201") ||		/* ramraj */
	 !XP_STRCMP((char*)user, "\211\206\171\204") ||				/* robm */
	 !XP_STRCMP((char*)user, "\211\206\174\171\174\211") ||			/* roeber */
	 !XP_STRCMP((char*)user, "\211\207\206\213\213\212") ||			/* rpotts */
	 !XP_STRCMP((char*)user, "\211\214\212\203\170\205") ||			/* ruslan */
	 !XP_STRCMP((char*)user, "\212\172\172") ||				/* scc */
	 !XP_STRCMP((char*)user, "\212\173\170\176\203\174\220") ||		/* sdagley */
	 !XP_STRCMP((char*)user, "\212\177\170\211\206\205\200") ||		/* sharoni */
	 !XP_STRCMP((char*)user, "\212\177\214\170\205\176") ||			/* shuang */
	 !XP_STRCMP((char*)user, "\212\202") ||					/* sk */
	 !XP_STRCMP((char*)user, "\212\202\220\174") ||				/* skye */
	 !XP_STRCMP((char*)user, "\212\203\170\204\204") ||			/* slamm */
	 !XP_STRCMP((char*)user, "\212\204\212\200\203\215\174\211") ||		/* smsilver */
	 !XP_STRCMP((char*)user, "\212\207\174\205\172\174") ||			/* spence */
	 !XP_STRCMP((char*)user, "\212\207\200\173\174\211") ||			/* spider */
	 !XP_STRCMP((char*)user, "\212\212\207\200\213\221\174\211") ||		/* sspitzer */
	 !XP_STRCMP((char*)user, "\212\213\174\211\205") ||			/* stern */
	 !XP_STRCMP((char*)user, "\213\170\211\170\177") ||			/* tarah */
	 !XP_STRCMP((char*)user, "\213\174\211\211\220") ||			/* terry */
    !XP_STRCMP((char*)user, "\213\175\203\200\205\213") ||         /* tflint */
	 !XP_STRCMP((char*)user, "\213\200\204\204") ||				/* timm */
	 !XP_STRCMP((char*)user, "\213\206\212\177\206\202") ||			/* toshok */
	 !XP_STRCMP((char*)user, "\213\211\170\172\220") ||			/* tracy */
	 !XP_STRCMP((char*)user, "\213\211\200\212\213\170\205") ||		/* tristan */
	 !XP_STRCMP((char*)user, "\213\211\214\173\174\203\203\174") ||		/* trudelle */
	 !XP_STRCMP((char*)user, "\215\170\203\174\212\202\200") ||		/* valeski */
	 !XP_STRCMP((char*)user, "\216\170\211\211\174\205") ||			/* warren */
	 !XP_STRCMP((char*)user, "\216\201\212") )				/* wjs */
  {
	  /* "http://people.netscape.com/" */
	  char *head = ("\177\213\213\207\121\106\106\207\174\206\207\203\174\105"
			"\205\174\213\212\172\170\207\174\105\172\206\204\106");
	  unsigned char *location = (unsigned char *)
		XP_ALLOC (XP_STRLEN ((char *) head) + XP_STRLEN ((char *) user) + 2);
	  if (! location)
		{
		  XP_FREE (user);
		  return FALSE;
		}
	  XP_STRCPY ((char *) location, (char *) head);
	  XP_STRCAT ((char *) location, (char *) user);
	  for (tmp = location; *tmp; tmp++) *tmp -= 23;
	  XP_STRCAT ((char *) location, "/");
	  XP_FREE (user);
	  XP_FREE (URL_s->address);
	  URL_s->address = (char *) location;
	  URL_s->address_modified = TRUE;
	  return TRUE;
	}
  else
	{
	  XP_FREE (user);
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
	XP_ASSERT(0);
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
