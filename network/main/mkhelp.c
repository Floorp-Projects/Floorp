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
/***************************************************
 *
 * Routines to parse HTML help mapping file
 */

#include "mkutils.h"
#include "xp.h"
#include "mkparse.h"
#include "mkhelp.h"
#include "xp_help.h"
#include "xpgetstr.h"
#include "libmocha.h"		/* For the onHelp handler */
#include "prefapi.h"		/* For the onHelp handler */
#include "fe_proto.h"

extern char * INTL_ResourceCharSet(void);

extern int MK_OUT_OF_MEMORY;
extern int MK_CANT_LOAD_HELP_TOPIC;

#define DEFAULT_HELP_ID			   "*"
#define DEFAULT_HELP_PROJECT	   "/help.hpf"  	/* Note the inclusion of the initial slash */
#define DEFAULT_HELP_WINDOW_NAME   "_HelpWindow"
#define DEFAULT_HELP_WINDOW_HEIGHT 500
#define DEFAULT_HELP_WINDOW_WIDTH  400

#define PREF_NSCP_HELP_URL		   "http://help.netscape.com/nethelp/"	/* This must include the trailing slash */
#define PREF_DEFAULT_HELP_LOC_ID   "general.help_source.site"	/* FIXME: Shouldn't this be defined somewhere else? */
#define PREF_DEFAULT_HELP_URL_ID   "general.help_source.url"	/* FIXME: Shouldn't this be defined somewhere else? */

/* Tokens within the .hpf file to parse for */

#define ID_MAP_TOKEN "ID_MAP"
#define END_ID_MAP_TOKEN "/ID_MAP"
#define FRAME_GROUP_TOKEN "FRAME_GROUP"
#define SRC_TOKEN "SRC"
#define WINDOW_TOKEN "DEFAULT_WINDOW"
#define END_FRAME_GROUP_TOKEN "/FRAME_GROUP"
#define WINDOW_SIZE_TOKEN "WINDOW-SIZE"
#define WINDOW_NAME_TOKEN "WINDOW-NAME"
#define HELP_VERSION_TOKEN "NETHELP-VERSION"
#define TARGET_TOKEN "TARGET"


/* @@@ Global hack.  See below */
PRIVATE URL_Struct * frame_content_for_pre_exit_routine=NULL;

typedef struct _HTMLHelpParseObj {
	XP_Bool  in_id_mapping;
	int32	 window_width;
	int32	 window_height;
	int		 helpVersion;
	char	*window_name;
	char    *id_value;
	char    *default_id_value;
	char    *url_to_map_file;
	char    *id;
	char    *line_buffer;
	int32    line_buffer_size;
	char    *content_target;
	XP_List *frame_group_stack;
} HTMLHelpParseObj;

typedef struct {
	char *address;
	char *target;
} frame_set_struct;


PRIVATE void
simple_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
	if(status != MK_CHANGING_CONTEXT)
		NET_FreeURLStruct(URL_s);
}


PUBLIC void
XP_NetHelp(MWContext *pContext, const char *topic)
{
	MWContext	*pActiveContext = NULL;
	char		*pHelpURLString = NULL;

	/* Prepend the vendor name "netscape/" to all of our own topics */

	if (topic == NULL) {
		pHelpURLString = XP_STRDUP("netscape/home");
	} else {
		pHelpURLString = (char *) XP_ALLOC(strlen(topic) + strlen("netscape/")+1);
		if (!pHelpURLString) {
			return;
		}
		XP_STRCPY(pHelpURLString, "netscape/");
		XP_STRCPY(&(pHelpURLString[9]), topic);
	}

	/* Now get the right context to load it from */

	if (pContext != NULL) {
		pActiveContext = pContext;
	} else {
		pActiveContext = FE_GetNetHelpContext();
	}

	NET_LoadNetHelpTopic(pActiveContext, pHelpURLString);	

	XP_FREEIF(pHelpURLString);
}


