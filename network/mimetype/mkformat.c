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
 * determine the format of a file
 *
 * Started with Rob's code and
 * then hacked up by Lou.
 */

#include "plstr.h"
#include "prmem.h"
#include "mkutils.h"
#include "mkformat.h"
#include "netutils.h"
#include "net_xp_file.h"

static char *net_default_types [] = {
# include "ngtypes.h"
  0 };

/* func proto
 */
PRIVATE int cinfo_parse_mcc_line(char *line, Bool is_external,
					Bool is_local, char **src_string);
PRIVATE int net_cinfo_merge(char *fn, Bool is_local);

/* ----------------------------- Static data ------------------------------ */

/* The master database of content info (linked list) */
PRIVATE XP_List * NET_ciMasterList=0;

/*
 * cinfo.c: Content Information for a file, i.e. its type, etc.
 *
 * See cinfo.h for full details.
 *
 * Rob McCool
 */

/* init types loads standard types so that a config file
 * is not required and can also read
 * a mime types file if filename is passed in non empty and
 * non null
 *
 * on each call to NET_InitFileFormatTypes the extension list
 * will be completely free'd and rebuilt.
 *
 * if you pass in NULL filename pointers they will not be
 * attempted to be read
 */
PUBLIC int
NET_InitFileFormatTypes(char * personal_file, char *global_file)
{
  /* init the data structures
   */
  int i = 0;

#ifdef XP_UNIX
  NET_CleanupFileFormat(NULL);
#else /* !XP_UNIX */
  NET_CleanupFileFormat();
#endif /* !XP_UNIX */
  NET_ciMasterList = XP_ListNew();

  TRACEMSG((" Setting up File Format code"));

  while (net_default_types [i])
	{
	  char buf [256];
	  char *src_string = 0;
	  PL_strcpy (buf, net_default_types [i]);
	  StrAllocCopy(src_string, buf);
	  cinfo_parse_mcc_line (buf, FALSE, FALSE, &src_string); /* destroys buf */
	  i++;
	  if (src_string) PR_Free(src_string);
	  
	}

  /* read the external mime types file and return */
  if(global_file)
	{
	  net_cinfo_merge(global_file, FALSE);
	}
  if(personal_file)
	{
	  net_cinfo_merge(personal_file, TRUE);
	}

  return(0);
}

/* -------------------------- Module structures --------------------------- */



static NET_cinfo default_cinfo_type = {
#ifdef MCC_PROXY
    MAGNUS_INTERNAL_UNKNOWN,
#else
	UNKNOWN_CONTENT_TYPE,
#endif
	0,
	0,
	0,
	0,
	0,
    TRUE,
	0
};

#ifdef NOT_USED
static NET_cinfo default_cinfo_enc = {
	0,
	0,
	0,
	0,
	0,
	0,
    1,
	0
};
#endif /* NOT_USED */

/* ------------------------------ _cinfo_free ------------------------------ */


PRIVATE void
_cinfo_free(NET_cinfo *ci)
{

    PR_FREEIF(ci->type);
    PR_FREEIF(ci->encoding);
    PR_FREEIF(ci->language);
    PR_FREEIF(ci->desc);
    PR_FREEIF(ci->icon);
    PR_FREEIF(ci->alt_text);
}


/* ---------------------------- NET_cdataCreate ---------------------------- */


PUBLIC NET_cdataStruct *
NET_cdataCreate(void)
{
  NET_cdataStruct *cd = PR_NEW(NET_cdataStruct);

  if(!cd)
	return(NULL);

  memset(cd, 0, sizeof(NET_cdataStruct));

  return cd;
}


/* ----------------------------- NET_cdataFree ---------------------------- */


PUBLIC void
NET_cdataFree(NET_cdataStruct *cd)
{
  register int x;

  if(cd->exts)
	{
	  for(x = 0; x < cd->num_exts; x++)
		PR_Free(cd->exts[x]);
	  PR_Free(cd->exts);
	}
  if ( cd->src_string )
	PR_Free(cd->src_string);

  _cinfo_free(&cd->ci);

  PR_Free(cd);
  cd = 0;
}

/* ---------------------------- NET_cdataAdd ------------------------------- */

PUBLIC void
NET_cdataAdd(NET_cdataStruct *cd)
{
    XP_ListAddObject(NET_ciMasterList, cd);
}

/* ---------------------------- NET_cdataRemove ---------------------------- */

