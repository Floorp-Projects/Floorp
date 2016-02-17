/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"
#include <prmem.h>
#include <prio.h>
#include <prenv.h>

static int	javascript_fn(char *relpath, char *basedir, char *reldir,
char *filename, void *arg);
static int	extract_js (char *filename);
static int	copyinto (char *from, char *to);
static PRStatus ensureExists (char *base, char *path);
static int	make_dirs(char *path, PRInt32 file_perms);

static char	*jartree = NULL;
static int	idOrdinal;
static PRBool dumpParse = PR_FALSE;

static char	*event_handlers[] = {
    "onAbort",
    "onBlur",
    "onChange",
    "onClick",
    "onDblClick",
    "onDragDrop",
    "onError",
    "onFocus",
    "onKeyDown",
    "onKeyPress",
    "onKeyUp",
    "onLoad",
    "onMouseDown",
    "onMouseMove",
    "onMouseOut",
    "onMouseOver",
    "onMouseUp",
    "onMove",
    "onReset",
    "onResize",
    "onSelect",
    "onSubmit",
    "onUnload"
};


static int	num_handlers = 23;

/*
 *  I n l i n e J a v a S c r i p t
 *
 *  Javascript signing. Instead of passing an archive to signtool,
 *  a directory containing html files is given. Archives are created
 *  from the archive= and src= tag attributes inside the html,
 *  as appropriate. Then the archives are signed.
 *
 */
int
InlineJavaScript(char *dir, PRBool recurse)
{
    jartree = dir;
    if (verbosity >= 0) {
	PR_fprintf(outputFD, "\nGenerating inline signatures from HTML files in: %s\n",
	     dir);
    }
    if (PR_GetEnv("SIGNTOOL_DUMP_PARSE")) {
	dumpParse = PR_TRUE;
    }

    return foreach(dir, "", javascript_fn, recurse, PR_FALSE /*include dirs*/,
         		(void * )NULL);

}


/************************************************************************
 *
 * j a v a s c r i p t _ f n
 */
static int	javascript_fn
(char *relpath, char *basedir, char *reldir, char *filename, void *arg)
{
    char	fullname [FNSIZE];

    /* only process inline scripts from .htm, .html, and .shtml*/

    if (!(PL_strcaserstr(filename, ".htm") == filename + strlen(filename) -
        4) && 
        !(PL_strcaserstr(filename, ".html") == filename + strlen(filename) -
        5) && 
        !(PL_strcaserstr(filename, ".shtml") == filename + strlen(filename)
        -6)) {
	return 0;
    }

    /* don't process scripts that signtool has already
     extracted (those that are inside .arc directories) */

    if (PL_strcaserstr(filename, ".arc") == filename + strlen(filename) - 4)
	return 0;

    if (verbosity >= 0) {
	PR_fprintf(outputFD, "Processing HTML file: %s\n", relpath);
    }

    /* reset firstArchive at top of each HTML file */

    /* skip directories that contain extracted scripts */

    if (PL_strcaserstr(reldir, ".arc") == reldir + strlen(reldir) - 4)
	return 0;

    sprintf (fullname, "%s/%s", basedir, relpath);
    return extract_js (fullname);
}


/*===========================================================================
 =
 = D A T A   S T R U C T U R E S
 =
*/
typedef enum {
    TEXT_HTML_STATE = 0,
    SCRIPT_HTML_STATE
} 


HTML_STATE ;

typedef enum {
    /* we start in the start state */
    START_STATE,

    /* We are looking for or reading in an attribute */
    GET_ATT_STATE,

    /* We're burning ws before finding an attribute */
    PRE_ATT_WS_STATE,

    /* We're burning ws after an attribute.  Looking for an '='. */
    POST_ATT_WS_STATE,

    /* We're burning ws after an '=', waiting for a value */
    PRE_VAL_WS_STATE,

    /* We're reading in a value */
    GET_VALUE_STATE,

    /* We're reading in a value that's inside quotes */
    GET_QUOTED_VAL_STATE,

    /* We've encountered the closing '>' */
    DONE_STATE,

    /* Error state */
    ERR_STATE
} 


TAG_STATE ;

typedef struct AVPair_Str {
    char	*attribute;
    char	*value;
    unsigned int	valueLine; /* the line that the value ends on */
    struct AVPair_Str *next;
} AVPair;

typedef enum {
    APPLET_TAG,
    SCRIPT_TAG,
    LINK_TAG,
    STYLE_TAG,
    COMMENT_TAG,
    OTHER_TAG
} 


TAG_TYPE ;

typedef struct {
    TAG_TYPE type;
    AVPair * attList;
    AVPair * attListTail;
    char	*text;
} TagItem;

typedef enum {
    TAG_ITEM,
    TEXT_ITEM
} 


ITEM_TYPE ;

typedef struct HTMLItem_Str {
    unsigned int	startLine;
    unsigned int	endLine;
    ITEM_TYPE type;
    union {
	TagItem *tag;
	char	*text;
    } item;
    struct HTMLItem_Str *next;
} HTMLItem;

typedef struct {
    PRFileDesc *fd;
    PRInt32 curIndex;
    PRBool IsEOF;
#define FILE_BUFFER_BUFSIZE 512
    char	buf[FILE_BUFFER_BUFSIZE];
    PRInt32 startOffset;
    PRInt32 maxIndex;
    unsigned int	lineNum;
} FileBuffer;

/*===========================================================================
 =
 = F U N C T I O N S
 =
*/
static HTMLItem*CreateTextItem(char *text, unsigned int startline,
unsigned int endline);
static HTMLItem*CreateTagItem(TagItem*ti, unsigned int startline,
unsigned int endline);
static TagItem*ProcessTag(FileBuffer*fb, char **errStr);
static void	DestroyHTMLItem(HTMLItem *item);
static void	DestroyTagItem(TagItem*ti);
static TAG_TYPE GetTagType(char *att);
static FileBuffer*FB_Create(PRFileDesc*fd);
static int	FB_GetChar(FileBuffer *fb);
static PRInt32 FB_GetPointer(FileBuffer *fb);
static PRInt32 FB_GetRange(FileBuffer *fb, PRInt32 start, PRInt32 end,
char **buf);
static unsigned int	FB_GetLineNum(FileBuffer *fb);
static void	FB_Destroy(FileBuffer *fb);
static void	PrintTagItem(PRFileDesc *fd, TagItem *ti);
static void	PrintHTMLStream(PRFileDesc *fd, HTMLItem *head);

/************************************************************************
 *
 * C r e a t e T e x t I t e m
 */
