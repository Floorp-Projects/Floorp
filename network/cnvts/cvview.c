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
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#ifdef MOZILLA_CLIENT

#include "mkstream.h"
#include "mkparse.h"
#include "cvview.h"

#include "libmime.h"


/* for XP_GetString() */
#include <xpgetstr.h>
extern int XP_CONFIRM_EXEC_UNIXCMD_ARE;
extern int XP_CONFIRM_EXEC_UNIXCMD_MAYBE;
extern int XP_ALERT_UNABLE_INVOKEVIEWER;
extern int MK_UNABLE_TO_OPEN_TMP_FILE;


typedef struct _CV_DataObject {
    FILE      * fp;
    char      * filename;
    char      * command;
	char      * url;
    unsigned int stream_block_size;
	int32       cur_size;
	int32       tot_size;
	MWContext * context;
} CV_DataObject;


/*
** build_viewer_cmd
**	Build up the command for forking the external viewer.
**	Argument list is the template for the command and the a set of
**	(char,char*) pairs of characters to be recognized as '%' escapes
**	and what they should expand to, terminated with a 0.
**
**	Return value is a malloc'ed string, must be freed when command is done.
**
**	Example:
**	    char* s=build_viewer(line_from_mailcap, 's', tmpFile, 'u', url, 0);
**
**	I'm completely unsure what to do about security considerations and
**	encodings.  Should the URL get % encoded.  What if it contains "bad"
**	characters.  etc. etc. etc
*/

char*
build_viewer_cmd(char *template, ...)
{
  va_list args;
  char *ret, *from, *to;
  int len;

  if (template == NULL)
    return NULL;

  len = strlen(template);
  ret = (char*) malloc(len+1);
  if (ret == NULL)
    return NULL;

  from = template, to = ret;

  while (*from) {
    if (*from != '%' || *++from == '%') {
      *to++ = *from++;
    } else {

      /*
      ** We have a % escape, now look through all the arguments for
      ** a matching one.  When one is found, substitute in the
      ** passed value.  If none is found, the % and following character
      ** get swallowed.
      */
      char argc;
      char* argv;

      va_start(args, template);
      while ((argc = va_arg(args, int)) != 0) {
	argv = va_arg(args, char*);
	if (*from == argc) {
	  int off = to - ret;
	  int arglen = strlen(argv);

	  len = len + arglen - 2;
	  ret = (char*) realloc(ret, len + 1);
	  if (ret == NULL)
	    return NULL;
	  XP_STRCPY(ret + off, argv);
	  to = ret + off + arglen;
	  break;
	}
      }
      if (*from) from++;	/* skip char following % unless it was last */
      va_end(args);
    }
  }
  *to = '\0';
  return ret;
}

PRIVATE int net_ExtViewWrite (NET_StreamClass *stream, CONST char* s, int32 l)
{
	CV_DataObject *obj=stream->data_object;	
	if(obj->tot_size)
	  {
		obj->cur_size += l;

		obj->context->funcs->SetProgressBarPercent(obj->context, (obj->cur_size*100)/obj->tot_size);
	  }

    /* TRACEMSG(("Length of string passed to display: %d\n",l));
     */
    return(fwrite((char *) s, 1, l, obj->fp)); 
}

PRIVATE int net_ExtViewWriteReady (NET_StreamClass * stream)
{
	CV_DataObject *obj=stream->data_object;
    fd_set write_fds;
    struct timeval timeout;
    int ret;	

    if(obj->command)
	  {
		return(MAX_WRITE_READY);  /* never wait for files */
	  }

    timeout.tv_sec = 0;
    timeout.tv_usec = 1;  /* minimum hopefully */

    XP_MEMSET(&write_fds, 0, sizeof(fd_set));

    FD_SET(fileno(obj->fp), &write_fds);

    ret = select(fileno(obj->fp)+1, NULL, &write_fds, NULL, &timeout);

    if(ret)
	    return(obj->stream_block_size);  /* read in a max of 8000 bytes */
	else
		return(0);
}