PUBLIC void
NET_LoadNetHelpTopic(MWContext *pContext, const char *topic)
{
	char		*pNetHelpURLString;
	URL_Struct	*pHelpURL;

	if (topic == NULL) {
		return;
	}
	
	/* Convert the fully-specified topic into a nethelp URL: */
	
	pNetHelpURLString = (char *) XP_ALLOC(strlen(topic) + strlen(NETHELP_URL_PREFIX)+1);
	if (!pNetHelpURLString) {
		return;
	}
	
	XP_STRCPY(pNetHelpURLString, NETHELP_URL_PREFIX);
	XP_STRCPY(&(pNetHelpURLString[strlen(NETHELP_URL_PREFIX)]), topic);
	
	pHelpURL = NET_CreateURLStruct(pNetHelpURLString, NET_NORMAL_RELOAD);

	if (!pHelpURL) {
		return;
	}

	NET_GetURL(pHelpURL, FO_PRESENT, pContext, simple_exit);

	XP_FREEIF(pNetHelpURLString);
		
}
	
PRIVATE void
net_help_free_frame_group_struct(frame_set_struct *obj)
{
	XP_ASSERT(obj);
	if(!obj)
		return;
	FREEIF(obj->address);
	FREEIF(obj->target);
	FREE(obj);
}

/* load the HTML help mapping file and search for
 * the id or text to load a specific document
 */
PUBLIC void
NET_GetHTMLHelpFileFromMapFile(MWContext *context,
							   char *map_file_url,
							   char *id,
							   char *search_text)
{
	URL_Struct *URL_s;

	XP_ASSERT(map_file_url && id);

	if(!map_file_url || !id)
		return;

	URL_s = NET_CreateURLStruct(map_file_url, NET_DONT_RELOAD);

	if(!URL_s)
		return;

	URL_s->fe_data = XP_STRDUP(id);

	NET_GetURL(URL_s, FO_CACHE_AND_LOAD_HTML_HELP_MAP_FILE, context, simple_exit);
}

PRIVATE void
net_get_default_help_URL(char **pHelpBase)
{
	int		success;
	int32	helpType;
	
	if ((success = PREF_GetIntPref(PREF_DEFAULT_HELP_LOC_ID, &helpType))
		== PREF_NOERROR) {

		switch (helpType) {

			case 0:		/* Netscape's help site: */
				StrAllocCopy(*pHelpBase, PREF_NSCP_HELP_URL);
				break;
			case 1:		/* Internal installed location. */
				*pHelpBase = FE_GetNetHelpDir();
				break;
			case 2:
				success = PREF_CopyCharPref(PREF_DEFAULT_HELP_URL_ID, pHelpBase);
				break;
			default:
				success = PREF_ERROR;
		}
		/* ...any of the above might accidentally _not_ end with a '/'*/
		if ((*pHelpBase) && ((*pHelpBase)[XP_STRLEN(*pHelpBase)-1]) != '/') {
			StrAllocCat(*pHelpBase, "/");
		}
	}

	/* Fall back on the Netscape help site */

	if (success != PREF_NOERROR) {
		StrAllocCopy(*pHelpBase, PREF_NSCP_HELP_URL);
	}

	return;
}

/* EA: NETHELP Begin */
 /* Takes a nethelp: URL of the form
	 nethelp:vendor/component:topic[@location/[projectfile.hpf]]
	 
	 and separates into the mapping file, with which it replaces URL_s->address,
	 and a topic, which it places into URL_s->fe_data.
	 
	 If a fully specified location (one which includes a projectfile) is given,
	 the mapping file is set to that.  If the given location ends in a slash,
	 then it is taken to be a directory, and the vendor and component are
	 appended as directories before the default projectfile name, helpidx.hpf.
	 If no location is given, the default help location is prepended to the
	 vendor and component and "helpidx.hpf".
	 
	 Examples:
	 
	 nethelp:netscape/navigator:url@http://help.netscape.com/navigator/navhelp.hpf
	   - mapping file: http://help.netscape.com/navigator/navhelp.hpf
	 
	 nethelp:netscape/navigator:url@http://help.netscape.com/
	   - mapping file: http://help.netscape.com/netscape/navigator/help.hpf
	 
	 nethelp:netscape/navigator:url@http://help.netscape.com
	   - mapping file: http://help.netscape.com (this will result in an error)

 	 nethelp:netscape/navigator:url
 	   - assuming the default help location were: C:\navigator\help
 	   - mapping file: file:///C|/navigator/help/netscape/navigator/helpidx.hpf
 	  
 	 In all cases, the topic would be "url"
 	 
 */