static HTMLItem*
CreateTextItem(char *text, unsigned int startline, unsigned int endline)
{
    HTMLItem * item;

    item = PR_Malloc(sizeof(HTMLItem));
    if (!item) {
	return NULL;
    }

    item->type = TEXT_ITEM;
    item->item.text = text;
    item->next = NULL;
    item->startLine = startline;
    item->endLine = endline;

    return item;
}


/************************************************************************
 *
 * C r e a t e T a g I t e m
 */
static HTMLItem*
CreateTagItem(TagItem*ti, unsigned int startline, unsigned int endline)
{
    HTMLItem * item;

    item = PR_Malloc(sizeof(HTMLItem));
    if (!item) {
	return NULL;
    }

    item->type = TAG_ITEM;
    item->item.tag = ti;
    item->next = NULL;
    item->startLine = startline;
    item->endLine = endline;

    return item;
}


static PRBool
isAttChar(int c)
{
    return (isalnum(c) || c == '/' || c == '-');
}


/************************************************************************
 *
 * P r o c e s s T a g
 */
static TagItem*
ProcessTag(FileBuffer*fb, char **errStr)
{
    TAG_STATE state;
    PRInt32 startText, startID, curPos;
    PRBool firstAtt;
    int	curchar;
    TagItem * ti = NULL;
    AVPair * curPair = NULL;
    char	quotechar = '\0';
    unsigned int	linenum;
    unsigned int	startline;

    state = START_STATE;

    startID = FB_GetPointer(fb);
    startText = startID;
    firstAtt = PR_TRUE;

    ti = (TagItem * ) PR_Malloc(sizeof(TagItem));
    if (!ti) 
	out_of_memory();
    ti->type = OTHER_TAG;
    ti->attList = NULL;
    ti->attListTail = NULL;
    ti->text = NULL;

    startline = FB_GetLineNum(fb);

    while (state != DONE_STATE && state != ERR_STATE) {
	linenum = FB_GetLineNum(fb);
	curchar = FB_GetChar(fb);
	if (curchar == EOF) {
	    *errStr = PR_smprintf(
	        "line %d: Unexpected end-of-file while parsing tag starting at line %d.\n",
	         linenum, startline);
	    state = ERR_STATE;
	    continue;
	}

	switch (state) {
	case START_STATE:
	    if (curchar == '!') {
		/*
		 * SGML tag or comment
		 * Here's the general rule for SGML tags.  Everything from
		 * <! to > is the tag.  Inside the tag, comments are
		 * delimited with --.  So we are looking for the first '>'
		 * that is not commented out, that is, not inside a pair
		 * of --: <!DOCTYPE --this is a comment >(psyche!)   -->
		 */

		PRBool inComment = PR_FALSE;
		short	hyphenCount = 0; /* number of consecutive hyphens */

		while (1) {
		    linenum = FB_GetLineNum(fb);
		    curchar = FB_GetChar(fb);
		    if (curchar == EOF) {
			/* Uh oh, EOF inside comment */
			*errStr = PR_smprintf(
    "line %d: Unexpected end-of-file inside comment starting at line %d.\n",
  						linenum, startline);
			state = ERR_STATE;
			break;
		    }
		    if (curchar == '-') {
			if (hyphenCount == 1) {
			    /* This is a comment delimiter */
			    inComment = !inComment;
			    hyphenCount = 0;
			} else {
			    /* beginning of a comment delimiter? */
			    hyphenCount = 1;
			}
		    } else if (curchar == '>') {
			if (!inComment) {
			    /* This is the end of the tag */
			    state = DONE_STATE;
			    break;
			} else {
			    /* The > is inside a comment, so it's not
							 * really the end of the tag */
			    hyphenCount = 0;
			}
		    } else {
			hyphenCount = 0;
		    }
		}
		ti->type = COMMENT_TAG;
		break;
	    }
	    /* fall through */
	case GET_ATT_STATE:
	    if (isspace(curchar) || curchar == '=' || curchar
	        == '>') {
		/* end of the current attribute */
		curPos = FB_GetPointer(fb) - 2;
		if (curPos >= startID) {
		    /* We have an attribute */
		    curPair = (AVPair * )PR_Malloc(sizeof(AVPair));
		    if (!curPair) 
			out_of_memory();
		    curPair->value = NULL;
		    curPair->next = NULL;
		    FB_GetRange(fb, startID, curPos,
		        &curPair->attribute);

		    /* Stick this attribute on the list */
		    if (ti->attListTail) {
			ti->attListTail->next = curPair;
			ti->attListTail = curPair;
		    } else {
			ti->attList = ti->attListTail =
			    curPair;
		    }

		    /* If this is the first attribute, find the type of tag
		     * based on it. Also, start saving the text of the tag. */
		    if (firstAtt) {
			ti->type = GetTagType(curPair->attribute);
			startText = FB_GetPointer(fb)
			    -1;
			firstAtt = PR_FALSE;
		    }
		} else {
		    if (curchar == '=') {
			/* If we don't have any attribute but we do have an
			 * equal sign, that's an error */
			*errStr = PR_smprintf("line %d: Malformed tag starting at line %d.\n",
			     linenum, startline);
			state = ERR_STATE;
			break;
		    }
		}

		/* Compute next state */
		if (curchar == '=') {
		    startID = FB_GetPointer(fb);
		    state = PRE_VAL_WS_STATE;
		} else if (curchar == '>') {
		    state = DONE_STATE;
		} else if (curPair) {
		    state = POST_ATT_WS_STATE;
		} else {
		    state = PRE_ATT_WS_STATE;
		}
	    } else if (isAttChar(curchar)) {
		/* Just another char in the attribute. Do nothing */
		state = GET_ATT_STATE;
	    } else {
		/* bogus char */
		*errStr = PR_smprintf("line %d: Bogus chararacter '%c' in tag.\n",
		     			linenum, curchar);
		state = ERR_STATE;
		break;
	    }
	    break;
	case PRE_ATT_WS_STATE:
	    if (curchar == '>') {
		state = DONE_STATE;
	    } else if (isspace(curchar)) {
		/* more whitespace, do nothing */
	    } else if (isAttChar(curchar)) {
		/* starting another attribute */
		startID = FB_GetPointer(fb) - 1;
		state = GET_ATT_STATE;
	    } else {
		/* bogus char */
		*errStr = PR_smprintf("line %d: Bogus character '%c' in tag.\n",
		     			linenum, curchar);
		state = ERR_STATE;
		break;
	    }
	    break;
	case POST_ATT_WS_STATE:
	    if (curchar == '>') {
		state = DONE_STATE;
	    } else if (isspace(curchar)) {
		/* more whitespace, do nothing */
	    } else if (isAttChar(curchar)) {
		/* starting another attribute */
		startID = FB_GetPointer(fb) - 1;
		state = GET_ATT_STATE;
	    } else if (curchar == '=') {
		/* there was whitespace between the attribute and its equal
		 * sign, which means there's a value coming up */
		state = PRE_VAL_WS_STATE;
	    } else {
		/* bogus char */
		*errStr = PR_smprintf("line %d: Bogus character '%c' in tag.\n",
		     					linenum, curchar);
		state = ERR_STATE;
		break;
	    }
	    break;
	case PRE_VAL_WS_STATE:
	    if (curchar == '>') {
		/* premature end-of-tag (sounds like a personal problem). */
		*errStr = PR_smprintf(
		    "line %d: End of tag while waiting for value.\n",
		     linenum);
		state = ERR_STATE;
		break;
	    } else if (isspace(curchar)) {
		/* more whitespace, do nothing */
		break;
	    } else {
		/* this must be some sort of value. Fall through
				 * to GET_VALUE_STATE */
		startID = FB_GetPointer(fb) - 1;
		state = GET_VALUE_STATE;
	    }
	    /* Fall through if we didn't break on '>' or whitespace */
	case GET_VALUE_STATE:
	    if (isspace(curchar) || curchar == '>') {
		/* end of value */
		curPos = FB_GetPointer(fb) - 2;
		if (curPos >= startID) {
		    /* Grab the value */
		    FB_GetRange(fb, startID, curPos,
		        &curPair->value);
		    curPair->valueLine = linenum;
		} else {
		    /* empty value, leave as NULL */
		}
		if (isspace(curchar)) {
		    state = PRE_ATT_WS_STATE;
		} else {
		    state = DONE_STATE;
		}
	    } else if (curchar == '\"' || curchar == '\'') {
		/* quoted value.  Start recording the value inside the quote*/
		startID = FB_GetPointer(fb);
		state = GET_QUOTED_VAL_STATE;
		PORT_Assert(quotechar == '\0');
		quotechar = curchar; /* look for matching quote type */
	    } else {
		/* just more value */
	    }
	    break;
	case GET_QUOTED_VAL_STATE:
	    PORT_Assert(quotechar != '\0');
	    if (curchar == quotechar) {
		/* end of quoted value */
		curPos = FB_GetPointer(fb) - 2;
		if (curPos >= startID) {
		    /* Grab the value */
		    FB_GetRange(fb, startID, curPos,
		        &curPair->value);
		    curPair->valueLine = linenum;
		} else {
		    /* empty value, leave it as NULL */
		}
		state = GET_ATT_STATE;
		quotechar = '\0';
		startID = FB_GetPointer(fb);
	    } else {
		/* more quoted value, continue */
	    }
	    break;
	case DONE_STATE:
	case ERR_STATE:
	default:
	    ; /* should never get here */
	}
    }

    if (state == DONE_STATE) {
	/* Get the text of the tag */
	curPos = FB_GetPointer(fb) - 1;
	FB_GetRange(fb, startText, curPos, &ti->text);

	/* Return the tag */
	return ti;
    }

    /* Uh oh, an error.  Kill the tag item*/
    DestroyTagItem(ti);
    return NULL;
}


