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
 * register all neccessary
 * stream converters
 */

/* Please leave outside of ifdef for window's precompiled headers. */
#include "mkutils.h"

#ifdef MOZILLA_CLIENT
#ifdef XP_UNIX  /* this whole module is ifdef'd UNIX */

#include "mkgeturl.h"
#include "mkstream.h"
#include "mkformat.h"
#include "net.h"

#include "cvview.h"
#include "cvdisk.h"
#include "cvproxy.h"
#include "cvextcon.h"
#include "txview.h"

#include "xlate.h"

#include "libi18n.h"		/* For character code set conversion stream	*/

#include "cvactive.h"

#include "pa_parse.h"

#include "np.h"

#include "il_strm.h"            /* Image Library stream converters. */

/* The master database of mail cap info (link list) */

PRIVATE XP_List *NET_mcMasterList = 0; /* May want to save out this info */

PUBLIC void
NET_mdataFree(NET_mdataStruct *md)
{
	FREEIF(md->contenttype);
	FREEIF(md->command);
	FREEIF(md->testcommand);
	FREEIF(md->label);
	FREEIF(md->printcommand);
	FREEIF(md->src_string);
	FREEIF(md->xmode);
	FREE(md);
	md = NULL;
}

/* File is a directory ?*/
PRIVATE Bool
net_isDir(char *name)
{
    XP_StatStruct st;
    if (!name || !*name) return (FALSE);
    if (!stat (name, &st) && S_ISDIR(st.st_mode))
        return (TRUE);
    else
        return (FALSE);
}

PUBLIC void
NET_CleanupMailCapList(char* filename)
{
	NET_mdataStruct *mdata;
	FILE *fp = NULL;
	XP_List *modifiedList = NULL;

	if ( !NET_mcMasterList ) return;

	if ( filename && *filename )
	{
	  /* Invalid filename, then don't write */
	  if ( net_isDir(filename) )
		return;
	  fp = fopen(filename, "w");
	  if ( !fp ) return;
	  modifiedList = XP_ListNew();

	  while ((mdata=(NET_mdataStruct *)XP_ListRemoveEndObject(NET_mcMasterList)))
	  {
		if (mdata->is_local )
		{
		if ( mdata->src_string && *mdata->src_string)
		{
		fputs(mdata->src_string, fp);
		if ( mdata->src_string[strlen(mdata->src_string)-1] != '\n' )
		   fputs("\n", fp);
		}
		else mdata->src_string = 0;

		NET_mdataFree(mdata);
		}
		else if (mdata->is_modified)
			XP_ListAddObject(modifiedList, mdata);
		else 
		{
			NET_mdataFree(mdata);
		}
	  }
	 while ((mdata=(NET_mdataStruct *)XP_ListRemoveEndObject(modifiedList)))
          {
		if ( mdata->src_string && *mdata->src_string)
		{
		fputs(mdata->src_string, fp);
		if ( mdata->src_string[strlen(mdata->src_string)-1] != '\n' )
		  fputs("\n", fp);
		}
		else mdata->src_string = 0;
                NET_mdataFree(mdata);
          }

	  fclose(fp);
	}
	else
	{
	  while ((mdata = (NET_mdataStruct *)XP_ListRemoveEndObject(NET_mcMasterList)))
          {
		NET_mdataFree(mdata);
	  }
	}
	XP_ListDestroy(NET_mcMasterList);
	NET_mcMasterList = 0;
}

/* register all your converters
 *
 * YOU MUST register a "*" converter for
 * EVERY format out type, this acts as a default
 * and prevents a "Could not convert error"
 */