PRIVATE void net_ExtViewComplete (NET_StreamClass *stream)
{
	CV_DataObject *obj=stream->data_object;
	obj->context->funcs->SetProgressBarPercent(obj->context, 100);	

    if(obj->command)
	  {
		char *p_tmp;
		char *command;

        /* restrict to allowed url chars
         *
         */
        for(p_tmp = obj->url; *p_tmp != '\0'; p_tmp++)
                if( (*p_tmp >= '0' && *p_tmp <= '9')
                    || (*p_tmp >= 'A' && *p_tmp <= 'Z')
                    || (*p_tmp >= 'a' && *p_tmp <= 'z')
                    || (*p_tmp == '_')
                    || (*p_tmp == '?')
                    || (*p_tmp == '#')
                    || (*p_tmp == '&')
                    || (*p_tmp == '%')
                    || (*p_tmp == '/')
                    || (*p_tmp == ':')
                    || (*p_tmp == '+')
                    || (*p_tmp == '.')
                    || (*p_tmp == '~')
                    || (*p_tmp == '=')
                    || (*p_tmp == '-'))
          {
            /* this is a good character.  Allow it.
             */
          }
        else
          {
            *p_tmp = '\0';
            break;
          }

		command=build_viewer_cmd(obj->command, 
							 's', obj->filename, 
							 'u', obj->url, 0);

        fclose(obj->fp);
		TRACEMSG(("Invoking: %s", command));

        system(command);
	    FREEIF(obj->command);
      }
	else
      {
        pclose(obj->fp);
      }

    FREEIF(obj->filename);
	FREEIF(obj->url);
    FREE(obj);
    return;
}

PRIVATE void net_ExtViewAbort (NET_StreamClass *stream, int status)
{
	CV_DataObject *obj=stream->data_object;
	obj->context->funcs->SetProgressBarPercent(obj->context, 100);	

    fclose(obj->fp);
   
    if(obj->filename)
      {
	    remove(obj->filename);
        FREE(obj->filename);
      }

	FREEIF(obj->url);
    FREEIF(obj->command);

    FREE(obj);
    
    return;
}

#ifdef XP_UNIX
extern char **fe_encoding_extensions; /* gag! */
#endif