/************************************************************************
 *
 * D e s t r o y H T M L I t e m
 */
static void	
DestroyHTMLItem(HTMLItem *item)
{
    if (item->type == TAG_ITEM) {
	DestroyTagItem(item->item.tag);
    } else {
	if (item->item.text) {
	    PR_Free(item->item.text);
	}
    }
}


/************************************************************************
 *
 * D e s t r o y T a g I t e m
 */
static void	
DestroyTagItem(TagItem*ti)
{
    AVPair * temp;

    if (ti->text) {
	PR_Free(ti->text); 
	ti->text = NULL;
    }

    while (ti->attList) {
	temp = ti->attList;
	ti->attList = ti->attList->next;

	if (temp->attribute) {
	    PR_Free(temp->attribute); 
	    temp->attribute = NULL;
	}
	if (temp->value) {
	    PR_Free(temp->value); 
	    temp->value = NULL;
	}
	PR_Free(temp);
    }

    PR_Free(ti);
}


/************************************************************************
 *
 * G e t T a g T y p e
 */
static TAG_TYPE
GetTagType(char *att)
{
    if (!PORT_Strcasecmp(att, "APPLET")) {
	return APPLET_TAG;
    }
    if (!PORT_Strcasecmp(att, "SCRIPT")) {
	return SCRIPT_TAG;
    }
    if (!PORT_Strcasecmp(att, "LINK")) {
	return LINK_TAG;
    }
    if (!PORT_Strcasecmp(att, "STYLE")) {
	return STYLE_TAG;
    }
    return OTHER_TAG;
}


/************************************************************************
 *
 * F B _ C r e a t e
 */
static FileBuffer*
FB_Create(PRFileDesc*fd)
{
    FileBuffer * fb;
    PRInt32 amountRead;
    PRInt32 storedOffset;

    fb = (FileBuffer * ) PR_Malloc(sizeof(FileBuffer));
    fb->fd = fd;
    storedOffset = PR_Seek(fd, 0, PR_SEEK_CUR);
    PR_Seek(fd, 0, PR_SEEK_SET);
    fb->startOffset = 0;
    amountRead = PR_Read(fd, fb->buf, FILE_BUFFER_BUFSIZE);
    if (amountRead == -1) 
	goto loser;
    fb->maxIndex = amountRead - 1;
    fb->curIndex = 0;
    fb->IsEOF = (fb->curIndex > fb->maxIndex) ? PR_TRUE : PR_FALSE;
    fb->lineNum = 1;

    PR_Seek(fd, storedOffset, PR_SEEK_SET);
    return fb;
loser:
    PR_Seek(fd, storedOffset, PR_SEEK_SET);
    PR_Free(fb);
    return NULL;
}


/************************************************************************
 *
 * F B _ G e t C h a r
 */