PUBLIC void
NET_cdataRemove(NET_cdataStruct *cd)
{
  XP_ListRemoveObject(NET_ciMasterList, cd);

  NET_cdataFree(cd);
}

PUBLIC NET_cdataStruct*
NET_cdataExist(NET_cdataStruct *old_cd )
{
  NET_cdataStruct *found_cd = NULL;
  NET_cdataStruct *cd = NULL;
  XP_List *infoList;

  infoList = cinfo_MasterListPointer();

  if ( !infoList )
	return found_cd;

  while ((cd= (NET_cdataStruct *)XP_ListNextObject(infoList)) != NULL)
	{
	  if ( old_cd->ci.type &&
		   cd->ci.type &&
		   !PL_strcasecmp(old_cd->ci.type, cd->ci.type))
		{
		  /* found matching type */
		  found_cd = cd;
		  break;
		}
	  else if ( !old_cd->ci.type &&
				old_cd->ci.encoding &&
				cd->ci.encoding &&
				!PL_strcasecmp(old_cd->ci.encoding, cd->ci.encoding))
		{
		  /* found matching encoding */
		  found_cd = cd;
		  break;
		}
	}
  return found_cd;
}

#if 0
PRIVATE
Bool
net_cdata_exist_ext( char *ext, NET_cdataStruct *cd )
{
  Bool not_found = FALSE;
  int n;

  if (!cd->exts) return not_found;

  for ( n = 0; n < cd->num_exts; n++ )
	{
	  if (!PL_strcasecmp(cd->exts[n], ext) )
		{
		  not_found = TRUE;
		  break;
		}
	}
  return not_found;
}
#endif

/* ---------------------------- net_cdata_new_ext ----------------------------- */


PRIVATE
void
net_cdata_new_ext(char *ext, NET_cdataStruct *cd)
{
  int n = cd->num_exts;

  cd->exts = (char **) (n ? PR_Realloc(cd->exts, (n+1)*sizeof(char *))
						: PR_Malloc(sizeof(char *)));

  if(!cd->exts)
	return;

  cd->exts[n] = 0;
  StrAllocCopy(cd->exts[n], ext);
  cd->num_exts = ++n;
}


/* ----------------------------- cinfo_merge ------------------------------ */


int _cinfo_parse_mimetypes(XP_File fp, char *t, Bool is_local);
int _cinfo_parse_mcc(XP_File fp, char *t, Bool is_local);

PRIVATE int
net_cinfo_merge(char *fn, Bool is_local)
{
  XP_File fp;
  char t[CINFO_MAX_LEN];
  int ret;

  TRACEMSG(("\n\nnet_cinfo_merge: called with %s\n\n\n", fn));
  if(!(fp = NET_XP_FileOpen(fn ? fn : "mime.types", xpMimeTypes, XP_FILE_READ))) {
	TRACEMSG(("cinfo.c: File open failed"));
	return -1;
  }

  TRACEMSG(("Reading mime.types file"));

  if(!(NET_XP_FileReadLine(t, CINFO_MAX_LEN, fp))) {
	TRACEMSG(("cinfo.c: Mime types file is empty"));
	return -1;

  } else if(!strncmp(t, MCC_MT_MAGIC, MCC_MT_MAGIC_LEN) ||
			!strncmp(t, NCC_MT_MAGIC, NCC_MT_MAGIC_LEN)) {
	/* Look for magic number */
	TRACEMSG(("Loading MCC mime types file"));
	ret = _cinfo_parse_mcc(fp, t, is_local);

  } else {
	TRACEMSG(("Loading old NCSA mime types file"));
	ret = _cinfo_parse_mimetypes(fp, t, is_local);
  }

  NET_XP_FileClose(fp);
  return ret;
}