PUBLIC int
NET_ParseNetHelpURL(URL_Struct *URL_s)
{
	/* this is a nethelp: URL
	 * first, see if it's local or remote by checking for the @ 
	 */
	char *remote_addr_ptr = 0;
	char *remote_addr=0;
	char *topic_ptr = 0;
	char *topic = 0;
	char *scheme_specific = 0;
	char *pCharacter;

	XP_Bool appendProjFile = FALSE;
	
	remote_addr_ptr = XP_STRCHR(URL_s->address, '@');
	
	if (!remote_addr_ptr) {
		char *default_URL = 0;

		/* it's local, so we need to get the default, then append the project file */
		net_get_default_help_URL(&default_URL);
		
		if (default_URL) {
			StrAllocCopy(remote_addr, default_URL);
			XP_FREE(default_URL);
		}
		
		appendProjFile = TRUE;
		
	} else {
		*remote_addr_ptr = '\0';
		
		StrAllocCopy(remote_addr, remote_addr_ptr+1);

		if (remote_addr && (remote_addr[XP_STRLEN(remote_addr)] == '/')) {
		/* Check to see if the remote_addr ends in a slash.  If so, we
		   have some appending to do */

			appendProjFile = TRUE;
		}
	}

	if (!remote_addr) {
		/* We've obviously run into some kind of a memory problem here. */
		/* Time to bail */
		return MK_OUT_OF_MEMORY;
	}

	/* By now, the URL_s->address has been stripped of any location information */
	/* First, remove the scheme, which is guaranteed to be there. */
	
	scheme_specific = XP_STRCHR(URL_s->address, ':') + 1;
	
	topic_ptr = XP_STRCHR(scheme_specific, ':');
	
	if (!topic_ptr) {
		/* This is an error case, but we'll handle it anyway by defaulting to
		   the generic topic */
		   
		StrAllocCopy(topic, DEFAULT_HELP_ID);
	} else {
		*topic_ptr = '\0';
		StrAllocCopy(topic, topic_ptr+1);

	}		
	
	if (appendProjFile) {
		/* Now the URL_s->address will contain only the vendor/component information */
		
		/* In an act of incredible lameness, we want to lowercase the
		   vendor/component, since we say that these will default to 
		   lower case in the spec.
		   
		   FIXME!: Note that this may not be correct for double-byte encoded
		   characters, but the Intl team was unable to come up with a good
		   solution here; in general, we probably won't have any issue, since
		   URLs themselves should be in an ASCII-encoding (?).
		*/

		pCharacter = scheme_specific;

		while (*pCharacter)
		{
			*pCharacter = (char) XP_TO_LOWER((unsigned int) *pCharacter);
			pCharacter++;
		}

		
		StrAllocCat(remote_addr, scheme_specific);
		StrAllocCat(remote_addr, DEFAULT_HELP_PROJECT);
	}
	
	FREE(URL_s->address);
	URL_s->address = remote_addr;
	
	/* If there is no topic, then we'll still attempt to load the project file and
	   its window.  The other code below should detect the non-existence of a topic and
	   either revert to a default or do some other elegant solution. */
	   
	if (topic) {
		NET_UnEscape(topic);
		URL_s->fe_data = XP_STRDUP(topic);
	} else {
		URL_s->fe_data = NULL;
	}
	
	FREEIF(topic);
	
	return MK_DATA_LOADED;
}


/* EA: NETHELP End */


PRIVATE HTMLHelpParseObj *
net_ParseHTMLHelpInit(char *url_to_map_file, char *id)
{
	HTMLHelpParseObj *rv = XP_NEW(HTMLHelpParseObj);

	if(!rv)
		return(NULL);

	XP_ASSERT(url_to_map_file && id);

	if(!url_to_map_file || !id)
		return(NULL);

	XP_MEMSET(rv, 0, sizeof(HTMLHelpParseObj));

	rv->url_to_map_file = XP_STRDUP(url_to_map_file);
	rv->id              = XP_STRDUP(id);
	rv->helpVersion     = 1;

	rv->window_height = DEFAULT_HELP_WINDOW_HEIGHT;
	rv->window_width  = DEFAULT_HELP_WINDOW_WIDTH;

	rv->frame_group_stack = XP_ListNew();

	return(rv);
}

PRIVATE void
net_ParseHTMLHelpFree(HTMLHelpParseObj * obj)
{
	XP_ASSERT(obj);

	if(!obj)
		return;

	FREEIF(obj->window_name);
	FREEIF(obj->id_value);
	FREEIF(obj->default_id_value);
	FREEIF(obj->url_to_map_file);
	FREEIF(obj->line_buffer);
	FREEIF(obj->id);

	if(obj->frame_group_stack)
	  {
		frame_set_struct * frame_group_ptr;

		while((frame_group_ptr = XP_ListRemoveTopObject(obj->frame_group_stack)) != NULL)
			net_help_free_frame_group_struct(frame_group_ptr);

		FREE(obj->frame_group_stack);
	  }

	FREE(obj);
}