static int	
FB_GetChar(FileBuffer *fb)
{
    PRInt32 storedOffset;
    PRInt32 amountRead;
    int	retval = -1;

    if (fb->IsEOF) {
	return EOF;
    }

    storedOffset = PR_Seek(fb->fd, 0, PR_SEEK_CUR);

    retval = (unsigned char) fb->buf[fb->curIndex++];
    if (retval == '\n') 
	fb->lineNum++;

    if (fb->curIndex > fb->maxIndex) {
	/* We're at the end of the buffer. Try to get some new data from the
		 * file */
	fb->startOffset += fb->maxIndex + 1;
	PR_Seek(fb->fd, fb->startOffset, PR_SEEK_SET);
	amountRead = PR_Read(fb->fd, fb->buf, FILE_BUFFER_BUFSIZE);
	if (amountRead == -1)  
	    goto loser;
	fb->maxIndex = amountRead - 1;
	fb->curIndex = 0;
    }

    fb->IsEOF = (fb->curIndex > fb->maxIndex) ? PR_TRUE : PR_FALSE;

loser:
    PR_Seek(fb->fd, storedOffset, PR_SEEK_SET);
    return retval;
}


/************************************************************************
 *
 * F B _ G e t L i n e N u m
 *
 */
static unsigned int	
FB_GetLineNum(FileBuffer *fb)
{
    return fb->lineNum;
}


/************************************************************************
 *
 * F B _ G e t P o i n t e r
 *
 */
static PRInt32
FB_GetPointer(FileBuffer *fb)
{
    return fb->startOffset + fb->curIndex;
}


/************************************************************************
 *
 * F B _ G e t R a n g e
 *
 */
static PRInt32
FB_GetRange(FileBuffer *fb, PRInt32 start, PRInt32 end, char **buf)
{
    PRInt32 amountRead;
    PRInt32 storedOffset;

    *buf = PR_Malloc(end - start + 2);
    if (*buf == NULL) {
	return 0;
    }

    storedOffset = PR_Seek(fb->fd, 0, PR_SEEK_CUR);
    PR_Seek(fb->fd, start, PR_SEEK_SET);
    amountRead = PR_Read(fb->fd, *buf, end - start + 1);
    PR_Seek(fb->fd, storedOffset, PR_SEEK_SET);
    if (amountRead == -1) {
	PR_Free(*buf);
	*buf = NULL;
	return 0;
    }

    (*buf)[end-start+1] = '\0';
    return amountRead;
}


/************************************************************************
 *
 * F B _ D e s t r o y
 *
 */
static void	
FB_Destroy(FileBuffer *fb)
{
    if (fb) {
	PR_Free(fb);
    }
}


/************************************************************************
 *
 * P r i n t T a g I t e m
 *
 */
static void	
PrintTagItem(PRFileDesc *fd, TagItem *ti)
{
    AVPair * pair;

    PR_fprintf(fd, "TAG:\n----\nType: ");
    switch (ti->type) {
    case APPLET_TAG:
	PR_fprintf(fd, "applet\n");
	break;
    case SCRIPT_TAG:
	PR_fprintf(fd, "script\n");
	break;
    case LINK_TAG:
	PR_fprintf(fd, "link\n");
	break;
    case STYLE_TAG:
	PR_fprintf(fd, "style\n");
	break;
    case COMMENT_TAG:
	PR_fprintf(fd, "comment\n");
	break;
    case OTHER_TAG:
    default:
	PR_fprintf(fd, "other\n");
	break;
    }

    PR_fprintf(fd, "Attributes:\n");
    for (pair = ti->attList; pair; pair = pair->next) {
	PR_fprintf(fd, "\t%s=%s\n", pair->attribute,
	    pair->value ? pair->value : "");
    }
    PR_fprintf(fd, "Text:%s\n", ti->text ? ti->text : "");

    PR_fprintf(fd, "---End of tag---\n");
}


/************************************************************************
 *
 * P r i n t H T M L S t r e a m
 *
 */
static void	
PrintHTMLStream(PRFileDesc *fd, HTMLItem *head)
{
    while (head) {
	if (head->type == TAG_ITEM) {
	    PrintTagItem(fd, head->item.tag);
	} else {
	    PR_fprintf(fd, "\nTEXT:\n-----\n%s\n-----\n\n", head->item.text);
	}
	head = head->next;
    }
}


/************************************************************************
 *
 * S a v e I n l i n e S c r i p t
 *
 */
static int	
SaveInlineScript(char *text, char *id, char *basedir, char *archiveDir)
{
    char	*filename = NULL;
    PRFileDesc * fd = NULL;
    int	retval = -1;
    PRInt32 writeLen;
    char	*ilDir = NULL;

    if (!text || !id || !archiveDir) {
	return - 1;
    }

    if (dumpParse) {
	PR_fprintf(outputFD, "SaveInlineScript: text=%s, id=%s, \n"
	    "basedir=%s, archiveDir=%s\n",
	    text, id, basedir, archiveDir);
    }

    /* Make sure the archive directory is around */
    if (ensureExists(basedir, archiveDir) != PR_SUCCESS) {
	PR_fprintf(errorFD,
	    "ERROR: Unable to create archive directory %s.\n", archiveDir);
	errorCount++;
	return - 1;
    }

    /* Make sure the inline script directory is around */
    ilDir = PR_smprintf("%s/inlineScripts", archiveDir);
    scriptdir = "inlineScripts";
    if (ensureExists(basedir, ilDir) != PR_SUCCESS) {
	PR_fprintf(errorFD,
	    "ERROR: Unable to create directory %s.\n", ilDir);
	errorCount++;
	return - 1;
    }

    filename = PR_smprintf("%s/%s/%s", basedir, ilDir, id);

    /* If the file already exists, give a warning, then blow it away */
    if (PR_Access(filename, PR_ACCESS_EXISTS) == PR_SUCCESS) {
	PR_fprintf(errorFD,
	    "warning: file \"%s\" already exists--will overwrite.\n",
	     			filename);
	warningCount++;
	if (rm_dash_r(filename)) {
	    PR_fprintf(errorFD, "ERROR: Unable to delete %s.\n", filename);
	    errorCount++;
	    goto finish;
	}
    }

    /* Write text into file with name id */
    fd = PR_Open(filename, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0777);
    if (!fd) {
	PR_fprintf(errorFD, "ERROR: Unable to create file \"%s\".\n",
	     			filename);
	errorCount++;
	goto finish;
    }
    writeLen = strlen(text);
    if ( PR_Write(fd, text, writeLen) != writeLen) {
	PR_fprintf(errorFD, "ERROR: Unable to write to file \"%s\".\n",
	     			filename);
	errorCount++;
	goto finish;
    }

    retval = 0;
finish:
    if (filename) {
	PR_smprintf_free(filename);
    }
    if (ilDir) {
	PR_smprintf_free(ilDir);
    }
    if (fd) {
	PR_Close(fd);
    }
    return retval;
}


/************************************************************************
 *
 * S a v e U n n a m a b l e S c r i p t
 *
 */