static NET_cinfo *
net_cinfo_find_type_or_encoding (char *uri, XP_Bool type_p)
{
  char *start = uri;
  char *end, *c;
  NET_cinfo *result = &default_cinfo_type;
  XP_Bool saw_encoding = FALSE;

  c = PL_strrchr (uri, '/');
  if (c) start = c;

  end = start + PL_strlen (start);
  c = PL_strrchr (start, '?');
  if (c) end = c;
  c = PL_strrchr (start, '#');
  if (c && c < end) end = c;

AGAIN:
  for (c = end - 1; c >= uri; c--)
	if (*c == '/' || *c == '.')
	  break;

  if (c >= uri && *c == '.')
	{
	  int32 i;
	  XP_List *list_ptr = NET_ciMasterList;
	  NET_cdataStruct *cdata;
	  uint32 ext_len;
	  start = c + 1;
	  ext_len = end - start;
	  while ((cdata = (NET_cdataStruct *) XP_ListNextObject (list_ptr)) != NULL)
		for (i = cdata->num_exts - 1; i >= 0; i--)
		  if (PL_strlen (cdata->exts[i]) == ext_len &&
			  !PL_strncasecmp (start, cdata->exts [i], ext_len))
			{
			  if (type_p && cdata->ci.type)
				{
				  /* We are looking for a type, and this extension represents
					 one.  Since this is the last one in the path (since we
					 start from the end and go backwards) we're done now.
				   */
				  return &cdata->ci;
				}
			  else if (type_p && cdata->ci.encoding)
				{
				  /* We are looking for a type, and this extension represents
					 an encoding.  Skip over it and look for the next
					 extension earlier in the file name (as that may turn out
					 to be a type which we should take instead.)

					 But, if this is the second encoding we've seen, then the
					 final content-type is irrelevant, because we have no
					 mechanism for decoding multiple encodings.  So, while
					 "foo.gif.gz" gets type gif, encoding gz, "foo.tar.gz.uu"
					 gets type unknown, encoding uu.  This is especially
					 important for the types that we put on outgoing MIME
					 attachments.
				   */
				  if (saw_encoding)
					return &default_cinfo_type;
				  saw_encoding = TRUE;
				  end = start - 1;
				  goto AGAIN;
				}
			  else if (cdata->ci.type)
				{
				  PR_ASSERT (!type_p);
				  /* We are looking for an encoding, and this extension
					 represents a content-type.  That means that there are
					 no encodings after the type in this file name, and
					 therefore it has no encoding (an encoding which occurs
					 before a type doesn't count: "foo.uu.gif" should be of
					 type gif with no encoding, not encoding uu. Also,
					 "foo.uu.unknown" should be of an unknown type with no
					 encoding; and "foo.uu.gif.Z" should be type gif,
					 encoding compress.)
				   */
				  return &default_cinfo_type;
				}
			  else if (cdata->ci.encoding)
				{
				  PR_ASSERT (!type_p);
				  /* We are looking for an encoding, and this extension
					 represents one.  It is the last one in the file name,
					 so now we're done.  ("foo.gz.uu" gets encoding "uu"
					 instead of "gz".)
				   */
				  return &cdata->ci;
				}
			  /* Else, this cdata is junk, or something...  Skip it. */
			}
	}

  return (result);
}


PUBLIC NET_cinfo *
NET_cinfo_find_type (char *uri)
{
  return net_cinfo_find_type_or_encoding (uri, TRUE);
}

PUBLIC NET_cinfo *
NET_cinfo_find_enc (char *uri)
{
  return net_cinfo_find_type_or_encoding (uri, FALSE);
}


#if defined(DEBUG) && defined(XP_UNIX)
void
print_mime_types()
{
  int32 i;
  XP_List *list_ptr = NET_ciMasterList;
  NET_cdataStruct *cdata;

  fprintf(stderr, "\n---------\n");

  while ((cdata = (NET_cdataStruct *) XP_ListNextObject (list_ptr)))
	{
	  fprintf (stderr, "%s%s%s (%s; %s):",
			   (cdata->ci.type ? cdata->ci.type : ""),
			   (cdata->ci.type && *cdata->ci.type &&
				cdata->ci.encoding && *cdata->ci.encoding
				? "+" : ""),
			   (cdata->ci.encoding ? cdata->ci.encoding : ""),
			   (cdata->ci.desc ? cdata->ci.desc : "no desc"),
			   (cdata->ci.icon ? cdata->ci.icon : "no icon"));

	  for (i = cdata->num_exts - 1; i >= 0; i--)
		fprintf (stderr, " %s", cdata->exts[i]);
	  fprintf (stderr, "\n");
	}
}
#endif /* DEBUG && XP_UNIX */


/* return a cdataStruct from a mime_type
 */
PRIVATE NET_cdataStruct *
NET_cinfo_find_cdata_by_type(char *mime_type)
{
  NET_cdataStruct *cdata;
  XP_List * list_ptr;

  if(!mime_type)
	return(NULL);

  list_ptr = NET_ciMasterList;
  while((cdata = (NET_cdataStruct *) XP_ListNextObject(list_ptr)) != NULL)
	{
	  if(cdata->ci.type && !PL_strcasecmp(mime_type, cdata->ci.type))
		return(cdata);
	}

  return(NULL);
}