/* parse a line that looks like 
 * [whitespace]=[whitespace]"token" other stuff...
 *
 * return token or NULL.
 */
PRIVATE char *
net_get_html_help_token(char *line_data, char**next_word)
{
	char * line = line_data;
	char * cp;

	if(next_word)
		*next_word = NULL;
	
	while(XP_IS_SPACE(*line)) line++;
	
	if(*line != '=')
		return(NULL);

	line++; /* go past '=' */

	while(XP_IS_SPACE(*line)) line++;

	if(*line != '"')
		return(NULL);

	line++; /* go past '"' */

	for(cp=line; *cp; cp++)
	  	if(*cp == '"' && *(cp-1) != '\\')
		  {
			*cp = '\0';
			if(next_word)
			  {
				*next_word = cp+1;
				while(XP_IS_SPACE(*(*next_word))) (*next_word)++;
			  }
			break;
		  }

	return(line);
}

/* parse lines in an HTML help mapping file.
 * get window_size and name, etc...
 *
 * when the id is found function returns HTML_HELP_ID_FOUND
 * on error function returns negative error code.
 */
PRIVATE int
net_ParseHTMLHelpLine(HTMLHelpParseObj *obj, char *line_data)
{
	char *line = XP_StripLine(line_data);
	char *token;
	char *next_word;

	if(*line == '<')
	  {
		/* find and terminate the end '>' */
		XP_STRTOK(line, ">");

		token = XP_StripLine(line+1);

		if(!strncasecomp(token, 
						 ID_MAP_TOKEN, 
						 sizeof(ID_MAP_TOKEN)-1))
		  {
			obj->in_id_mapping = TRUE;
		  }
		else if(!strncasecomp(token, 
						 END_ID_MAP_TOKEN, 
						 sizeof(END_ID_MAP_TOKEN)-1))
		  {
			obj->in_id_mapping = FALSE;
		  }
		else if(!strncasecomp(token, 
						 FRAME_GROUP_TOKEN, 
						 sizeof(FRAME_GROUP_TOKEN)-1))
		  {
			char *cp = token + sizeof(FRAME_GROUP_TOKEN)-1;
			frame_set_struct * fgs = XP_NEW(frame_set_struct);

			while(isspace(*cp)) cp++;

			if(fgs)
			  {
				XP_MEMSET(fgs, 0, sizeof(frame_set_struct));

				next_word=NULL; /* init */

				do {
					if(!strncasecomp(cp, SRC_TOKEN, sizeof(SRC_TOKEN)-1))
				  	  {
						char *address = net_get_html_help_token(
														cp+sizeof(SRC_TOKEN)-1,
														&next_word);
						cp = next_word;
						fgs->address = XP_STRDUP(address);
				  	  }
					else if(!strncasecomp(cp, 
									  WINDOW_TOKEN, 
									  sizeof(WINDOW_TOKEN)-1))
				      {
					    char *window = net_get_html_help_token(
													cp+sizeof(WINDOW_TOKEN)-1, 
													&next_word);
					    cp = next_word;
					    fgs->target = XP_STRDUP(window);
				      }
					else
					  {
						/* unknown attribute.  Skip to next whitespace
						 */ 
						while(*cp && !isspace(*cp)) 
							cp++;


						if(*cp)
						  {
							while(isspace(*cp)) cp++;
							next_word = cp;
						  }
						else
						  {
							next_word = NULL;
						  }
					  }

				  } while(next_word);
			
				XP_ListAddObject(obj->frame_group_stack, fgs);
			  }
		  }
		else if(!strncasecomp(token, 
						 END_FRAME_GROUP_TOKEN, 
						 sizeof(END_FRAME_GROUP_TOKEN)-1))
		  {
			frame_set_struct *fgs;

			fgs = XP_ListRemoveTopObject(obj->frame_group_stack);

			if(fgs)
				net_help_free_frame_group_struct(fgs);
		  }
	  }
	else if(!obj->in_id_mapping)
	  {
		if(!strncasecomp(line, 
					 	WINDOW_SIZE_TOKEN, 
					 	sizeof(WINDOW_SIZE_TOKEN)-1))
		  {
			/* get window size */
			char *comma=0;
			char *window_size = net_get_html_help_token(line+
												sizeof(WINDOW_SIZE_TOKEN)-1, 
												NULL);

			if(window_size)
				comma = XP_STRCHR(window_size, ',');

			if(comma)
			  {
				*comma =  '\0';
				obj->window_width = XP_ATOI(window_size);
				obj->window_height = XP_ATOI(comma+1);
			  }
		  }
		else if(!strncasecomp(line, 
						 	WINDOW_NAME_TOKEN, 
						 	sizeof(WINDOW_NAME_TOKEN)-1))
		  {
			char *window_name = net_get_html_help_token(line+
												sizeof(WINDOW_NAME_TOKEN)-1,
												NULL);

			if(window_name)
			  {
				FREEIF(obj->window_name);
				obj->window_name = XP_STRDUP(window_name);
			  }
		  }
		else if(!strncasecomp(line, 
					 	HELP_VERSION_TOKEN, 
					 	sizeof(HELP_VERSION_TOKEN)-1))
		  {
			/* get window size */
			char *help_version = net_get_html_help_token(line+
												sizeof(HELP_VERSION_TOKEN)-1, 
												NULL);

			if(help_version)
			  {
				obj->helpVersion = XP_ATOI(help_version);
			  }
		  }
	  }
	else
	  {
		/* id mapping pair */
		if(!strncasecomp(line, obj->id, XP_STRLEN(obj->id)))
		  {
			char *id_value = net_get_html_help_token(line+XP_STRLEN(obj->id),
													 &next_word);

			if(id_value)
			  {
			  	obj->id_value = XP_STRDUP(id_value);

				while(next_word)
				  {
					char *cp = next_word;

                    if(!strncasecomp(cp,
                                     TARGET_TOKEN,
                                      sizeof(TARGET_TOKEN)-1))
                      {
                        char *target = net_get_html_help_token(
                                                    cp+sizeof(TARGET_TOKEN)-1,
                                                    &next_word);
                        cp = next_word;
                        obj->content_target = XP_STRDUP(target);
                      }
					else
					  {
                        /* unknown attribute.  Skip to next whitespace
                         */
                        while(*cp && !isspace(*cp))
                            cp++;

                        if(*cp)
                          {
                            while(isspace(*cp)) cp++;
                            next_word = cp;
                          }
                        else
                          {
                            next_word = NULL;
                          }
					  }
				  }
			  }
		
			return(HTML_HELP_ID_FOUND);
		  }
		if(!strncasecomp(line, DEFAULT_HELP_ID, sizeof(DEFAULT_HELP_ID)-1))
		  {
			char *default_id_value = net_get_html_help_token(
												line+sizeof(DEFAULT_HELP_ID)-1,
												NULL);

            if(default_id_value)
                obj->default_id_value = XP_STRDUP(default_id_value);
		  }
		
	  }

	return(0);
}