static int	
SaveUnnamableScript(char *text, char *basedir, char *archiveDir,
char *HTMLfilename)
{
    char	*id = NULL;
    char	*ext = NULL;
    char	*start = NULL;
    int	retval = -1;

    if (!text || !archiveDir || !HTMLfilename) {
	return - 1;
    }

    if (dumpParse) {
	PR_fprintf(outputFD, "SaveUnnamableScript: text=%s, basedir=%s,\n"
	    "archiveDir=%s, filename=%s\n", text, basedir, archiveDir,
	     			HTMLfilename);
    }

    /* Construct the filename */
    ext = PL_strrchr(HTMLfilename, '.');
    if (ext) {
	*ext = '\0';
    }
    for (start = HTMLfilename; strpbrk(start, "/\\"); 
         start = strpbrk(start, "/\\") + 1)
	/* do nothing */;
    if (*start == '\0') 
	start = HTMLfilename;
    id = PR_smprintf("_%s%d", start, idOrdinal++);
    if (ext) {
	*ext = '.';
    }

    /* Now call SaveInlineScript to do the work */
    retval = SaveInlineScript(text, id, basedir, archiveDir);

    PR_Free(id);

    return retval;
}


/************************************************************************
 *
 * S a v e S o u r c e
 *
 */
static int	
SaveSource(char *src, char *codebase, char *basedir, char *archiveDir)
{
    char	*from = NULL, *to = NULL;
    int	retval = -1;
    char	*arcDir = NULL;

    if (!src || !archiveDir) {
	return - 1;
    }

    if (dumpParse) {
	PR_fprintf(outputFD, "SaveSource: src=%s, codebase=%s, basedir=%s,\n"
	    "archiveDir=%s\n", src, codebase, basedir, archiveDir);
    }

    if (codebase) {
	arcDir = PR_smprintf("%s/%s/%s/", basedir, codebase, archiveDir);
    } else {
	arcDir = PR_smprintf("%s/%s/", basedir, archiveDir);
    }

    if (codebase) {
	from = PR_smprintf("%s/%s/%s", basedir, codebase, src);
	to = PR_smprintf("%s%s", arcDir, src);
    } else {
	from = PR_smprintf("%s/%s", basedir, src);
	to = PR_smprintf("%s%s", arcDir, src);
    }

    if (make_dirs(to, 0777)) {
	PR_fprintf(errorFD,
	    "ERROR: Unable to create archive directory %s.\n", archiveDir);
	errorCount++;
	goto finish;
    }

    retval = copyinto(from, to);
finish:
    if (from) 
	PR_Free(from);
    if (to) 
	PR_Free(to);
    if (arcDir) 
	PR_Free(arcDir);
    return retval;
}


/************************************************************************
 *
 * T a g T y p e T o S t r i n g
 *
 */
char	*
TagTypeToString(TAG_TYPE type)
{
    switch (type) {
    case APPLET_TAG:
	return "APPLET";
    case SCRIPT_TAG:
	return "SCRIPT";
    case LINK_TAG:
	return "LINK";
    case STYLE_TAG:
	return "STYLE";
    default:
	break;
    }
    return "unknown";
}


/************************************************************************
 *
 * e x t r a c t _ j s
 *
 */