/* return a cinfo from a mime_type
 */
PUBLIC NET_cinfo *
NET_cinfo_find_info_by_type(char *mime_type)
{
  NET_cdataStruct *cdata = NET_cinfo_find_cdata_by_type(mime_type);

  if(cdata)
	return(&cdata->ci);
  else
	return(NULL);
}


/* --------------------------- cinfo_find_ext ------------------------------ */

/* returns an extension (without the ".") for
 * a given mime type.
 *
 * If no match is found NULL is returned.
 *
 * Do not free the returned char *
 */
PUBLIC char *
NET_cinfo_find_ext(char *mime_type)
{
  NET_cdataStruct *cdata = NET_cinfo_find_cdata_by_type(mime_type);

  if(cdata)
	return(cdata->exts[0]);
  else
	return(NULL);
}


/* ----------------------------- File parsers ----------------------------- */


#if 0
static void
net_build_string(char** curr_str, char *new_str)
{
  int len;

  if ( new_str && *new_str )
	{
	  if (!*curr_str )
		{
		  len = strlen(new_str);
		  *curr_str = (char *)PR_Malloc((len+1)*sizeof(char));
		  sprintf(*curr_str, "%s", new_str);
		}
	  else
		{
		  len = strlen(new_str) + strlen(*curr_str);
		  *curr_str = (char *)PR_Malloc((len+1)*sizeof(char));
		  sprintf(*curr_str, "%s%s", *curr_str, new_str);
		}
	}
}
#endif /* 0 */

/* Parse old NCSA mime.types files */

int
_cinfo_parse_mimetypes(XP_File fp, char *t, Bool is_local)
{
  char *ext, *end;
  NET_cdataStruct *cd;
  char *src_string = 0;

  /* t is already filled. */
  while(1) {

	if ( src_string )
	  {
		src_string = XP_AppendStr(src_string, t);
	  }
	else
	  {
		StrAllocCopy(src_string, t);
	  }

	while((*t) && (NET_IS_SPACE(*t)))
	  ++t;

	if(*t && (*t != '#')) {
	  ext = t;
	  while((*ext) && (!NET_IS_SPACE(*ext))) ++ext;
	  if((*ext))
		*ext++ = '\0';

	  cd = NULL;

	  while((*ext)) {
		while((*ext) && (NET_IS_SPACE(*ext))) ++ext;

		if((*ext)) {
		  if(!cd) {
			cd = NET_cdataCreate();
			if(!cd)
			  return(-1);
		  }

		  cd->is_local = is_local;
		  cd->is_external = FALSE; /* don't count these as external types */

		  cd->is_modified = FALSE; /* default is not modified in any way */
		  end = ext+1;
		  while((*end) && (!NET_IS_SPACE(*end))) ++end;
		  if(*end)
			*end++ = '\0';
		  net_cdata_new_ext(ext, cd);
		  ext = end;
		}
	  }
	  if(cd) {
		NET_cdataStruct *old_cd = NULL;
		PR_FREEIF(cd->ci.type);
        cd->ci.type = 0;
		StrAllocCopy(cd->ci.type, t);
		PR_FREEIF(cd->src_string);
		StrAllocCopy(cd->src_string, src_string);

		if ( (old_cd =  NET_cdataExist(cd)) != NULL)
		  /* Found an old one, remove it first */
		  {
			/* Extra hack: if the old one defined an icon, or a desc,
			   and the new one does not, carry those over. */
			char *icon = (old_cd->ci.icon && !cd->ci.icon
						  ? PL_strdup(old_cd->ci.icon)
						  : 0);
			char *desc = (old_cd->ci.desc && !cd->ci.desc
						  ? PL_strdup(old_cd->ci.desc)
						  : 0);

			NET_cdataRemove(old_cd);
			old_cd = 0;

			if (icon) cd->ci.icon = icon;
			if (desc) cd->ci.desc = desc;
		  }
		/* Not an existing type, we add a new one */
		NET_cdataAdd(cd);
		PR_Free(src_string);
		src_string = 0;
	  }
	}
	if(!(NET_XP_FileReadLine(t, CINFO_MAX_LEN, fp)))
	  break;
  }
  return 0;
}