PRIVATE
int
NET_ParseHTMLHelpPut(HTMLHelpParseObj *obj, char *str, int32 len)
{
	int32 string_len;
	char *new_line;
	int32 status;
	
	if(!obj)
        return(MK_OUT_OF_MEMORY);

    /* buffer until we have a line */
    BlockAllocCat(obj->line_buffer, obj->line_buffer_size, str, len);
    obj->line_buffer_size += len;

    /* see if we have a line */
    while((new_line = strchr_in_buf(obj->line_buffer,
                                    obj->line_buffer_size,
                                    LF)) != NULL
		  || ((new_line = strchr_in_buf(obj->line_buffer,
                                    obj->line_buffer_size,
                                    CR)) != NULL) )
      {
        /* terminate the line */
        *new_line = '\0';

		status = net_ParseHTMLHelpLine(obj, obj->line_buffer);

        /* remove the parsed line from obj->line_buffer */
        string_len = (new_line - obj->line_buffer) + 1;
        XP_MEMCPY(obj->line_buffer,
                  new_line+1,
                  obj->line_buffer_size-string_len);
        obj->line_buffer_size -= string_len;

		if(status == HTML_HELP_ID_FOUND)
			return(HTML_HELP_ID_FOUND);
	  }

	return(0);
}

PRIVATE void
net_HelpPreExitRoutine(URL_Struct *URL_s, int status, MWContext *context)
{

	/* @@@@.  I wanted to use the fe_data to pass the URL struct
     * of the frame content URL.  But the fe_data gets cleared
     * by the front ends.  Therefore I'm using a private global
     * store the information.  This will work fine as long
	 * as two help requests don't come in simulatainiously.
	 */

	/* compatibility for previous versions of NetHelp */
	if(frame_content_for_pre_exit_routine) {

		NET_GetURL(frame_content_for_pre_exit_routine,
				   FO_CACHE_AND_PRESENT,
				   context,
				   simple_exit);
		frame_content_for_pre_exit_routine=NULL;
	} else {
		LM_SendOnHelp(context);
	}


}