static int	
extract_js(char *filename)
{
    PRFileDesc * fd = NULL;
    FileBuffer * fb = NULL;
    HTMLItem * head = NULL;
    HTMLItem * tail = NULL;
    HTMLItem * curitem = NULL;
    HTMLItem * styleList	= NULL;
    HTMLItem * styleListTail	= NULL;
    HTMLItem * entityList	= NULL;
    HTMLItem * entityListTail	= NULL;
    TagItem * tagp = NULL;
    char	*text = NULL;
    char	*tagerr = NULL;
    char	*archiveDir = NULL;
    char	*firstArchiveDir = NULL;
    char	*basedir = NULL;
    PRInt32    textStart;
    PRInt32    curOffset;
    HTML_STATE state;
    int	       curchar;
    int	       retval = -1;
    unsigned int linenum, startLine;

    /* Initialize the implicit ID counter for each file */
    idOrdinal = 0;

    /*
     * First, parse the HTML into a stream of tags and text.
     */

    fd = PR_Open(filename, PR_RDONLY, 0);
    if (!fd) {
	PR_fprintf(errorFD, "Unable to open %s for reading.\n", filename);
	errorCount++;
	return - 1;
    }

    /* Construct base directory of filename. */
     {
	char	*cp;

	basedir = PL_strdup(filename);

	/* Remove trailing slashes */
	while ( (cp = PL_strprbrk(basedir, "/\\")) == 
	    (basedir + strlen(basedir) - 1)) {
	    *cp = '\0';
	}

	/* Now remove everything from the last slash (which will be followed
	 * by a filename) to the end */
	cp = PL_strprbrk(basedir, "/\\");
	if (cp) {
	    *cp = '\0';
	}
    }

    state = TEXT_HTML_STATE;

    fb = FB_Create(fd);

    textStart = 0;
    startLine = 0;
    while (linenum = FB_GetLineNum(fb), (curchar = FB_GetChar(fb)) !=
        EOF) {
	switch (state) {
	case TEXT_HTML_STATE:
	    if (curchar == '<') {
		/*
		 * Found a tag
		 */
		/* Save the text so far to a new text item */
		curOffset = FB_GetPointer(fb) - 2;
		if (curOffset >= textStart) {
		    if (FB_GetRange(fb, textStart, curOffset,
		         &text) != 
		        curOffset - textStart + 1)  {
			PR_fprintf(errorFD,
			    "Unable to read from %s.\n",
			     filename);
			errorCount++;
			goto loser;
		    }
		    /* little fudge here.  If the first character on a line
		     * is '<', meaning a new tag, the preceding text item
		     * actually ends on the previous line.  In this case
		     * we will be saying that the text segment ends on the
		     * next line. I don't think this matters for text items. */
		    curitem = CreateTextItem(text, startLine,
		         linenum);
		    text = NULL;
		    if (tail == NULL) {
			head = tail = curitem;
		    } else {
			tail->next = curitem;
			tail = curitem;
		    }
		}

		/* Process the tag */
		tagp = ProcessTag(fb, &tagerr);
		if (!tagp) {
		    if (tagerr) {
			PR_fprintf(errorFD, "Error in file %s: %s\n",
						  filename, tagerr);
			errorCount++;
		    } else {
			PR_fprintf(errorFD,
			    "Error in file %s, in tag starting at line %d\n",
						  filename, linenum);
			errorCount++;
		    }
		    goto loser;
		}
		/* Add the tag to the list */
		curitem = CreateTagItem(tagp, linenum, FB_GetLineNum(fb));
		if (tail == NULL) {
		    head = tail = curitem;
		} else {
		    tail->next = curitem;
		    tail = curitem;
		}

		/* What's the next state */
		if (tagp->type == SCRIPT_TAG) {
		    state = SCRIPT_HTML_STATE;
		}

		/* Start recording text from the new offset */
		textStart = FB_GetPointer(fb);
		startLine = FB_GetLineNum(fb);
	    } else {
		/* regular character.  Next! */
	    }
	    break;
	case SCRIPT_HTML_STATE:
	    if (curchar == '<') {
		char	*cp;
		/*
		 * If this is a </script> tag, then we're at the end of the
		 * script.  Otherwise, ignore
		 */
		curOffset = FB_GetPointer(fb) - 1;
		cp = NULL;
		if (FB_GetRange(fb, curOffset, curOffset + 8, &cp) != 9) {
		    if (cp) { 
			PR_Free(cp); 
			cp = NULL; 
		    }
		} else {
		    /* compare the strings */
		    if ( !PORT_Strncasecmp(cp, "</script>", 9) ) {
			/* This is the end of the script. Record the text. */
			curOffset--;
			if (curOffset >= textStart) {
			    if (FB_GetRange(fb, textStart, curOffset, &text) != 
			        curOffset - textStart + 1) {
				PR_fprintf(errorFD, "Unable to read from %s.\n",
				     filename);
				errorCount++;
				goto loser;
			    }
			    curitem = CreateTextItem(text, startLine, linenum);
			    text = NULL;
			    if (tail == NULL) {
				head = tail = curitem;
			    } else {
				tail->next = curitem;
				tail = curitem;
			    }
			}

			/* Now parse the /script tag and put it on the list */
			tagp = ProcessTag(fb, &tagerr);
			if (!tagp) {
			    if (tagerr) {
				PR_fprintf(errorFD, "Error in file %s: %s\n",
				     filename, tagerr);
			    } else {
				PR_fprintf(errorFD, 
				    "Error in file %s, in tag starting at"
				    " line %d\n", filename, linenum);
			    }
			    errorCount++;
			    goto loser;
			}
			curitem = CreateTagItem(tagp, linenum,
						FB_GetLineNum(fb));
			if (tail == NULL) {
			    head = tail = curitem;
			} else {
			    tail->next = curitem;
			    tail = curitem;
			}

			/* go back to text state */
			state = TEXT_HTML_STATE;

			textStart = FB_GetPointer(fb);
			startLine = FB_GetLineNum(fb);
		    }
		}
	    }
	    break;
	}
    }

    /* End of the file.  Wrap up any remaining text */
    if (state == SCRIPT_HTML_STATE) {
	if (tail && tail->type == TAG_ITEM) {
	    PR_fprintf(errorFD, "ERROR: <SCRIPT> tag at %s:%d is not followed "
	        "by a </SCRIPT> tag.\n", filename, tail->startLine);
	} else {
	    PR_fprintf(errorFD, "ERROR: <SCRIPT> tag in file %s is not followed"
	        " by a </SCRIPT tag.\n", filename);
	}
	errorCount++;
	goto loser;
    }
    curOffset = FB_GetPointer(fb) - 1;
    if (curOffset >= textStart) {
	text = NULL;
	if ( FB_GetRange(fb, textStart, curOffset, &text) != 
	    curOffset - textStart + 1) {
	    PR_fprintf(errorFD, "Unable to read from %s.\n", filename);
	    errorCount++;
	    goto loser;
	}
	curitem = CreateTextItem(text, startLine, linenum);
	text = NULL;
	if (tail == NULL) {
	    head = tail = curitem;
	} else {
	    tail->next = curitem;
	    tail = curitem;
	}
    }

    if (dumpParse) {
	PrintHTMLStream(outputFD, head);
    }

    /*
     * Now we have a stream of tags and text.  Go through and deal with each.
     */
    for (curitem = head; curitem; curitem = curitem->next) {
	TagItem * tagp = NULL;
	AVPair * pairp = NULL;
	char	*src = NULL, *id = NULL, *codebase = NULL;
	PRBool hasEventHandler = PR_FALSE;
	int	i;

	/* Reset archive directory for each tag */
	if (archiveDir) {
	    PR_Free(archiveDir); 
	    archiveDir = NULL;
	}

	/* We only analyze tags */
	if (curitem->type != TAG_ITEM) {
	    continue;
	}

	tagp = curitem->item.tag;

	/* go through the attributes to get information */
	for (pairp = tagp->attList; pairp; pairp = pairp->next) {

	    /* ARCHIVE= */
	    if ( !PL_strcasecmp(pairp->attribute, "archive")) {
		if (archiveDir) {
		    /* Duplicate attribute.  Print warning */
		    PR_fprintf(errorFD,
		        "warning: \"%s\" attribute overwrites previous attribute"
		        " in tag starting at %s:%d.\n",
		        pairp->attribute, filename, curitem->startLine);
		    warningCount++;
		    PR_Free(archiveDir);
		}
		archiveDir = PL_strdup(pairp->value);

		/* Substiture ".arc" for ".jar" */
		if ( (PL_strlen(archiveDir) < 4) || 
		    PL_strcasecmp((archiveDir + strlen(archiveDir) -4), 
			".jar")) {
		    PR_fprintf(errorFD,
		        "warning: ARCHIVE attribute should end in \".jar\" in tag"
		        " starting on %s:%d.\n", filename, curitem->startLine);
		    warningCount++;
		    PR_Free(archiveDir);
		    archiveDir = PR_smprintf("%s.arc", archiveDir);
		} else {
		    PL_strcpy(archiveDir + strlen(archiveDir) -4, ".arc");
		}

		/* Record the first archive.  This will be used later if
		 * the archive is not specified */
		if (firstArchiveDir == NULL) {
		    firstArchiveDir = PL_strdup(archiveDir);
		}
	    } 
	    /* CODEBASE= */
	    else if ( !PL_strcasecmp(pairp->attribute, "codebase")) {
		if (codebase) {
		    /* Duplicate attribute.  Print warning */
		    PR_fprintf(errorFD,
		        "warning: \"%s\" attribute overwrites previous attribute"
		        " in tag staring at %s:%d.\n",
		        pairp->attribute, filename, curitem->startLine);
		    warningCount++;
		}
		codebase = pairp->value;
	    } 
	    /* SRC= and HREF= */
	    else if ( !PORT_Strcasecmp(pairp->attribute, "src") ||
	        !PORT_Strcasecmp(pairp->attribute, "href") ) {
		if (src) {
		    /* Duplicate attribute.  Print warning */
		    PR_fprintf(errorFD,
		        "warning: \"%s\" attribute overwrites previous attribute"
		        " in tag staring at %s:%d.\n",
		        pairp->attribute, filename, curitem->startLine);
		    warningCount++;
		}
		src = pairp->value;
	    } 
	    /* CODE= */
	    else if (!PORT_Strcasecmp(pairp->attribute, "code") ) {
		/*!!!XXX Change PORT to PL all over this code !!! */
		if (src) {
		    /* Duplicate attribute.  Print warning */
		    PR_fprintf(errorFD,
		        "warning: \"%s\" attribute overwrites previous attribute"
		        " ,in tag staring at %s:%d.\n",
		        pairp->attribute, filename, curitem->startLine);
		    warningCount++;
		}
		src = pairp->value;

		/* Append a .class if one is not already present */
		if ( (PL_strlen(src) < 6) || 
		    PL_strcasecmp( (src + PL_strlen(src) - 6), ".class") ) {
		    src = PR_smprintf("%s.class", src);
		    /* Put this string back into the data structure so it
		     * will be deallocated properly */
		    PR_Free(pairp->value);
		    pairp->value = src;
		}
	    } 
	    /* ID= */
	    else if (!PL_strcasecmp(pairp->attribute, "id") ) {
		if (id) {
		    /* Duplicate attribute.  Print warning */
		    PR_fprintf(errorFD,
		        "warning: \"%s\" attribute overwrites previous attribute"
		        " in tag staring at %s:%d.\n",
		        pairp->attribute, filename, curitem->startLine);
		    warningCount++;
		}
		id = pairp->value;
	    }

	    /* STYLE= */
	    /* style= attributes, along with JS entities, are stored into
	     * files with dynamically generated names. The filenames are
	     * based on the order in which the text is found in the file.
	     * All JS entities on all lines up to and including the line
	     * containing the end of the tag that has this style= attribute
	     * will be processed before this style=attribute.  So we need
	     * to record the line that this _tag_ (not the attribute) ends on.
	     */
	    else if (!PL_strcasecmp(pairp->attribute, "style") && pairp->value) 
	    {
		HTMLItem * styleItem;
		/* Put this item on the style list */
		styleItem = CreateTextItem(PL_strdup(pairp->value),
		    curitem->startLine, curitem->endLine);
		if (styleListTail == NULL) {
		    styleList = styleListTail = styleItem;
		} else {
		    styleListTail->next = styleItem;
		    styleListTail = styleItem;
		}
	    } 
	    /* Event handlers */
	    else {
		for (i = 0; i < num_handlers; i++) {
		    if (!PL_strcasecmp(event_handlers[i], pairp->attribute)) {
			hasEventHandler = PR_TRUE;
			break;
		    }
		}
	    }


	    /* JS Entity */
	    {
		char	*entityStart, *entityEnd;
		HTMLItem * entityItem;

		/* go through each JavaScript entity ( &{...}; ) and store it
		 * in the entityList.  The important thing is to record what
		 * line number it's on, so we can get it in the right order
		 * in relation to style= attributes.
		 * Apparently, these can't flow across lines, so the start and
		 * end line will be the same.  That helps matters.
		 */
		entityEnd = pairp->value;
		while ( entityEnd && 
		    (entityStart = PL_strstr(entityEnd, "&{")) /*}*/ != NULL) {
		    entityStart += 2; /* point at beginning of actual entity */
		    entityEnd = PL_strchr(entityStart, '}');
		    if (entityEnd) {
			/* Put this item on the entity list */
			*entityEnd = '\0';
			entityItem = CreateTextItem(PL_strdup(entityStart),
					    pairp->valueLine, pairp->valueLine);
			*entityEnd = /* { */ '}';
			if (entityListTail) {
			    entityListTail->next = entityItem;
			    entityListTail = entityItem;
			} else {
			    entityList = entityListTail = entityItem;
			}
		    }
		}
	    }
	}

	/* If no archive was supplied, we use the first one of the file */
	if (!archiveDir && firstArchiveDir) {
	    archiveDir = PL_strdup(firstArchiveDir);
	}

	/* If we have an event handler, we need to archive this tag */
	if (hasEventHandler) {
	    if (!id) {
		PR_fprintf(errorFD,
		    "warning: tag starting at %s:%d has event handler but"
		    " no ID attribute.  The tag will not be signed.\n",
					filename, curitem->startLine);
		warningCount++;
	    } else if (!archiveDir) {
		PR_fprintf(errorFD,
		    "warning: tag starting at %s:%d has event handler but"
		    " no ARCHIVE attribute.  The tag will not be signed.\n",
					    filename, curitem->startLine);
		warningCount++;
	    } else {
		if (SaveInlineScript(tagp->text, id, basedir, archiveDir)) {
		    goto loser;
		}
	    }
	}

	switch (tagp->type) {
	case APPLET_TAG:
	    if (!src) {
		PR_fprintf(errorFD,
		    "error: APPLET tag starting on %s:%d has no CODE "
		    "attribute.\n", filename, curitem->startLine);
		errorCount++;
		goto loser;
	    } else if (!archiveDir) {
		PR_fprintf(errorFD,
		    "error: APPLET tag starting on %s:%d has no ARCHIVE "
		    "attribute.\n", filename, curitem->startLine);
		errorCount++;
		goto loser;
	    } else {
		if (SaveSource(src, codebase, basedir, archiveDir)) {
		    goto loser;
		}
	    }
	    break;
	case SCRIPT_TAG:
	case LINK_TAG:
	case STYLE_TAG:
	    if (!archiveDir) {
		PR_fprintf(errorFD,
		    "error: %s tag starting on %s:%d has no ARCHIVE "
		    "attribute.\n", TagTypeToString(tagp->type),
					    filename, curitem->startLine);
		errorCount++;
		goto loser;
	    } else if (src) {
		if (SaveSource(src, codebase, basedir, archiveDir)) {
		    goto loser;
		}
	    } else if (id) {
		/* Save the next text item */
		if (!curitem->next || (curitem->next->type !=
		    TEXT_ITEM)) {
		    PR_fprintf(errorFD,
		        "warning: %s tag starting on %s:%d is not followed"
		        " by script text.\n", TagTypeToString(tagp->type),
					    filename, curitem->startLine);
		    warningCount++;
		    /* just create empty file */
		    if (SaveInlineScript("", id, basedir, archiveDir)) {
			goto loser;
		    }
		} else {
		    curitem = curitem->next;
		    if (SaveInlineScript(curitem->item.text,
		         id, basedir,
		        archiveDir)) {
			goto loser;
		    }
		}
	    } else {
		/* No src or id tag--warning */
		PR_fprintf(errorFD,
		    "warning: %s tag starting on %s:%d has no SRC or"
		    " ID attributes.  Will not sign.\n",
		    TagTypeToString(tagp->type), filename, curitem->startLine);
		warningCount++;
	    }
	    break;
	default:
	    /* do nothing for other tags */
	    break;
	}

    }

    /* Now deal with all the unnamable scripts */
    if (firstArchiveDir) {
	HTMLItem * style, *entity;

	/* Go through the lists of JS entities and style attributes.  Do them
	 * in chronological order within a list.  Pick the list with the lower
	 * endLine. In case of a tie, entities come first.
	 */
	style = styleList; 
	entity = entityList;
	while (style || entity) {
	    if (!entity || (style && (style->endLine < entity->endLine))) {
		/* Process style */
		SaveUnnamableScript(style->item.text, basedir, firstArchiveDir,
				    filename);
		style = style->next;
	    } else {
		/* Process entity */
		SaveUnnamableScript(entity->item.text, basedir, firstArchiveDir,
				    filename);
		entity = entity->next;
	    }
	}
    }


    retval = 0;
loser:
    /* Blow away the stream */
    while (head) {
	curitem = head;
	head = head->next;
	DestroyHTMLItem(curitem);
    }
    while (styleList) {
	curitem = styleList;
	styleList = styleList->next;
	DestroyHTMLItem(curitem);
    }
    while (entityList) {
	curitem = entityList;
	entityList = entityList->next;
	DestroyHTMLItem(curitem);
    }
    if (text) {
	PR_Free(text); 
	text = NULL;
    }
    if (fb) {
	FB_Destroy(fb); 
	fb = NULL;
    }
    if (fd) {
	PR_Close(fd);
    }
    if (tagerr) {
	PR_smprintf_free(tagerr); 
	tagerr = NULL;
    }
    if (archiveDir) {
	PR_Free(archiveDir); 
	archiveDir = NULL;
    }
    if (firstArchiveDir) {
	PR_Free(firstArchiveDir); 
	firstArchiveDir = NULL;
    }
    return retval;
}