PUBLIC NET_StreamClass * 
NET_ExtViewerConverter   (int         format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *window_id)
{
    CV_DataObject* obj;
    NET_StreamClass* stream;
	char *tmp_filename;
    char *dot;
    char *path;
    CV_ExtViewStruct * view_struct = (CV_ExtViewStruct *)data_obj;
	char small_buf[256];
	int yes_stream=0;
    
	/* If this URL is a mail or news attachment, use the name of that
	   attachment as the URL -- this is so the temp file gets the right
	   extension on it (some helper apps are picky about that...)
	 */
	path = MimeGuessURLContentName(window_id, URL_s->address);
	if (!path)
	  path = NET_ParseURL(URL_s->address, GET_PATH_PART);
	if (!path)
	  return 0;


    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = XP_NEW(NET_StreamClass);
    if(stream == NULL) 
            return(NULL);

	memset(stream, 0, sizeof(NET_StreamClass));

    obj = XP_NEW(CV_DataObject);
    if (obj == NULL) 
            return(NULL);
	memset(obj, 0, sizeof(CV_DataObject));

	obj->context  = window_id;

	if(URL_s->content_length)
	  {
	    obj->tot_size = URL_s->content_length;
	  }
	else
	  {
	    /* start the progress bar cyloning
	     */
	    obj->context->funcs->SetProgressBarPercent(window_id, -1);
	  }

    stream->name           = "Execute external viewer";
    stream->complete       = (MKStreamCompleteFunc) net_ExtViewComplete;
    stream->abort          = (MKStreamAbortFunc) net_ExtViewAbort;
    stream->put_block      = (MKStreamWriteFunc) net_ExtViewWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_ExtViewWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

#ifdef XP_UNIX
	/* Some naive people may have trustingly put

			application/x-sh; sh %s
			application/x-csh; csh %s

	   in their mailcap files without realizing how dangerous that is.
	   Worse, it might be there and they might not realize it.  So, if
	   we're about to execute a shell, pop up a dialog box first.
	 */
	{
	  char *prog = XP_STRDUP (view_struct->system_command);
	  char *s, *start, *end;
	  int danger = 0;

	  /* strip end space */
	  end = XP_StripLine(prog);

	  /* Extract the leaf name of the program: " /bin/sh -foo" ==> "sh". */
	  for (; *end && !XP_IS_SPACE(*end); end++)
		;
	  *end = 0;

	  if ((start = XP_STRRCHR (prog, '/')))
		start++;
	  else
	    start = XP_StripLine(prog); /* start at first non-white space */

	  /* Strip off everything after the first nonalphabetic.
		 This is so "perl-4.0" is compared as "perl" and
		 "emacs19" is compared as "emacs".
	   */
	  for (s = start; *s; s++)
		if (!isalpha (*s))
		  *s = 0;

	  /* These are things known to be shells - very bad. */
	  if (!XP_STRCMP (start, "ash") ||
		  !XP_STRCMP (start, "bash") ||
		  !XP_STRCMP (start, "csh") ||
		  !XP_STRCMP (start, "jsh") ||
		  !XP_STRCMP (start, "ksh") ||
		  !XP_STRCMP (start, "pdksh") ||
		  !XP_STRCMP (start, "sh") ||
		  !XP_STRCMP (start, "tclsh") ||
		  !XP_STRCMP (start, "tcsh") ||
		  !XP_STRCMP (start, "wish") ||		/* a tcl thing */
		  !XP_STRCMP (start, "wksh") ||
		  !XP_STRCMP (start, "zsh"))
		danger = 2;

	  /* Remote shells are potentially dangerous, in the case of
		 "rsh somehost some-dangerous-program", but it's hard to
		 parse that out, since rsh could take arbitrarily complicated
		 args, like "rsh somehost -u something -pass8 /bin/sh %s".
		 And we don't want to squawk about "rsh somehost playulaw -".
		 So... allow rsh to possibly be a security hole.
	   */
	  else if (!XP_STRCMP (start, "remsh") ||	/* remote shell */
			   !XP_STRCMP (start, "rksh") ||
			   !XP_STRCMP (start, "rsh")		/* remote- or restricted- */
			   )
		danger = 0;

	  /* These are things which aren't really shells, but can do the
		 same damage anyway since they can write files and/or execute
		 other programs. */
	  else if (!XP_STRCMP (start, "awk") ||
			   !XP_STRCMP (start, "e") ||
			   !XP_STRCMP (start, "ed") ||
			   !XP_STRCMP (start, "ex") ||
			   !XP_STRCMP (start, "gawk") ||
			   !XP_STRCMP (start, "m4") ||
			   !XP_STRCMP (start, "sed") ||
			   !XP_STRCMP (start, "vi") ||
			   !XP_STRCMP (start, "emacs") ||
			   !XP_STRCMP (start, "lemacs") ||
			   !XP_STRCMP (start, "xemacs") ||
			   !XP_STRCMP (start, "temacs") ||

			   /* Other dangerous interpreters */
			   !XP_STRCMP (start, "basic") ||
			   !XP_STRCMP (start, "expect") ||
			   !XP_STRCMP (start, "expectk") ||
			   !XP_STRCMP (start, "perl") ||
			   !XP_STRCMP (start, "python") ||
			   !XP_STRCMP (start, "rexx")
			   )
		danger = 1;

	  /* Be suspicious of anything ending in "sh". */
	  else if (XP_STRLEN (start) > 2 &&
			   !XP_STRCMP (start + XP_STRLEN (start) - 2, "sh"))
		danger = 1;

	  if (danger)
		{
		  char msg [2048];
		  PR_snprintf (msg,
				   sizeof(msg),
				   (danger > 1 ? XP_GetString(XP_CONFIRM_EXEC_UNIXCMD_ARE) : XP_GetString(XP_CONFIRM_EXEC_UNIXCMD_MAYBE)),
				   start
				   );
		  if (!FE_Confirm (window_id, msg))
			{
			  FREE (stream);
			  FREE (obj);
			  FREE (path);
			  FREE (prog);
			  return(NULL);
			}
		}
	  FREE (prog);
	}
#endif /* XP_UNIX */

    if(view_struct->stream_block_size)
	  {
		/* asks the user if they want to stream data.
		 * -1 cancel
		 * 0  No, don't stream data, play from the file
		 * 1  Yes, stream the data from the network
		 */
		int XFE_AskStreamQuestion(MWContext * window_id); /* definition */

		if (NET_URL_Type (URL_s->address) == ABOUT_TYPE_URL)
		  yes_stream = 1;
		else
		  yes_stream = XFE_AskStreamQuestion(window_id);

		if(yes_stream == -1)
		  {
			FREE(stream);
			FREE(obj);
        	FREE(path);
			return(NULL);
		  }
	  }

    if(yes_stream && view_struct->stream_block_size)
      {
		/* use popen */
		obj->fp = popen(view_struct->system_command, "w");

		if(!obj->fp)
		  {
			FE_Alert(window_id, XP_GetString(XP_ALERT_UNABLE_INVOKEVIEWER));
		    return(NULL);
		  }

		obj->stream_block_size = view_struct->stream_block_size;

		signal(SIGPIPE, SIG_IGN);

      }
	else
      {
		
		dot = XP_STRRCHR(path, '.');

#ifdef XP_UNIX
		/* Gag.  foo.ps.gz --> tmpXXXXX.ps, not tmpXXXXX.gz. */
		if (dot && fe_encoding_extensions)
		  {
			int i = 0;
			while (fe_encoding_extensions [i])
			  {
				if (!XP_STRCMP (dot, fe_encoding_extensions [i]))
				  {
					*dot = 0;
					dot--;
					while (dot > path && *dot != '.')
					  dot--;
					if (*dot != '.')
					  dot = 0;
					break;
				  }
				i++;
			  }
		  }
#endif /* XP_UNIX */

		tmp_filename = WH_TempName(xpTemporary, "MO");
		if (!tmp_filename) {
			FREEIF(stream);
			FREEIF(obj);
			return NULL;
		}
		if (dot)
		  {
			  char * p_tmp;

			  StrAllocCopy(obj->filename, tmp_filename);

			  /* restrict to ascii alphanumeric chars
			   *
			   * this fixes really bad security hole
			   */
			  for(p_tmp = dot+1; *p_tmp != '\0'; p_tmp++)
				  if( (*p_tmp >= '0' && *p_tmp <= '9')
					  || (*p_tmp >= 'A' && *p_tmp <= 'Z')
					  || (*p_tmp >= 'a' && *p_tmp <= 'z')
					  || (*p_tmp == '_')
					  || (*p_tmp == '+')
					  || (*p_tmp == '-'))
				  {
					  /* this is a good character.  Allow it.
					   */
				  }
				  else
				  {
					  *p_tmp = '\0';
					  break;
				  }
				  
			  StrAllocCat(obj->filename, dot);
		  }
		else
		  {
			  StrAllocCopy(obj->filename, tmp_filename);
		  }
		
        FREE(path);
		XP_FREE(tmp_filename);

		obj->fp = XP_FileOpen(obj->filename, xpTemporary, XP_FILE_WRITE);
			
		TRACEMSG(("Trying to open output file: %s\n", obj->filename));
    
        if(!obj->fp)
		  {
			char *s = NET_ExplainErrorDetails (MK_UNABLE_TO_OPEN_TMP_FILE,
											   obj->filename);
			if (s)
			  {
				FE_Alert (window_id, s);
				XP_FREE (s);
			  }
	        return(NULL);
		  }

		/* construct the command like this
		 *
		 * (( COMMAND ); rm %s )&
		 */
        StrAllocCopy(obj->command, "((");

		/* this is a stream writable program that the user wants
		 * to use non streaming
		 */
		if(view_struct->stream_block_size)
			StrAllocCat(obj->command, "cat %s | ");

        StrAllocCat(obj->command, view_struct->system_command);

		PR_snprintf(small_buf, sizeof(small_buf), "); rm %.200s )&", obj->filename);
		StrAllocCat(obj->command, small_buf);
	  }

	StrAllocCopy(obj->url, URL_s->address);
    
    TRACEMSG(("Returning stream from NET_ExtViewer\n"));

    return stream;
}

#endif /* MOZILLA_CLIENT */