PUBLIC void
NET_RegisterConverters (char * personal_file, char * global_file)
{


  NET_CleanupMailCapList(NULL);
  NET_mcMasterList = XP_ListNew();

  NET_RegisterMIMEDecoders ();

  NET_RegisterExternalViewerCommand (IMAGE_WILDCARD, "xv %s", 0);

  NET_RegisterExternalViewerCommand ("audio/ulaw", "playulaw", 2000);

#ifdef NOT  /* NO default audio */
#ifdef __sun
  NET_RegisterExternalViewerCommand (AUDIO_BASIC, "cat > /dev/audio", 2000);
#else
  NET_RegisterExternalViewerCommand (AUDIO_BASIC, "playulaw", 2000);
#endif /* __sgi */
#endif /* NOT */

  NET_RegisterExternalViewerCommand (VIDEO_MPEG,
									 /* throw away stderr and stdout, since
										mpeg_play is so darn noisy. */
									 "mpeg_play %s >/dev/null 2>&1", 0);

  NET_RegisterExternalViewerCommand ("image/x-xwd", "xwud", 2000);

  /* global file must come first!!!!!!!!!!!!!!
   */
  if (global_file)
	NET_ProcessMailcapFile (global_file, FALSE);

  if (personal_file)
	NET_ProcessMailcapFile (personal_file, TRUE);
}

/*
 * before you modify this,
 * make darn sure you want to deviuate from the standard bellcore mailcap
 * handling!!  *this means you Garrett!!*"
 *
 * Code borrowed from:
 *
 *  Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)
 *
 *  Permission to use, copy, modify, and distribute this material
 *  for any purpose and without fee is hereby granted, provided
 *  that the above copyright notice and this permission notice
 *  appear in all copies, and that the name of Bellcore not be
 *  used in advertising or publicity pertaining to this
 *  material without the specific, prior written permission
 *  of an authorized representative of Bellcore.  BELLCORE
 *  MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
 *  OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
 *  WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
 */

/*
 *  Metamail -- A tool to help diverse mail readers
 *              cope with diverse multimedia mail formats.
 *
 *  Author:  Nathaniel S. Borenstein, Bellcore
 *
 */

struct MailcapEntry {
    char *contenttype;
    char *command;
    char *testcommand;
    int needsterminal;
    int copiousoutput;
    int needtofree;
    char *label;
    char *printcommand;
	int32 stream_buffer_size;
    char *xmode;
};

#define LINE_BUF_SIZE      2048
#define TMPFILE_NAME_SIZE  127

PRIVATE char *GetCommand (char *s, char **t)
{
    char *s2;
    int quoted = 0;

    s2 = (char *) PR_Malloc(PL_strlen(s)*2 + 3); /* absolute max, if all % signs */
	if(!s2)
		return(NULL);
    *s2 = 0;
    *t = s2;


    while (s && *s)
	  {
	    if (quoted)
		  {
            if (*s == '%')
				*s2++ = '%';

            *s2++ = *s++;
	        quoted = 0;
	      }
		else
		  {
	        if (*s == ';')
			  {
				break;  /* finish the string below */
	          }
	        if (*s == '\\')
			  {
		        quoted = 1;
		        ++s;
	          }
			else
			  {
		        *s2++ = *s++;
	          }
	      }
      }

	/* look at the last character of the command and make sure it doesn't have
	 * a '&'
	 */
	if( s2 > *t && *(s2-1) == '&')
		s2--;  /* kill the ampersand */

    *s2 = 0;  /* terminate string */

	if(*s)
        return(s+1);
	else
        return(s);
}

/* Trims leading and trailing space in the input string while converting
 * it to all lower case.
 *
 * WARNING: Modified input string.
 */
PRIVATE char *Cleanse (char *s)
{
    char *tmp, *news, *tmpnews;

    news = s;
    /* strip leading white space */
    while (*s && XP_IS_SPACE(*s)) ++s;
    /* put in lower case */
    for (tmpnews=news, tmp=s; *tmp; tmpnews++, ++tmp) {
      *tmpnews = tolower ((unsigned char)*tmp);
    }
	*tmpnews = '\0';
    /* strip trailing white space */
    while (*--tmpnews && XP_IS_SPACE(*tmpnews)) *tmpnews = 0;
    return(news);
}