/**********************************************************************
 *
 * e n s u r e E x i s t s
 *
 * Check for existence of indicated directory.  If it doesn't exist,
 * it will be created.
 * Returns PR_SUCCESS if the directory is present, PR_FAILURE otherwise.
 */
static PRStatus
ensureExists (char *base, char *path)
{
    char	fn [FNSIZE];
    PRDir * dir;
    sprintf (fn, "%s/%s", base, path);

    /*PR_fprintf(outputFD, "Trying to open directory %s.\n", fn);*/

    if ( (dir = PR_OpenDir(fn)) ) {
	PR_CloseDir(dir);
	return PR_SUCCESS;
    }
    return PR_MkDir(fn, 0777);
}


/***************************************************************************
 *
 * m a k e _ d i r s
 *
 * Ensure that the directory portion of the path exists.  This may require
 * making the directory, and its parent, and its parent's parent, etc.
 */
static int	
make_dirs(char *path, int file_perms)
{
    char	*Path;
    char	*start;
    char	*sep;
    int	ret = 0;
    PRFileInfo info;

    if (!path) {
	return 0;
    }

    Path = PL_strdup(path);
    start = strpbrk(Path, "/\\");
    if (!start) {
	return 0;
    }
    start++; /* start right after first slash */

    /* Each time through the loop add one more directory. */
    while ( (sep = strpbrk(start, "/\\")) ) {
	*sep = '\0';

	if ( PR_GetFileInfo(Path, &info) != PR_SUCCESS) {
	    /* No such dir, we have to create it */
	    if ( PR_MkDir(Path, file_perms) != PR_SUCCESS) {
		PR_fprintf(errorFD, "ERROR: Unable to create directory %s.\n",
		     					Path);
		errorCount++;
		ret = -1;
		goto loser;
	    }
	} else {
	    /* something exists by this name, make sure it's a directory */
	    if ( info.type != PR_FILE_DIRECTORY ) {
		PR_fprintf(errorFD, "ERROR: Unable to create directory %s.\n",
		     					Path);
		errorCount++;
		ret = -1;
		goto loser;
	    }
	}

	start = sep + 1; /* start after the next slash */
	*sep = '/';
    }

loser:
    PR_Free(Path);
    return ret;
}