PRIVATE void 
net_help_init_chrome(Chrome *window_chrome, int32 w, int32 h)
{
    window_chrome->type           = MWContextHTMLHelp;
  #if defined(XP_WIN) || defined(XP_UNIX)
    window_chrome->topmost        = FALSE;
  #else
    window_chrome->topmost        = TRUE;  
  #endif
    window_chrome->w_hint         = w;
    window_chrome->h_hint         = h;
    window_chrome->allow_resize   = TRUE;
    window_chrome->show_scrollbar = TRUE;
    window_chrome->allow_close    = TRUE;
	window_chrome->disable_commands = TRUE;
	window_chrome->restricted_target = TRUE;
}

PRIVATE void
net_ParseHTMLHelpLoadHelpDoc(HTMLHelpParseObj *obj, MWContext *context)
{
	URL_Struct *URL_s;
	char *frame_address = NULL;
	char *content_address = NULL;
	MWContext *new_context;
	frame_set_struct *fgs;

	if(obj->id_value || obj->default_id_value)
		content_address = NET_MakeAbsoluteURL(obj->url_to_map_file, 
											  obj->id_value ? 
												obj->id_value : 
												obj->default_id_value);

	if(!content_address)
	  {
		FE_Alert(context, XP_GetString(MK_CANT_LOAD_HELP_TOPIC));
		return;
	  }

	fgs = XP_ListPeekTopObject(obj->frame_group_stack);

	if(fgs)
	  {
		if(fgs->address)
		  {
			frame_address = NET_MakeAbsoluteURL(obj->url_to_map_file, 
												fgs->address);
		  }
	  }

	if(frame_address)
		URL_s = NET_CreateURLStruct(frame_address, NET_DONT_RELOAD);
	else
		URL_s = NET_CreateURLStruct(content_address, NET_DONT_RELOAD);

	if(!URL_s)
		goto cleanup;

	URL_s->window_chrome = XP_NEW(Chrome);	

	if(!URL_s->window_chrome)
		goto cleanup;

	XP_MEMSET(URL_s->window_chrome, 0, sizeof(Chrome));

	if(obj->window_name)
		URL_s->window_target = XP_STRDUP(obj->window_name);
	else
		URL_s->window_target = XP_STRDUP(DEFAULT_HELP_WINDOW_NAME);

	net_help_init_chrome(URL_s->window_chrome, 
						 obj->window_width, 
						 obj->window_height);

	/* We want to revert the character set of the help frame from the standard
	   character set, not whatever happened to be the last viewed source */

	StrAllocCopy(URL_s->charset, INTL_ResourceCharSet());
	
	new_context = XP_FindNamedContextInList(NULL, URL_s->window_target);

	if(frame_address)
	  {
		URL_Struct *content_URL_s;

		/* if there is a frame_address then we load the
		 * frame first and then load the contents
		 * in the frame exit function.
		 */
		content_URL_s = NET_CreateURLStruct(content_address, NET_DONT_RELOAD);

		if(obj->content_target)
			content_URL_s->window_target = XP_STRDUP(obj->content_target);
		else if(fgs->target)
			content_URL_s->window_target = XP_STRDUP(fgs->target);

		/* doesn't work: URL_s->fe_data = (void *) content_URL_s; */

		/* hack for older versions, see pre_exit_routine_above */
		if (obj->helpVersion < 2) {
			frame_content_for_pre_exit_routine = content_URL_s;
		} else {
			frame_content_for_pre_exit_routine = NULL;
			NET_FreeURLStruct(content_URL_s);
		}

		URL_s->pre_exit_fn = net_HelpPreExitRoutine;
	  }

	if(!new_context)
	  {
	  
		/* this will cause the load too */
		new_context = FE_MakeNewWindow(context, 
						 URL_s, 
						 (obj->window_name) ? obj->window_name :  DEFAULT_HELP_WINDOW_NAME, 
						 URL_s->window_chrome);

		if (HELP_INFO_PTR(*new_context) == NULL) {
			new_context->pHelpInfo = XP_NEW_ZAP(HelpInfoStruct);
		}
		
		if (HELP_INFO_PTR(*new_context)->topicURL != NULL) {
			XP_FREE(HELP_INFO_PTR(*new_context)->topicURL);
			HELP_INFO_PTR(*new_context)->topicURL = NULL;
		}
		
		StrAllocCopy(HELP_INFO_PTR(*new_context)->topicURL, content_address);

	  }
	else
	  {
	
		if (HELP_INFO_PTR(*new_context) == NULL) {
			new_context->pHelpInfo = XP_NEW_ZAP(HelpInfoStruct);
		}
		
		if (HELP_INFO_PTR(*new_context)->topicURL != NULL) {
			XP_FREE(HELP_INFO_PTR(*new_context)->topicURL);
			HELP_INFO_PTR(*new_context)->topicURL = NULL;
		}
		
		StrAllocCopy(HELP_INFO_PTR(*new_context)->topicURL, content_address);

		FE_RaiseWindow(new_context);

		/* Compatibility with earlier versions of NetHelp */
		if (obj->helpVersion < 2) {
			FE_GetURL(new_context, URL_s);
		} else {
			LM_SendOnHelp(new_context);
		}
	  }

cleanup:
	FREEIF(frame_address);
	FREE(content_address);

	return;
}