PRIVATE void BuildCommand (char *Buf, char *controlstring, char *TmpFileName)
{
    char *from, *to;
    int prefixed = 0;

    for (from=controlstring, to=Buf; *from != '\0'; from++)
      {
        if (prefixed)
          {
            prefixed = 0;
            switch(*from)
              {
                case '%':
                    *to++ = '%';
                    break;
                case 'n':
                case 'F':
                     fprintf(stderr, "metamail: Bad mailcap \"test\" clause: %s\n", controlstring);
                case 's':
                    if (TmpFileName)
                      {
                        PL_strcpy(to, TmpFileName);
                        to += PL_strlen(TmpFileName);
                      }
                    break;
                default:
                    fprintf(stderr,
				"%s: Ignoring unsupported format code in mailcap file: %%%c\n",
      						XP_AppName, *from);
                    break;
              }
          }
        else if (*from == '%')
          {
            prefixed = 1;
          }
        else
          {
            *to++ = *from;
          }
      }
    *to = 0;
}

static void
net_register_new_converter(char *contenttype, char *command, char *xmode,
						   int buffer_size)
{
		/* xmode takes priority over command */
        if (xmode && !PL_strcasecmp(xmode, NET_COMMAND_NETSCAPE))
        {
                if ( !PL_strcasecmp(contenttype, TEXT_HTML) )
                        NET_RegisterContentTypeConverter (TEXT_HTML, FO_PRESENT,
                                                NULL, INTL_ConvCharCode);
                else if (!PL_strcasecmp(contenttype, TEXT_MDL))
                        NET_RegisterContentTypeConverter (TEXT_MDL, FO_PRESENT,
                                                NULL, INTL_ConvCharCode);

                else if (!PL_strcasecmp(contenttype, TEXT_PLAIN))
                        NET_RegisterContentTypeConverter (TEXT_PLAIN, FO_PRESENT,
                                                NULL, NET_PlainTextConverter);
                else if (!PL_strcasecmp(contenttype, IMAGE_GIF))
                        NET_RegisterContentTypeConverter (IMAGE_GIF,
                                 FO_PRESENT,NULL, IL_ViewStream);
                else if (!PL_strcasecmp(contenttype, IMAGE_XBM) ||
						 !PL_strcasecmp(contenttype, IMAGE_XBM2) ||
						 !PL_strcasecmp(contenttype, IMAGE_XBM3))
                        NET_RegisterContentTypeConverter (IMAGE_XBM,
                                 FO_PRESENT,NULL, IL_ViewStream);
                else if (!PL_strcasecmp(contenttype, IMAGE_JPG) ||
						 !PL_strcasecmp(contenttype, IMAGE_PJPG))
                        NET_RegisterContentTypeConverter (IMAGE_JPG,
                                 FO_PRESENT,NULL, IL_ViewStream);

                else if (!PL_strcasecmp(contenttype, IMAGE_PNG)) 
                        NET_RegisterContentTypeConverter (IMAGE_PNG,
                                 FO_PRESENT,NULL, IL_ViewStream);
                else if (!PL_strcasecmp(contenttype,
									 APPLICATION_NS_PROXY_AUTOCONFIG))
                        NET_RegisterContentTypeConverter(
                                APPLICATION_NS_PROXY_AUTOCONFIG,
                                FO_PRESENT,
                                (void *)0, NET_ProxyAutoConfig);
                else if (!PL_strcasecmp(contenttype,
									 APPLICATION_NS_JAVASCRIPT_AUTOCONFIG))
                        NET_RegisterContentTypeConverter(
                                APPLICATION_NS_JAVASCRIPT_AUTOCONFIG,
                                FO_PRESENT,
                                (void *)0, NET_ProxyAutoConfig);
                else
                {
                    fprintf(stderr,
		    "%s: Ignoring unsupported netscape contenttype in user mailcap file.\n", XP_AppName);
                }
        }
        else if (xmode && !strncasecmp(xmode, NET_COMMAND_PLUGIN,
									   strlen(NET_COMMAND_PLUGIN)))
		  {
			char *pluginName = xmode + strlen(NET_COMMAND_PLUGIN);
			/* For plugins enable the plugin for this mimetype */
			NPL_EnablePlugin(contenttype, pluginName, TRUE);
		  }
        else if (xmode &&
				 (!PL_strcasecmp(xmode, NET_COMMAND_SAVE_TO_DISK) || 
				  !PL_strcasecmp(xmode, NET_COMMAND_SAVE_BY_NETSCAPE)))
                NET_RegisterContentTypeConverter(contenttype,
                                FO_PRESENT,
                                NULL, fe_MakeSaveAsStream );
 
        else if (xmode && !PL_strcasecmp(xmode, NET_COMMAND_UNKNOWN))
                NET_RegisterContentTypeConverter(contenttype,
                                FO_PRESENT,
                                NULL, fe_MakeSaveAsStream );
 
        else if (xmode && !PL_strcasecmp(xmode,NET_COMMAND_DELETED))
		  {
			/* Do nothing */
		  }
        else if (command && *command )/* It's an external application then */
	{

        /* register the command
         *
         * but only if it isn't text/html
         */
        if(PL_strcasecmp(contenttype, TEXT_HTML) &&
		   PL_strcasecmp(contenttype, MESSAGE_RFC822) &&
		   PL_strcasecmp(contenttype, MESSAGE_NEWS))
		  NET_RegisterExternalViewerCommand(contenttype, command, buffer_size);
	}
}