PUBLIC int
cinfo_parse_mcc_line(char *line, Bool is_external, Bool is_local,
					 char **src_string)
{
  int ln = 1;
  char *t, *u, *v, *name, *value, c;
  NET_cdataStruct *cd;

  if(line && *line)
	{
	  cd = NULL;

	  t = line;

	  if((*t) && (*t != '#')) {
		while(1) {

		  while(*t && NET_IS_SPACE(*t)) ++t;
		  if(!(*t))
			break;
		  name = t;

		  while(*t && (*t != '=')) ++t;
		  if(!(*t)) {
			TRACEMSG(("line %d: name without =", ln));
			if(cd)
			  NET_cdataFree(cd);
			return (-1);
		  }
		  for(u = t - 1; NET_IS_SPACE(*u); --u)
		    ;
		  *(u+1) = '\0';   /* terminate name */
		  ++t;
		  while(*t && (*t != '\"') && (NET_IS_SPACE(*t))) ++t;
		  if(!(*t)) {
			TRACEMSG(("line %d: empty value", ln));
			if(cd)
			  NET_cdataFree(cd);
			return (-1);
		  }
		  if(*t == '\"') {
			value = ++t;
			while(*t && (*t != '\"')) ++t;
			if(!(*t)) {
			  TRACEMSG(("line %d: missing closing quotes", ln));
			  if(cd)
				NET_cdataFree(cd);
			  return (-1);
			}
		  }
		  else {
			value = t;
			while(*t && (!NET_IS_SPACE(*t))) ++t;
			/* null t is okay */
		  }
		  if(*t) *t++ = '\0';
		  if(!cd) {
			cd = NET_cdataCreate();
			if(!cd)
			  return(0);

			cd->is_external = is_external;
	        cd->is_modified = FALSE; /* default is "not modified yet!*/
			cd->is_local = is_local;
			PR_FREEIF(cd->src_string);
			cd->src_string = *src_string;
			*src_string = 0;
		  }

		  if(!PL_strcasecmp(name, "type")) {
		    PR_FREEIF(cd->ci.type);
			cd->ci.type = 0;
			StrAllocCopy(cd->ci.type, value);

		  } else if(!PL_strcasecmp(name, "enc")) {
			PR_FREEIF(cd->ci.encoding);
            cd->ci.encoding = 0;
			StrAllocCopy(cd->ci.encoding, value);

		  } else if(!PL_strcasecmp(name, "lang")) {
			PR_FREEIF(cd->ci.language);
            cd->ci.language = 0;
			StrAllocCopy(cd->ci.language, value);

		  } else if(!PL_strcasecmp(name, "desc")) {
			PR_FREEIF(cd->ci.desc);
            cd->ci.desc = 0;
			StrAllocCopy(cd->ci.desc, value);

		  } else if(!PL_strcasecmp(name, "alt")) {
			PR_FREEIF(cd->ci.alt_text);
            cd->ci.alt_text = 0;
			StrAllocCopy(cd->ci.alt_text, value);

		  } else if(!PL_strcasecmp(name, "icon")) {
			PR_FREEIF(cd->ci.icon);
            cd->ci.icon = 0;
			StrAllocCopy(cd->ci.icon, value);

		  } else if(!PL_strcasecmp(name, "exts")) {
			u = value;
			while(1) {
			  v = u;
			  while(*u && (*u != ',')) ++u;
			  c = *u;
			  *u = '\0';
			  net_cdata_new_ext(v, cd);
			  if(c)
				++u;
			  else
				break;
			}
		  }
		}

		/* Some people like to be clever and use mime.types entries
		   like "type=application/x-tar  enc=x-gzip  exts=tgz",
		   but unfortunately, that doesn't even come close to working,
		   and will override (delete) the existing tar entry, causing
		   disaster of various forms.

		   So, if this line defines *both* "type" and "enc", just throw
		   it away.
		 */
		if (cd &&
			cd->ci.type && *cd->ci.type &&
			cd->ci.encoding && *cd->ci.encoding)
		  {
			NET_cdataFree(cd);
			cd = 0;
		  }

		if(cd)
		  {
			NET_cdataStruct	*old_cd = NULL;

			if ((old_cd =  NET_cdataExist(cd)) != NULL)
			  /* Found an old one, remove it first */
			  {
				/* Extra hack: if the old one defined an icon, or a desc,
				   and the new one does not, carry those over. */
				char *icon = (old_cd->ci.icon && !cd->ci.icon
							  ? PL_strdup(old_cd->ci.icon)
							  : 0);
				char *desc = (old_cd->ci.desc && !cd->ci.desc
							  ? PL_strdup(old_cd->ci.desc)
							  : 0);

				NET_cdataRemove(old_cd);
				old_cd = 0;

				if (icon) cd->ci.icon = icon;
				if (desc) cd->ci.desc = desc;
			  }
			NET_cdataAdd(cd);
		  }
	  }

	  ln++;
	}
  return 0; /* #### what should return value be? */
}