typedef struct {
	HTMLHelpParseObj *parse_obj;
	MWContext        *context;
	XP_Bool           file_is_local;
} html_help_map_stream;

PRIVATE int
net_HMFConvPut(NET_StreamClass *stream, char *s, int32 l)
{
	html_help_map_stream *obj=stream->data_object;
	int status = NET_ParseHTMLHelpPut(obj->parse_obj, s, l);		

	if(obj->file_is_local && status == HTML_HELP_ID_FOUND)
	  {
		/* abort since we don't need any more of the file */
		return(MK_UNABLE_TO_CONVERT);
	  }

	return(status);
}

PRIVATE int
net_HMFConvWriteReady(NET_StreamClass *stream)
{	
	return(MAX_WRITE_READY);
}

PRIVATE void
net_HMFConvComplete(NET_StreamClass *stream)
{
	html_help_map_stream *obj=stream->data_object;	
	net_ParseHTMLHelpLoadHelpDoc(obj->parse_obj, obj->context);
	net_ParseHTMLHelpFree(obj->parse_obj);
}

PRIVATE void
net_HMFConvAbort(NET_StreamClass *stream, int status)
{
	html_help_map_stream*obj=stream->data_object;	
	if(status == MK_UNABLE_TO_CONVERT)
		net_ParseHTMLHelpLoadHelpDoc(obj->parse_obj, obj->context);

	net_ParseHTMLHelpFree(obj->parse_obj);
}

PUBLIC NET_StreamClass *
NET_HTMLHelpMapToURL(int         format_out,
                     void       *data_object,
                     URL_Struct *URL_s,
                     MWContext  *window_id)
{
    html_help_map_stream* obj;
    NET_StreamClass* stream;

    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    stream = XP_NEW(NET_StreamClass);
    if(stream == NULL)
        return(NULL);

    obj = XP_NEW(html_help_map_stream);
    if (obj == NULL)
      {
        FREE(stream);
        return(NULL);
      }

    XP_MEMSET(obj, 0, sizeof(html_help_map_stream));

	if(URL_s->cache_file || URL_s->memory_copy)
		obj->file_is_local = TRUE;
	else
		obj->file_is_local = NET_IsLocalFileURL(URL_s->address);
	
    obj->parse_obj = net_ParseHTMLHelpInit(URL_s->address, URL_s->fe_data);

    if(!obj->parse_obj)
      {
        FREE(stream);
        FREE(obj);
      }

    obj->context = window_id;
    stream->name           = "HTML Help Map File converter";
    stream->complete       = (MKStreamCompleteFunc) net_HMFConvComplete;
    stream->abort          = (MKStreamAbortFunc) net_HMFConvAbort;
    stream->put_block      = (MKStreamWriteFunc) net_HMFConvPut;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_HMFConvWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

    return(stream);
}