PRIVATE int PassesTest (struct MailcapEntry *mc)
{
    int result;
    char *cmd, TmpFileName[TMPFILE_NAME_SIZE];

    if (!mc->testcommand) return(1);
    tmpnam(TmpFileName);
    cmd = (char *)PR_Malloc(1024);
    if (!cmd)
        return(0);
    BuildCommand(cmd, mc->testcommand, TmpFileName);
    TRACEMSG(("Executing test command: %s\n", cmd));
    result = system(cmd);
    free(cmd);

    remove(TmpFileName);

    if(result)
        TRACEMSG(("[HTInit] Test failed!\n"));
    else
        TRACEMSG(("[HTInit] Test passed!\n"));

    return(result==0);
}

PRIVATE int ProcessMailcapEntry (FILE *fp, struct MailcapEntry *mc,
			char** src_string, Bool is_local)
{
	NET_cdataStruct *tmp_cd = NULL;
	NET_cdataStruct *cd = NULL;
	NET_mdataStruct *md = NULL;
    int unprocessed_entryalloc = 1023, len;
    char *unprocessed_entry, *s, *t, *buffer;
    char *arg, *eq;
    char *command=0;
	
    buffer = (char *) PR_Malloc(LINE_BUF_SIZE);
    if (!buffer)
	    return 0;
    unprocessed_entry = (char *) PR_Malloc(1 + unprocessed_entryalloc);
    if (!unprocessed_entry)
	  {
		free(buffer);
	    return 0;
	  }
    *unprocessed_entry = '\0';
    while (fgets(buffer, LINE_BUF_SIZE, fp))
      {
		if ( !*src_string )
			StrAllocCopy(*src_string, buffer );
		else
			*src_string = XP_AppendStr(*src_string, buffer);
		
	    if (buffer[0] == '#')
	        continue;
        len = PL_strlen(buffer);
        if (len == 0)
	        continue;
        if (buffer[len-1] == '\n')
		    buffer[--len] = 0;
	    if ((len + PL_strlen(unprocessed_entry)) > unprocessed_entryalloc)
          {
	        unprocessed_entryalloc += 1024;
	        unprocessed_entry = (char *) PR_Realloc(unprocessed_entry, unprocessed_entryalloc+1);
			TRACEMSG(("Growing input line in mkinit"));
	        if (!unprocessed_entry)
			  {
			    TRACEMSG(("ERROR! Growing input line in mkinit"));
		        return 0;
			  }
	      }
		if (len > 0 && buffer[len-1] == '\\')
		  {
			buffer[len-1] = 0;
			PL_strcat(unprocessed_entry, buffer);
		  }
	    else
          {
			PL_strcat(unprocessed_entry, buffer);
			break;
		  }
      }
	
    for (s=unprocessed_entry; *s && XP_IS_SPACE(*s); ++s)
		; /* NULL BODY */
    if (!*s)
      {
	    /* totally blank entry -- quietly ignore */
	    free(unprocessed_entry);
    	free(buffer);
	    return(0);
      }
    s = PL_strchr(unprocessed_entry, ';');
    if (s == NULL)
      {
	    fprintf(stderr, "%s: Ignoring invalid mailcap entry: %s\n",
				XP_AppName, unprocessed_entry);
	    free(unprocessed_entry);
    	free(buffer);
		PR_Free(*src_string);
		*src_string = 0;
	    return(0);
      }
    *s++ = 0;
    mc->needsterminal = 0;
    mc->copiousoutput = 0;
    mc->needtofree = 1;
    mc->testcommand = 0;
    mc->label = NULL;
    mc->printcommand = NULL;
	mc->contenttype = 0; /* init */
    mc->xmode = 0;
	mc->stream_buffer_size = 0;
	StrAllocCopy(mc->contenttype, unprocessed_entry); /* copy */
    if (!mc->contenttype)
	  {
    	free(buffer);
		TRACEMSG(("mailcap: no content-type"));
	    return 0;
	  }
    s = GetCommand(s, &mc->command);
	
    while (s && *s != '\0')
      {
		arg = s;
        eq = PL_strchr(arg, '=');
        if (eq)
			*eq++ = 0;
		else
			eq = arg;
		
		t = GetCommand(eq, &command);
		
	    if (arg && *arg)
	      {
	        arg = Cleanse(arg);
			if ( eq && !PL_strcmp(arg, NET_MOZILLA_FLAGS))
			  {
				/* xmode is case sensitive. That is why we dont Cleanse it. */
				mc->xmode = command;
				command = 0;
			  }
			else {
			  command = Cleanse(command);
			  if (eq && !PL_strcmp(arg, "test"))
				{
				  mc->testcommand = command;
				  command = 0;
				  TRACEMSG(("mailcap: found testcommand:%s\n",mc->testcommand));
				}
			  else if (eq && !PL_strcmp(arg, "description"))
				{
				  mc->label = command;
				  command = 0;
				}
			  else if (eq && !PL_strcmp(arg, "label"))
				{
				  mc->label = command; /* bogus old name for description */
				  command = 0;
				}
			  else if (eq && !PL_strcmp(arg, "stream-buffer-size"))
				{
				  mc->stream_buffer_size = atol(command);
				  TRACEMSG(("mailcap: found stream-buffer-size:%d\n",mc->stream_buffer_size));
				  FREEIF(command);
				  command = 0;
				}
			  else
				  FREEIF(command);
			}
	      }
		else if (command && *command) FREEIF(command);
	    s = t;
      }
	
	free(unprocessed_entry);
	
    /* Copy over the mail cap entry so that it can saved into data base */
	
    if (mc)
	  {
		md = NET_mdataCreate();
		
		if ( !md ) return (-1);
		StrAllocCopy(md->contenttype, mc->contenttype);
		StrAllocCopy(md->command, mc->command);
		StrAllocCopy(md->testcommand, mc->testcommand);
		StrAllocCopy(md->label, mc->label);
		StrAllocCopy(md->printcommand, mc->printcommand);
		md->stream_buffer_size = mc->stream_buffer_size;
		if (is_local && *src_string && **src_string)
			StrAllocCopy(md->src_string, *src_string); /* copy buffer */
		StrAllocCopy(md->xmode, mc->xmode);
		md->is_local = is_local;
		PR_Free(*src_string);
		*src_string = 0;
	  }


    if (PassesTest(mc))
      {
		TRACEMSG(("mailcap: Setting up conversion %s : %s\n", mc->contenttype,
				  mc->command));
		
		net_register_new_converter(XP_StripLine(mc->contenttype), 
								   mc->command, mc->xmode,
								   mc->stream_buffer_size);
		if (md)
			md->test_succeed = TRUE;
      }
	
    /* Add to the database */
    if (md)
	  {
        NET_mdataStruct *old_md = NULL;
		
        if ( !(old_md = NET_mdataExist(md)))
    		NET_mdataAdd(md);
		else
		  {
			NET_mdataRemove(old_md);
			NET_mdataAdd(md);
		  }
		
		cd = NET_cdataCreate();
		cd->ci.type = 0;
		cd->is_external = 1;
		cd->is_local = 0; /* This is internal; only for user to view */
		StrAllocCopy(cd->ci.type, md->contenttype );
		
		/* Does not exist add this contenttype to mime content list  */
		if ( !(tmp_cd = NET_cdataExist(cd) ) )
			NET_cdataAdd(cd);
		else /* Already Exist, so we don't need to add it again. Then, free it now */
			NET_cdataRemove(cd);
	  }
	
    free(buffer);
    FREEIF(mc->contenttype);
    FREEIF(mc->command);
    FREEIF(mc->testcommand);
    FREEIF(mc->label);
    FREEIF(mc->xmode);
    return(1);
}