/* Parse sexy new MCC mime types files */
int
_cinfo_parse_mcc(XP_File fp, char *line, Bool is_local)
{
  char *end;
  char *src_string = 0;

  TRACEMSG(("mime.types file is MCC type"));

  /* t is already filled. */

  if ( !src_string )
	StrAllocCopy(src_string, line);
  else
	{
	  src_string = XP_AppendStr(src_string, line);
	}
  while(1) {
	cinfo_parse_mcc_line(line, TRUE, is_local, &src_string);

	if(!(NET_XP_FileReadLine(line, CINFO_MAX_LEN, fp)))
	  break;

	if ( !src_string )
	  StrAllocCopy(src_string, line);
	else
	  {
		src_string = XP_AppendStr(src_string, line);
	  }
	/* assemble extended lines
	 */
	end = &line[PL_strlen(line)-1];
	while (end > line && (*end == CR || *end == LF))
	  end--;
	while(*end == '\\')
	  {
        *end = ' ';
		end++;

		if(!(NET_XP_FileReadLine(end, CINFO_MAX_LEN-(end-line), fp)))
		  break;

		if ( !src_string )
		  StrAllocCopy(src_string, end);
		else
		  src_string = XP_AppendStr(src_string, end);


		end = &end[PL_strlen(end)-1];
		while (end > line && (*end == CR || *end == LF))
		  end--;

		if((end-line)+2 >= CINFO_MAX_LEN)
		  break;
	  }
  }

  return 0;
}

/* return the master cdata List pointer
 */
PUBLIC XP_List *
cinfo_MasterListPointer()
{
  return(NET_ciMasterList);
}

/* fills in cdata structure
 *
 * if cdata for a given type do not exist, create a new one
 */
PUBLIC void
NET_cdataCommit(char * mimeType, char * cdataString)
{
  XP_List * extensionList;
  NET_cdataStruct * matchInfo = NULL;
  int listSize;
  int i;

  extensionList = cinfo_MasterListPointer();
  if (!extensionList)
   	{
	  NET_ciMasterList = XP_ListNew();
	  extensionList = cinfo_MasterListPointer();  /* Lou help me out here,
													 create the list and
													 matchInfo */
    }
  if (!extensionList)
	return;

  listSize = XP_ListCount(extensionList);
  for (i = 1; i<=listSize; i++)
	{
	  NET_cdataStruct * cinfo =
		(NET_cdataStruct *)XP_ListGetObjectNum(extensionList, i);
	  if (cinfo &&
		  cinfo->ci.type &&
		  (PL_strcasecmp(mimeType, cinfo->ci.type) == 0))
        {
		  int j;
		  matchInfo = cinfo;
		  for ( j= 0; j< matchInfo->num_exts; j++)  /* Free 'em all */
			PR_Free((char*)matchInfo->exts[j]);
		  PR_Free(matchInfo->exts);
		  matchInfo->exts = NULL;
		  break;
		}
	}

  if (matchInfo == NULL)
	{
	  matchInfo = NET_cdataCreate();
	  if (matchInfo == NULL)
		{
		  return;
		}
	  else
		{
		  PR_FREEIF(matchInfo->ci.type);
		  matchInfo->ci.type = 0;
		  StrAllocCopy(matchInfo->ci.type, mimeType);
		}
	  NET_cdataAdd(matchInfo);

	}
  /* Found the right type */
  {
	char newExtension[30]; /* extension limit is 30 chars */
	char * citer;
	int newExIndex = 0;
	memset(newExtension, 0, sizeof(newExtension));
	citer = cdataString;

	matchInfo->num_exts = 0;
	matchInfo->exts = (char**)PR_Malloc(10);

	if (matchInfo->exts == NULL)
	  return;
	while (*citer != 0)    /* Parse the string. Skip all non-c*/
	  {
		if (isalnum(*citer) && (newExIndex < 29)) /* NET_IS_ALPHA(*citer) */
		  newExtension[newExIndex++] = *citer;
		else if (PL_strlen(newExtension) > 0)
		  {
			char * newString = NULL;
			newExtension[newExIndex] = 0;
			StrAllocCopy(newString, newExtension);
			matchInfo->num_exts++;
			matchInfo->exts =
			  (char**)PR_Realloc(matchInfo->exts,
								 matchInfo->num_exts * sizeof(void*));
			matchInfo->exts[matchInfo->num_exts-1] = newString;
			newExIndex = 0;
			newExtension[0] = 0;
		  }
		citer++;
	  }
	/* commit the last extension, if there was one */
	if (PL_strlen(newExtension) > 0)
	  {
		char * newString = NULL;
		newExtension[newExIndex] = 0;
		StrAllocCopy(newString, newExtension);
		matchInfo->num_exts++;
		matchInfo->exts =
		  (char**)PR_Realloc(matchInfo->exts,
							 matchInfo->num_exts * sizeof(void*));
		matchInfo->exts[matchInfo->num_exts-1] = newString;
	  }
  }
}


