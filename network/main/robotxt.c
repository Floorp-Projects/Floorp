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
/*** robotxt.c ****************************************************/
/*   description:		implementation of robots.txt parser       */
  

 /********************************************************************

  $Revision: 1.3 $
  $Date: 1998/05/22 23:38:14 $

 *********************************************************************/

#include "xp.h"
#include "xp_str.h"
#include "ntypes.h" /* for MWContext */
#include "net.h"
#include "robotxt.h"
#include "prmem.h"
#include "prthread.h"
#include "prinrval.h"
#include "prio.h" /* for testing */

#define USER_AGENT "User-agent"
#define DISALLOW "Disallow"
#define ALLOW "Allow"
#define ASTERISK "*"
#define MOZILLA "mozilla"

typedef uint8 CRAWL_RobotControlAvailability;

#define ROBOT_CONTROL_AVAILABLE			((CRAWL_RobotControlAvailability)0x00)
#define ROBOT_CONTROL_NOT_AVAILABLE		((CRAWL_RobotControlAvailability)0x01)
#define ROBOT_CONTROL_NOT_YET_QUERIED	((CRAWL_RobotControlAvailability)0x02)

#define PARSE_STATE_ALLOW 1
#define	PARSE_STATE_DISALLOW 2
#define	PARSE_STATE_AGENT 3

#define PARSE_NO_ERR 0
#define PARSE_ERR 1
#define PARSE_NO_MEMORY 2
#define MOZILLA_RECORD_READ 3 /* found the Mozilla record so we're done */

extern int crawl_appendString(char **str, uint16 *len, uint16 *size, char c);

typedef struct _CRAWL_RobotControlStruct {
	/* char *host; */
	char *siteURL;
	CRAWL_RobotControlAvailability status;
	char **line;
	uint16 numLines;
	uint16 sizeLines;
	PRBool *allowed;
	MWContext *context;
	CRAWL_RobotControlStatusFunc completion_func;
	void *owner_data;
	PRBool freeData;
	/* char *requested_url; */
} CRAWL_RobotControlStruct;

typedef struct _CRAWL_RobotParseStruct {
	uint8 state;
	char *token;
	uint16 lenToken;
	uint16 sizeToken;
	PRBool inComment;
	PRBool isProcessing;
	PRBool skipWhitespace;
	PRBool mozillaSeen; /* true if we saw a mozilla user agent */
	PRBool defaultSeen; /* true if we saw a default user agent */
	PRBool foundRecord; /* true if we read a mozilla or default record */
} CRAWL_RobotParseStruct;

typedef CRAWL_RobotParseStruct *CRAWL_RobotParse;

/* prototypes */
static int crawl_unescape (char *str, char *reserved, int numReserved);
PRBool crawl_startsWith (char *pattern, char *uuid);
PRBool crawl_endsWith (char *pattern, char *uuid);
void crawl_stringToLower(char *str);
static void crawl_destroyLines(CRAWL_RobotControl control);
static void crawl_addRobotControlDirective(CRAWL_RobotControl control, char *token, PRBool isAllowed);
static int crawl_parseRobotControlInfo(CRAWL_RobotControl control, CRAWL_RobotParse parse, char *str, uint32 len);
static CRAWL_RobotControlStatus crawl_isRobotAllowed(CRAWL_RobotControl control, char *url);

/* this stuff is adapted from mkparse.c */
#define HEX_ESCAPE '%'
#define RESERVED_CHARS ";/:@=&"
#define NUM_RESERVED 6

/* decode % escaped hex codes into character values
 */