/*
 *  c o p y i n t o
 *
 *  Function to copy file "from" to path "to".
 *
 */
static int	
copyinto (char *from, char *to)
{
    PRInt32 num;
    char	buf [BUFSIZ];
    PRFileDesc * infp = NULL, *outfp = NULL;
    int	retval = -1;

    if ((infp = PR_Open(from, PR_RDONLY, 0777)) == NULL) {
	PR_fprintf(errorFD, "ERROR: Unable to open \"%s\" for reading.\n",
	     			from);
	errorCount++;
	goto finish;
    }

    /* If to already exists, print a warning before deleting it */
    if (PR_Access(to, PR_ACCESS_EXISTS) == PR_SUCCESS) {
	PR_fprintf(errorFD, "warning: %s already exists--will overwrite\n", to);
	warningCount++;
	if (rm_dash_r(to)) {
	    PR_fprintf(errorFD,
	        "ERROR: Unable to remove %s.\n", to);
	    errorCount++;
	    goto finish;
	}
    }

    if ((outfp = PR_Open(to, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0777))
         == NULL) {
	char	*errBuf = NULL;

	errBuf = PR_Malloc(PR_GetErrorTextLength() + 1);
	PR_fprintf(errorFD, "ERROR: Unable to open \"%s\" for writing.\n", to);
	if (PR_GetErrorText(errBuf)) {
	    PR_fprintf(errorFD, "Cause: %s\n", errBuf);
	}
	if (errBuf) {
	    PR_Free(errBuf);
	}
	errorCount++;
	goto finish;
    }

    while ( (num = PR_Read(infp, buf, BUFSIZ)) > 0) {
	if (PR_Write(outfp, buf, num) != num) {
	    PR_fprintf(errorFD, "ERROR: Error writing to %s.\n", to);
	    errorCount++;
	    goto finish;
	}
    }

    retval = 0;
finish:
    if (infp) 
	PR_Close(infp);
    if (outfp) 
	PR_Close(outfp);

    return retval;
}