PUBLIC XP_Bool
NET_IsOldMimeTypes (XP_List *masterList)
{
  NET_cdataStruct *cd;

  while((cd = XP_ListNextObject(masterList)) != NULL)
	{
	  if ( cd->is_local && !cd->is_external )
		{ /* This means that this is a old mime types */
		  return TRUE;
		}
	}
  return FALSE;
}

#ifndef XP_UNIX
static void
net_Real_CleanupFileFormat(char *filename, Bool file_not_open)
{
  NET_cdataStruct * cdata;
  XP_File fp = 0;
  int len = 0;
  XP_Bool failed = FALSE;
  char buffer[512];
  int i;

  if(!NET_ciMasterList)
	return;

  /* Need to remove from the end so that the next time
	 around, the file will be loaded up into a list in
	 the consistent way */
  while(!failed && ((cdata = (NET_cdataStruct *)XP_ListRemoveEndObject(NET_ciMasterList)) != NULL))
	{

	  if(file_not_open &&
		 (cdata->is_external|| cdata->is_modified))
		{
		  /* open file
		   * if it doesn't open we won't try again
		   */
		  fp = NET_XP_FileOpen(filename? filename : "mime.types",
						   xpMimeTypes, XP_FILE_WRITE);
		  TRACEMSG(("Writing mime types file..."));
		  file_not_open = FALSE;

		  if(fp)
			{
			  PR_snprintf(buffer, sizeof(buffer), "#\
--Netscape Communications Corporation MIME Information\n#\
Do not delete the above line. It is used to identify the file type.\n#\n");
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}
		}

	  /* Save out the modfied entries as well */
	  /* cdata might be modified in FE */

	  if((cdata->is_external||cdata->is_modified) && fp)
		{
		  if(!failed && cdata->ci.type)
			{
			  PR_snprintf(buffer, sizeof(buffer), "type=%.500s	",
						  cdata->ci.type);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  if(!failed && cdata->ci.encoding)
			{
			  PR_snprintf(buffer, sizeof(buffer), "enc=%.500s	",
						  cdata->ci.encoding);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  if(!failed && (cdata->num_exts > 0))
			{
			  PR_snprintf(buffer, sizeof(buffer), "exts=%.500s",
						  cdata->exts[0]);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;

			  for(i=1; !failed, i<cdata->num_exts; i++)
				{
				  PR_snprintf(buffer, sizeof(buffer), ",%.500s",
							  cdata->exts[i]);
				  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
				  if (len != (int) strlen(buffer))
					failed = TRUE;
				}

			  PR_snprintf(buffer, sizeof(buffer), "	");
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  if(!failed && cdata->ci.icon)
			{
			  PR_snprintf(buffer, sizeof(buffer), "icon=\"%.500s\"	",
						  cdata->ci.icon);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  if(!failed && cdata->ci.desc)
			{
			  PR_snprintf(buffer, sizeof(buffer), "desc=\"%.500s\"	",
						  cdata->ci.desc);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  if(!failed && cdata->ci.alt_text)
			{
			  PR_snprintf(buffer, sizeof(buffer), "alt=\"%.500s\"	",
						  cdata->ci.alt_text);
			  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
			  if (len != (int) strlen(buffer))
				failed = TRUE;
			}

		  PR_snprintf(buffer, sizeof(buffer), "%c", LF);
		  len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
		  if (len != (int) strlen(buffer))
			failed = TRUE;
		}

	  /* free the data
	   */
	  NET_cdataFree(cdata);
	}

  XP_ListDestroy(NET_ciMasterList);
  NET_ciMasterList = 0;

  if(!file_not_open && fp)
	NET_XP_FileClose(fp);
  /* This function should really return a return code, and handle any errors */

}
#endif /* XP_UNIX */

#ifdef XP_UNIX
PUBLIC void
NET_CleanupFileFormat(char *filename)
{
	NET_cdataStruct *cdata;
	XP_File fp = 0;
	int len = 0;
	XP_Bool failed = FALSE;
	Bool file_open = FALSE;
	XP_List *modifiedList = NULL;
	char buffer[512];
	Bool first = TRUE;

	if ( !NET_ciMasterList ) return;

	if ( filename )
	{
		fp = NET_XP_FileOpen(filename, xpMimeTypes, XP_FILE_WRITE);
		/* Need to remove from the end so that the next time
		around, the file will be loaded up into a list in
		the consistent way */

		if ( fp )
		{
			file_open = TRUE;
			modifiedList = XP_ListNew();
			while((cdata = (NET_cdataStruct *) XP_ListRemoveEndObject(NET_ciMasterList)))
			{
				if ( cdata->is_local )
				{
					if ( first && !NET_IsOldMimeTypes(NET_ciMasterList) )
					{
						first = FALSE;
						if (cdata->src_string && *cdata->src_string &&
							strncmp(cdata->src_string, MCC_MT_MAGIC,
							MCC_MT_MAGIC_LEN) &&
							strncmp(cdata->src_string, NCC_MT_MAGIC,
							NCC_MT_MAGIC_LEN))
						{
							/* Write out standard new mime type header info */
							PR_snprintf(buffer, sizeof(buffer),
							"#\
--Netscape Communications Corporation MIME Information\n#\
Do not delete the above line. It is used to identify the file type.\n#\n");
							len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
							if (len != (int) strlen(buffer))
								failed = TRUE;
						}
					}
					if (!failed && cdata->src_string && *cdata->src_string )
					{
						len = NET_XP_FileWrite(cdata->src_string,
							strlen(cdata->src_string), fp);

						if (cdata->src_string[strlen(cdata->src_string)-1] != '\n')
							len = NET_XP_FileWrite("\n", strlen("\n"), fp);
					}
					NET_cdataFree(cdata);

				}
				else if ( cdata->is_modified )
					XP_ListAddObject(modifiedList, cdata);
				else
					NET_cdataFree(cdata);

			}

			while (!failed && (cdata = (NET_cdataStruct *) XP_ListRemoveEndObject(modifiedList)))
			{

				if ( first && !NET_IsOldMimeTypes(NET_ciMasterList) )
				{
					first = FALSE;
					if (cdata->src_string && *cdata->src_string &&
						strncmp(cdata->src_string, MCC_MT_MAGIC,
						MCC_MT_MAGIC_LEN) &&
						strncmp(cdata->src_string, NCC_MT_MAGIC,
						NCC_MT_MAGIC_LEN))
					{
						/* Write out standard new mime type header info */
						PR_snprintf(buffer, sizeof(buffer),
						"#\
--Netscape Communications Corporation MIME Information\n#\
Do not delete the above line. It is used to identify the file type.\n#\n");
						len = NET_XP_FileWrite(buffer, strlen(buffer), fp);
						if (len != (int) strlen(buffer))
							failed = TRUE;

					}
				}

				if (!failed && cdata->src_string && *cdata->src_string )
				{
					len = NET_XP_FileWrite(cdata->src_string,
						strlen(cdata->src_string), fp);
					if (len != (int) strlen(cdata->src_string))
						failed = TRUE;
					if ( cdata->src_string[strlen(cdata->src_string)-1] != '\n')
					{
						len = NET_XP_FileWrite("\n", strlen("\n"), fp);
						if (len != (int) strlen("\n"))
							failed = TRUE;
					}
				}

				NET_cdataFree(cdata);
			}
			NET_XP_FileClose(fp);
		}
	}
	else /* Clean up each node before deleting the root node */
	{
		while((cdata = (NET_cdataStruct *) XP_ListRemoveEndObject(NET_ciMasterList)))
			NET_cdataFree(cdata);
	}
	XP_ListDestroy(NET_ciMasterList);
	NET_ciMasterList = 0;
	/* This function should really return an error code and deal with any errors when they happen */
}

#else /* !XP_UNIX */

PUBLIC void
NET_CleanupFileFormat()
{
  Bool file_not_open=TRUE;
  net_Real_CleanupFileFormat(NULL, file_not_open);
}

#endif /* !XP_UNIX */
