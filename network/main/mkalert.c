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
#include "mkutils.h"
#include "mkgeturl.h"

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

PUBLIC Bool
FE_SecurityDialog(MWContext * context, int message)
{
   switch(message)
     {
	    case SD_INSECURE_POST_FROM_SECURE_DOC:
		case SD_INSECURE_POST_FROM_INSECURE_DOC:
		case SD_ENTERING_SECURE_SPACE:
		case SD_LEAVING_SECURE_SPACE:
		case SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN:
		case SD_REDIRECTION_TO_INSECURE_DOC:
		case SD_REDIRECTION_TO_SECURE_SITE:
		printf("Security message: %d", message);
	}
}

void
TESTFE_AllConnectionsComplete(MWContext * context)
{
}

void
TESTFE_EnableClicking(MWContext * context)
{
}

void 
XFE_SetProgressBarPercent(MWContext * context, int percent)
{
}

PUBLIC const char * 
FE_UsersMailAddress(void)
{
	return("montulli@netscape.com");
}

PUBLIC const char * 
FE_UsersFullName(void)
{
	return(NULL);
}

extern void sample_exit_routine(URL_Struct *URL_s,int status,MWContext *window_id);

PUBLIC void 
FE_EditMailMessage (MWContext *context,
                               const char * to_address,
                               const char * subject,
                               const char * newsgroups,
                               const char * references,
                               const char * news_url)
{
#if 0
	URL_Struct * URL_s;
	char buffer[356];

	XP_SPRINTF(buffer, "mailto:%.256s", to_address);

	URL_s = NET_CreateURLStruct(buffer, FALSE);

	StrAllocCopy(URL_s->post_headers,"Subject: This is a test\r\nX-URL: http://bogus\r\n");
	StrAllocCopy(URL_s->post_data,"This is a test, this is only a test\n");
	URL_s->method = URL_POST_METHOD;

    NET_GetURL(URL_s, FO_CACHE_AND_PRESENT, (MWContext *)0 ,sample_exit_routine);

	return;
#endif
}


PUBLIC void
FE_ConnectToRemoteHost(MWContext * window_id, int url_type, char * hostname, char * port, char * username)
{
}


extern NET_StreamClass *
IL_NewStream             (int         format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *context)
{
return(NULL);
}

fe_MakeViewSourceStream (int         format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *context)
{
return(NULL);
}

fe_MakeMailToStream (int         format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *context)
{
return(NULL);
}


fe_MakePPMStream      (int         format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *context)
{
return(NULL);
}

Bool
TESTFE_ShowAllNewsArticles(MWContext * window_id)
{
return(FALSE);
}


intn
LO_Format(void * data_object, PA_Tag *tag, intn status)
{
return(0);
}


void
XP_Trace (const char* message, ...)
{
    int actualLen;
    static char xp_Buffer[2048];
    va_list stack;

    va_start (stack, message);
    actualLen = vsprintf (xp_Buffer, message, stack);
    va_end (stack);

    fwrite(xp_Buffer, 1, strlen(xp_Buffer), stderr);
    fprintf (stderr, "\n");
}



PUBLIC int32 FE_GetContextID(MWContext * window_id)
{
   return((int) window_id);
}

PUBLIC int TESTFE_FileSortMethod(MWContext * window_id)
{
   return(SORT_BY_NAME);
}

PUBLIC Bool TESTFE_UseFancyNewsgroupListing (MWContext * window_id)
{
   return(TRUE);
}

PUBLIC Bool TESTFE_UseFancyFTP(MWContext * window_id)
{
   return(1);
}

PUBLIC int TESTFE_CheckForInterrupt(void * window_id)
{
   /* check check check check and check again */
   return(0);
}

PUBLIC void TESTFE_Spinner (MWContext * window_id)
{
  /* big wheel keep on rolling. Proud Mary keep on.... */
}

PUBLIC void TESTFE_Alert (MWContext * window_id, CONST char * mess)
{
    TRACEMSG(("WWW Alert:  %s\n", mess));
}

PUBLIC void TESTFE_Progress (MWContext * window_id, CONST char * mess)
{
    TRACEMSG(("   %s ...\n", mess));
}

PUBLIC void TESTFE_GraphProgressInit (MWContext * window_id, URL_Struct *url, int32 total_length)
{
    TRACEMSG(("  GraphInit: %s is %d long...\n", url->address, total_length));
}

PUBLIC void TESTFE_GraphProgressDestroy (MWContext * window_id, URL_Struct *url, int32 bytes_transferred, int32 total_length)
{
    TRACEMSG(("  GraphDestroy: %s is %d long...\n", url->address, total_length));
}

PUBLIC void TESTFE_GraphProgress (MWContext * window_id, URL_Struct *url, int32 cur_length,  int32 length_delta, int32 total_length)
{
    TRACEMSG(("  GraphProgress: %d of %d ...\n", cur_length, total_length));
}

PUBLIC Bool TESTFE_Confirm (MWContext * window_id, CONST char * mess)
{
  char Reply[4];
  char *URep;
  
  fprintf(stderr, "WWW: %s (y/n) ", mess);
                      

  fgets(Reply, 4, stdin); /* get reply, max 3 characters */
  URep=Reply;
  while (*URep) {
    if (*URep == '\n') {
	*URep = (char)0;	/* Overwrite newline */
	break;
    }
    *URep=toupper(*URep);
    URep++;	/* This was previously embedded in the TOUPPER */
                /* call an it became evaluated twice because   */
  }

  if ((XP_STRCMP(Reply,"YES")==0) || (XP_STRCMP(Reply,"Y")==0))
    return(YES);
  else
    return(NO);
}

/*	Prompt for answer and get text back
*/
PUBLIC char * TESTFE_Prompt (MWContext * window_id, CONST char * mess, CONST char * deflt)
{
    char temp[512];
    char * t_string = 0;

    fprintf(stderr, "Prompt: %s", mess);
    if (deflt) fprintf(stderr, " (RETURN for [%s]) ", deflt);
    
    fgets(temp, 200, stdin);
    temp[XP_STRLEN(temp)-1] = (char)0;	/* Overwrite newline */
   
    StrAllocCopy(t_string, *temp ? temp : deflt);
    return t_string;
}


/*	Prompt for password without echoing the reply
*/
PUBLIC char * TESTFE_PromptPassword (MWContext * window_id, CONST char * mess)
{
    char *result = NULL;
    char pw[80];

	printf("%s",mess ? mess : "Type your password:");
    scanf("%s",pw);

    StrAllocCopy(result, pw);
    return result;
}


PUBLIC void TESTFE_PromptUsernameAndPassword (MWContext * window_id, 
					  CONST char *	mess,
					  char **		username,
					  char **		password)
{
    if (mess)
	fprintf(stderr, "WWW: %s\n", mess);
    *username = TESTFE_Prompt(window_id, "Username: ", *username);
    *password = TESTFE_PromptPassword(window_id, "Password: ");
}