/* reads a mailcap file and adds entries to the
 * external viewers converter list
 */
PUBLIC int
NET_ProcessMailcapFile (char *file, Bool is_local)
{
    struct MailcapEntry mc;
    FILE *fp;
    char *src_string = 0;

    TRACEMSG(("Loading types config file '%s'\n", file));

    if ( !net_isDir(file) )
    {
    	fp = fopen(file, "r");

    	while (fp && !feof(fp))
      	{
        	ProcessMailcapEntry(fp, &mc, &src_string, is_local);
      	}
    	if (fp) fclose(fp);
    }
    return(-1);
}


PUBLIC XP_List *
mailcap_MasterListPointer(void)
{
	return (NET_mcMasterList);
}

PUBLIC NET_mdataStruct *
NET_mdataCreate(void)
{
	NET_mdataStruct *md = PR_NEW(NET_mdataStruct);

        if(!md)
           return(NULL);

        memset(md, 0, sizeof(NET_mdataStruct));

    return md;
}

PUBLIC void
NET_mdataAdd(NET_mdataStruct *md)
{
    XP_ListAddObject(NET_mcMasterList, md);
}


PUBLIC void
NET_mdataRemove(NET_mdataStruct *md)
{
    XP_ListRemoveObject(NET_mcMasterList, md);

    NET_mdataFree(md);

}

PUBLIC NET_mdataStruct*
NET_mdataExist(NET_mdataStruct *old_md )
{
        NET_mdataStruct *found_md = NULL;
        NET_mdataStruct *md = NULL;
        XP_List *infoList;

        infoList = mailcap_MasterListPointer();

        if ( !infoList )
                return found_md;

        while ((md= (NET_mdataStruct *)XP_ListNextObject(infoList)))
        {
                if ( old_md->contenttype &&
                     md->contenttype &&
                     !PL_strcasecmp(old_md->contenttype, md->contenttype))
                {
                        /* found matching type */
                        found_md = md;
                        break;
                }
        }
        return found_md;
}



#endif /* XP_UNIX */
#endif /* MOZILLA_CLIENT */