#define UNHEX(C) \
    ((C >= '0' && C <= '9') ? C - '0' : \
     ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
     ((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

/* unescapes a string, but leaves octets encoded if they match one of the supplied reserved characters.
   this was adapted from NET_UnescapeCnt */
static int
crawl_unescape (char *str, char *reserved, int numReserved)
{
	int i;
    register char *src = str;
    register char *dst = str;

    while(*src)
        if (*src != HEX_ESCAPE)
		  {
        	*dst++ = *src++;
		  }
        else 	
		  {
        	src++; /* walk over escape */
        	if (*src)
              {
            	*dst = UNHEX(*src) << 4;
            	src++;
              }
        	if (*src)
              {
            	*dst = (*dst + UNHEX(*src));
            	src++;
              }
			/* check if it belongs to the reserved characters */
			for (i = 0; i < numReserved; i++) {
				if (*dst == reserved[i]) {
					/* put it back */
					*dst++ = HEX_ESCAPE;
					*dst++ = *(src-2);
					*dst = *(src-1);
				}
			}
        	dst++;
          }

    *dst = 0;

    return (int)(dst - str);
}

#define CHAR_CMP(x, y) ((x == y) || (NET_TO_LOWER(x) == NET_TO_LOWER(y)))

PRBool crawl_startsWith (char *pattern, char *uuid) {
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  short index;
  
  if (l2 < l1) return PR_FALSE;
  
  for (index = 0; index < l1; index++) {
    if (!(CHAR_CMP(pattern[index], uuid[index]))) return PR_FALSE;
  }
	
  return PR_TRUE;
}

PRBool crawl_endsWith (char *pattern, char *uuid) {
  short l1 = strlen(pattern);
  short l2 = strlen(uuid);
  short index;
  
  if (l2 < l1) return PR_FALSE;
  
  for (index = 0; index < l1; index++) {
    if (!(CHAR_CMP(pattern[l1-index], uuid[l2-index]))) return PR_FALSE;
  }
  
  return PR_TRUE;
}

void crawl_stringToLower(char *str) {
    register char *src = str;
    register char *dst = str;
    while(*src) {
        *dst++ = tolower(*src++);
	}
	*dst = 0;
}

PR_IMPLEMENT(CRAWL_RobotControl) CRAWL_MakeRobotControl(MWContext *context, char *siteURL) {
	CRAWL_RobotControl control = PR_NEWZAP(CRAWL_RobotControlStruct);
	if (control == NULL) return(NULL);
	control->siteURL = PL_strdup(siteURL);
	if (siteURL == NULL) return(NULL);
	control->status = ROBOT_CONTROL_NOT_YET_QUERIED;
	control->context = context;
	return control;
}

static void 
crawl_destroyLines(CRAWL_RobotControl control) {
	uint16 i;
	for (i = 0; i < control->numLines; i++) {
		PR_Free(control->line[i]);
	}
	if (control->line != NULL) PR_Free(control->line);
	if (control->allowed != NULL) PR_Free(control->allowed);
	control->allowed = NULL;
	control->line = NULL;
	control->numLines = control->sizeLines = 0;
}

PR_IMPLEMENT(void) CRAWL_DestroyRobotControl(CRAWL_RobotControl control) {
	if (control->siteURL != NULL) PR_Free(control->siteURL);
	crawl_destroyLines(control);
	PR_Free(control);
}

static void 
crawl_addRobotControlDirective(CRAWL_RobotControl control, char *token, PRBool isAllowed) {
	/* convert token to lower case and unescape it */
	crawl_stringToLower(token);
	crawl_unescape(token, RESERVED_CHARS, NUM_RESERVED);
	if (control->numLines == control->sizeLines) {
		char **newLines;
		char **old;
		PRBool *newAllowed;
		PRBool *oldAllowed;
		/* copy the paths array */
		newLines = (char**)PR_MALLOC(sizeof(char**) * (control->sizeLines + 10));
		if (newLines == NULL) return;
		old = control->line;
		memcpy((char*)newLines, (char*)control->line, (sizeof(char**) * control->numLines));
		control->line = newLines;
		if (old != NULL) PR_Free(old);
		/* copy the boolean array */
		newAllowed = (PRBool*)PR_MALLOC(sizeof(PRBool) * (control->sizeLines + 10));
		if (newAllowed == NULL) return;
		oldAllowed = control->allowed;
		memcpy((char*)newAllowed, (char*)control->allowed, (sizeof(PRBool) * control->numLines));
		control->allowed = newAllowed;
		if (oldAllowed != NULL) PR_Free(oldAllowed);
		control->sizeLines += 10;
	}
	*(control->line + control->numLines) = token;
	*(control->allowed + control->numLines) = isAllowed;
	control->numLines++;
}

static int 
crawl_parseRobotControlInfo(CRAWL_RobotControl control, CRAWL_RobotParse parse, char *str, uint32 len) { 
	uint32 n = 0; /* where we are in the buffer */
	char c;
	while (n < len) {
		c = *(str + n);
		if (parse->skipWhitespace) {
			if ((c == ' ') || (c == '\n') || (c == '\r') || (c == '\t')) {
				n++;
			} else parse->skipWhitespace = PR_FALSE;
		} else {
			if (c == '#') {
				parse->inComment = PR_TRUE;
				n++;
			} else if (parse->inComment) {
				if ((c == '\n') || (c == '\r')) {
					parse->inComment = PR_FALSE;
					parse->skipWhitespace = PR_TRUE;
					n++;
				} else n++; /* skip all other characters */
			} else if (c == ':') { /* directive */
				PRBool mozillaRecordRead = PR_FALSE;
				if (crawl_appendString(&parse->token, &parse->lenToken, &parse->sizeToken, '\0') != 0) /* null terminate */
					return PARSE_NO_MEMORY;
				if (PL_strcasecmp(parse->token, USER_AGENT) == 0) {
					if ((parse->state == PARSE_STATE_DISALLOW) || (parse->state == PARSE_STATE_ALLOW)) {
						/* already read a disallow or allow directive so the previous record is done */
						if (parse->isProcessing) {
							if (parse->mozillaSeen) mozillaRecordRead = PR_TRUE;
							if (parse->mozillaSeen || parse->defaultSeen) parse->foundRecord = PR_TRUE;
							parse->isProcessing = PR_FALSE;
						}
					}
					parse->state = PARSE_STATE_AGENT;
				} else if (PL_strcasecmp(parse->token, DISALLOW) == 0) {
					parse->state = PARSE_STATE_DISALLOW;
				} else if (PL_strcasecmp(parse->token, ALLOW) == 0)
					parse->state = PARSE_STATE_ALLOW;
				/* else it is an unknown directive */
				PR_Free(parse->token);
				parse->token = NULL;
				parse->lenToken = parse->sizeToken = 0;
				parse->skipWhitespace = PR_TRUE;
				n++;
				if (mozillaRecordRead) return MOZILLA_RECORD_READ; /* read the mozilla record so we're outta here */
			} else if ((c == '\n') || (c == '\r')) {
				if (crawl_appendString(&parse->token, &parse->lenToken, &parse->sizeToken, '\0') != 0) /* null terminate */
					return PARSE_NO_MEMORY;
				switch (parse->state) {
				case PARSE_STATE_AGENT:
					if (PL_strcasestr(parse->token, MOZILLA) != NULL) {
						parse->mozillaSeen = PR_TRUE;
						crawl_destroyLines(control); /* destroy previous default data */
						parse->isProcessing = PR_TRUE; /* start processing */
					} else if ((PL_strcmp(parse->token, ASTERISK) == 0) && (!parse->mozillaSeen)) {
						parse->defaultSeen = PR_TRUE;
						parse->isProcessing = PR_TRUE; /* start processing */
					}
					PR_Free(parse->token);
					break;
				case PARSE_STATE_DISALLOW:
					/* if processing, add to disallowed */
					if (parse->isProcessing) {
						crawl_addRobotControlDirective(control, parse->token, PR_FALSE);
					}
					break;
				case PARSE_STATE_ALLOW:
					/* if processing, add to allowed */
					if (parse->isProcessing) {
						crawl_addRobotControlDirective(control, parse->token, PR_TRUE);
					}
					break;
				default:
					PR_Free(parse->token);
					break;
				}
				parse->token = NULL;
				parse->lenToken = parse->sizeToken = 0;
				parse->skipWhitespace = PR_TRUE;
			} else {
				if (crawl_appendString(&parse->token, &parse->lenToken, &parse->sizeToken, c) != 0)
					return PARSE_NO_MEMORY;
				n++;
			}
		}
	}
	return PARSE_NO_ERR;
}

static CRAWL_RobotControlStatus 
crawl_isRobotAllowed(CRAWL_RobotControl control, char *url) {
	/* extract file component (after host) from url and decode it */
	uint16 i;
	char *file = NET_ParseURL(url, GET_PATH_PART);
	if (file == NULL) return CRAWL_ROBOT_ALLOWED;
	crawl_unescape(file, RESERVED_CHARS, NUM_RESERVED);
	
	for (i = 0; i < control->numLines; i++) {
		if (crawl_startsWith(control->line[i], file))
			return (control->allowed[i] ? CRAWL_ROBOT_ALLOWED : CRAWL_ROBOT_DISALLOWED);
	}
	PR_Free(file);
	return CRAWL_ROBOT_ALLOWED; /* no matches */
}

static void
crawl_get_robots_txt_exit(URL_Struct *URL_s, int status, MWContext *window_id)
{
#if defined(XP_MAC)
#pragma unused(window_id)
#endif	
	CRAWL_RobotControl control = (CRAWL_RobotControl)URL_s->owner_data;
	if (status < 0) {
		control->status = ROBOT_CONTROL_NOT_AVAILABLE;
		if (control->owner_data != NULL) {
			(control->completion_func)(control->owner_data);
			if (control->freeData) PR_DELETE(control->owner_data);
		}
	}
	if(status != MK_CHANGING_CONTEXT)
		NET_FreeURLStruct(URL_s);
}

/* issues a request for the robots.txt file.
   returns PR_TRUE if the request was issued succesfully, PR_FALSE if not.
*/
PR_IMPLEMENT(PRBool) CRAWL_ReadRobotControlFile(CRAWL_RobotControl control, CRAWL_RobotControlStatusFunc func, void *data, PRBool freeData) {
	/* create new cache request for site + /robots.txt" */
	char *url = NET_MakeAbsoluteURL(control->siteURL, "/robots.txt");
	if (url != NULL) {
		URL_Struct *url_s = NET_CreateURLStruct(url, NET_NORMAL_RELOAD);
		if (url_s != NULL) {
			control->completion_func = func;
			control->owner_data = data;
			control->freeData = freeData;
			url_s->owner_data = control;
			NET_GetURL(url_s, FO_CACHE_AND_ROBOTS_TXT, control->context, crawl_get_robots_txt_exit);
			/* func(data); */
			return PR_TRUE;
		}
	} 
	control->status = ROBOT_CONTROL_NOT_AVAILABLE;
	return PR_FALSE;
}

PR_IMPLEMENT(CRAWL_RobotControlStatus) CRAWL_GetRobotControl(CRAWL_RobotControl control, char *url) {
	/* return ROBOT_ALLOWED; */
	switch (control->status) {
	case ROBOT_CONTROL_NOT_YET_QUERIED:
		return CRAWL_ROBOTS_TXT_NOT_QUERIED;
	case ROBOT_CONTROL_AVAILABLE:
		return crawl_isRobotAllowed(control, url);
		break;
	case ROBOT_CONTROL_NOT_AVAILABLE:
		return CRAWL_ROBOT_ALLOWED; /* no robots.txt file found so assume we can crawl */
		break;
	default:
		return CRAWL_ROBOT_ALLOWED;
		break;
	}
}

/* content type conversion */
typedef struct {
	CRAWL_RobotParse parse_obj;
	CRAWL_RobotControl control;
} crawl_robots_txt_stream;

PRIVATE int
crawl_RobotsTxtConvPut(NET_StreamClass *stream, char *s, int32 l)
{
	crawl_robots_txt_stream *obj=stream->data_object;
	int status = crawl_parseRobotControlInfo(obj->control, obj->parse_obj, s, l);		

	if ((status == MOZILLA_RECORD_READ) || 
		(status == PARSE_NO_MEMORY)) {
		return (MK_UNABLE_TO_CONVERT); /* abort since we read the mozilla record, no need to read any others */
	}

	return(status);
}

PRIVATE int
crawl_RobotsTxtConvWriteReady(NET_StreamClass *stream)
{	
#if defined(XP_MAC)
#pragma unused(stream)
#endif
	return(MAX_WRITE_READY);
}

PRIVATE void
crawl_RobotsTxtConvComplete(NET_StreamClass *stream)
{
	crwal_robots_txt_stream*obj=stream->data_object;	
	if (obj->parse_obj->foundRecord) obj->control->status = ROBOT_CONTROL_AVAILABLE; 
	if (obj->control->owner_data != NULL) {
		(obj->control->completion_func)(obj->control->owner_data);
		if (obj->control->freeData) PR_DELETE(obj->control->owner_data);
	}
	PR_Free(obj->parse_obj);
}

PRIVATE void
crawl_RobotsTxtConvAbort(NET_StreamClass *stream, int status)
{
	crawl_robots_txt_stream *obj=stream->data_object;	
	if(status == MK_UNABLE_TO_CONVERT) { /* special case, we read the mozilla record and exited early */
		obj->control->status = ROBOT_CONTROL_AVAILABLE;
	} else obj->control->status = ROBOT_CONTROL_NOT_AVAILABLE; 
	if (obj->control->owner_data != NULL) {
		(obj->control->completion_func)(obj->control->owner_data);
		if (obj->control->freeData) PR_DELETE(obj->control->owner_data);
	}
	PR_Free(obj->parse_obj);
}

PUBLIC NET_StreamClass *
CRAWL_RobotsTxtConverter(int format_out,
						void *data_object,
						URL_Struct *URL_s,
						MWContext  *window_id)
{
#if defined(XP_MAC)
#pragma unused(format_out, data_object)
#endif
    crawl_robots_txt_stream *obj;
    NET_StreamClass *stream;
	CRAWL_RobotControl control = (CRAWL_RobotControl)URL_s->owner_data;

    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

	if (URL_s->server_status < 400) {
		stream = PR_NEW(NET_StreamClass);
		if(stream == NULL) {
			control->status = ROBOT_CONTROL_NOT_AVAILABLE;
			return(NULL);
		}

		obj = PR_NEW(crawl_robots_txt_stream);
		if (obj == NULL)
		  {
			PR_Free(stream);
			control->status = ROBOT_CONTROL_NOT_AVAILABLE;
			return(NULL);
		  }
		obj->parse_obj = PR_NEWZAP(CRAWL_RobotParseStruct);
		if (obj->parse_obj == NULL) return(NULL);
		obj->control = URL_s->owner_data;

		stream->name           = "robots.txt Converter";
		stream->complete       = (MKStreamCompleteFunc) crawl_RobotsTxtConvComplete;
		stream->abort          = (MKStreamAbortFunc) crawl_RobotsTxtConvAbort;
		stream->put_block      = (MKStreamWriteFunc) crawl_RobotsTxtConvPut;
		stream->is_write_ready = (MKStreamWriteReadyFunc) crawl_RobotsTxtConvWriteReady;
		stream->data_object    = obj;  /* document info object */
		stream->window_id      = window_id;

		return(stream);
	} else {
		control->status = ROBOT_CONTROL_NOT_AVAILABLE;
		if (control->owner_data != NULL) {
			control->completion_func(control->owner_data);
			if (control->freeData) PR_DELETE(control->owner_data);
		}
		return (NULL);
	}
}

#if DEBUG_TEST_ROBOT
void testRobotControlParser(char *url) {
	/* this will be done through libnet but for now use PR_OpenFile */
	PRFileDesc *fp;
	int32 len;
	char *path;
	static char buf[512]; /* xxx alloc */
	CRAWL_RobotParse parse;
	CRAWL_RobotControl control = MakeRobotControl("foo");
	/* XXX need to unescape URL */
	path=&(url[8]);
	fp = PR_Open(path,  PR_RDONLY, 0644);
	if(fp == NULL)
	{
		return;
	}
	parse = PR_NEWZAP(CRAWL_RobotParseStruct);
	while((len=PR_Read(fp, buf, 512))>0) {
		if (crawl_parseRobotControlInfo(control, parse, buf, len) == MOZILLA_RECORD_READ) break;
	}
	PR_Close(fp);
	PR_Free(parse);
	DestroyRobotControl(control);
	return;
}
#endif
