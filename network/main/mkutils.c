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
 *
 * Implemented by Lou Montulli '94-'98
 *
 * this is the dumping grounds for random Netlib functions
 * Home for utility functions and anything else that 
 * doesn't fit elsewhere.
 */
#if defined(CookieManagement)
/* #define TRUST_LABELS 1 */
#endif

#include "rosetta.h"
#include "mkutils.h"
#include "net_xp_file.h"
#include "mkprefs.h"
#include "gui.h"
#include "mkparse.h"
#include "mkgeturl.h"
#include "cookies.h"
#include "httpauth.h"
#include "libi18n.h"
#include "msgcom.h"
#include "mime.h"
#include "secrng.h"
#include HG38763
#include "prefapi.h"
#include "secnav.h"
#include "preenc.h"
#include "mkselect.h"
#include "prerror.h"
#include "netcache.h"
#include "mktcp.h"
#include "netutils.h"

#include "xpgetstr.h"

#include "mimeenc.h"
#include "intl_csi.h"

#ifdef XP_MAC
#include "MacBinSupport.h"
#endif

#ifdef TRUST_LABELS
#include "mkaccess.h"
#include "pics.h"
#endif

#ifdef NU_CACHE
#include "CacheStubs.h"
#endif

typedef struct {
  char *buffer;
  int32 size;
  int32 pos;
} BufferStruct;

#ifndef MAX
#define MAX(x, y)   (((x) > (y)) ? (x) : (y))
#endif

static int
net_buffer_output_fn ( const char *buf, int32 size, void *closure);

extern int MK_OUT_OF_MEMORY;
extern int MK_UNABLE_TO_LOCATE_FILE;
extern int XP_ERRNO_EWOULDBLOCK;
extern int MK_TCP_WRITE_ERROR;
extern int XP_ERRNO_EALREADY;
extern int XP_ALERT_URN_USEHTTP;
extern int XP_ALERT_NFS_USEHTTP;
extern int MK_NO_WAIS_PROXY;

/* print network progress to the front end
 */
MODULE_PRIVATE void
NET_Progress(MWContext *context, char *msg)
{
    FE_Progress(context, msg);
}

/* note:  on the Macintosh local_dir_name will be in the following format: */
/*              file:///Hard%20Disk/Folder%20Name/File.html                */
PUBLIC char **
NET_AssembleAllFilesInDirectory(MWContext *context, char * local_dir_name)
{
    PRDir *dir_ptr;
    PRDirEntry *dir_entry;
    XP_StatStruct stat_entry;
    char **files_to_post;
    char *file_to_post = 0;
    int32 i, cur_array_size;
    int end;
    Bool have_slash;
#define INITIAL_ARRAY_SIZE 10
#define EXPAND_ARRAY_BY 5

    PR_ASSERT(local_dir_name);

#ifdef XP_MAC
    local_dir_name += 7;    // chop-off "file://"
#endif

    if(NULL == (dir_ptr = PR_OpenDir(local_dir_name)))
      {
        FE_Alert(context, "Unable to open local directory");
        return NULL;
      }

    /* make sure local_dir_name doesn't have a slash at the end */
    end = PL_strlen(local_dir_name)-1;
    have_slash = (local_dir_name[end] == '/') || (local_dir_name[end] == '\\');

    files_to_post = (char**) PR_Malloc(INITIAL_ARRAY_SIZE * sizeof(char*));
    if(!files_to_post)
        return NULL;
    cur_array_size = INITIAL_ARRAY_SIZE;

    i=0;
    while((dir_entry = PR_ReadDir(dir_ptr, PR_SKIP_BOTH)) != NULL)
      {
        /* assemble full pathname first so we can test if its a directory */
        file_to_post = PL_strdup(local_dir_name);
        if ( ! file_to_post ){
            PR_CloseDir(dir_ptr);
            return NULL;
        }

        /* Append slash to directory if we don't already have one */
        if( !have_slash )
          {
#ifdef XP_WIN
            StrAllocCat(file_to_post, "\\");
#else
            StrAllocCat(file_to_post, "/");
#endif
          }
        if ( ! file_to_post )
          {
            PR_CloseDir(dir_ptr);
            return NULL;
          }

        StrAllocCat(file_to_post, dir_entry->name);

        if ( ! file_to_post )
          {
            PR_CloseDir(dir_ptr);
            return NULL;
          }

        /* skip over subdirectory names */
        if(-1 != NET_XP_Stat(file_to_post, &stat_entry, xpFileToPost) && S_ISDIR(stat_entry.st_mode) )
          {
            PR_Free(file_to_post);
            continue;
          }

        /* expand array if necessary 
         * always leave room for the NULL terminator */
        if(i >= cur_array_size-1)
          {
            files_to_post = (char**) PR_Realloc(files_to_post, (cur_array_size + EXPAND_ARRAY_BY) * sizeof(char*)); 
            if(!files_to_post) {
                PR_CloseDir(dir_ptr);
                return NULL;
            }
            cur_array_size += EXPAND_ARRAY_BY;
          }

        files_to_post[i++] = PL_strdup(file_to_post);

        PR_Free(file_to_post);
       }

    PR_CloseDir(dir_ptr);

    /* NULL terminate the array, space is guarenteed above */
    files_to_post[i] = NULL;

    return(files_to_post);
}

/********KILL this, use NET_PublishFilesTo ************/
/* upload a set of local files (array of char*)
 * all files must have full path name
 * first file is primary html document,
 * others are included images or other files
 * in the same directory as main file
 */
PUBLIC void
NET_PublishFiles(MWContext *context, 
                 char **files_to_publish,
                 char *remote_directory)
{
    URL_Struct *URL_s;     

    PR_ASSERT(context && files_to_publish && remote_directory);
    if(!context || !files_to_publish || !*files_to_publish || !remote_directory)
        return;

    /* create a URL struct */
    URL_s = NET_CreateURLStruct(remote_directory, NET_SUPER_RELOAD);
    if(!URL_s)
      {    
        FE_Alert(context, "Error: not enough memory for file upload");
        return;  /* does not exist */
      }

    FREE_AND_CLEAR(URL_s->content_type);

    /* add the files that we are posting and set the method to POST */
    URL_s->files_to_post = files_to_publish;
    URL_s->method = URL_POST_METHOD;
    
    /* start the load */
    FE_GetURL(context, URL_s);
}

/* upload a set of local files (array of char*)
 * first file is primary html document,
 * others are included images or other files. 
 *
 * It is legal to pass in NULL as the value of publish_to.  This will duplicate
    the functionality of the old NET_PublishFiles. 
 * files_to_publish and publish_to are used by and will be freed by NET_PublishFilesTo, 
 * base_url is copied */
PUBLIC void
NET_PublishFilesTo(MWContext *context, 
                 char **files_to_publish,
                 char **publish_to,  /* Absolute URLs of the location to 
                                               * publish the files to. Used only if HTTP.  
                                               * Ignored if FTP (except to delete memory.)*/
                 XP_Bool *add_crlf, /* For each file in files_to_publish, should every line 
                                       end in a CRLF. */
                 char *base_url, /* Directory to publish to, or the destination 
                                           * URL of the root HTML document. */
                 char *username,
                 char *password,
                 Net_GetUrlExitFunc *exit_func,
                 void *fe_data)
{
  URL_Struct *URL_s;       

  if(!context || !files_to_publish || !*files_to_publish || !base_url) {
    PR_ASSERT(0);
    return;
  }
    
    /* create a URL struct */
  URL_s = NET_CreateURLStruct(base_url, NET_SUPER_RELOAD);
    if(!URL_s)
  {    
    FE_Alert(context, "Error: not enough memory for file upload");
    return;  /* does not exist */
  }

  if (username)
    URL_s->username = PL_strdup(username);
  if (password)
    URL_s->password = PL_strdup(password);

  FREE_AND_CLEAR(URL_s->content_type);

    /* add the files that we are posting and set the method to POST */
  URL_s->files_to_post = files_to_publish;
  URL_s->post_to = publish_to;
  URL_s->add_crlf = add_crlf;
  URL_s->method = URL_POST_METHOD;
  URL_s->pre_exit_fn = exit_func; /* May be NULL */
  URL_s->fe_data = fe_data;
   
    /* start the load */
  FE_GetURL(context, URL_s);
}

/* assemble username, password, and ftp:// or http:// URL into
 * ftp://user:password@/blah  format for uploading
*/
PUBLIC Bool
NET_MakeUploadURL( char **full_location, char *location, 
                   char *user_name, char *password )
{
    char *start;
    char *pSrc;
    char *destination;
    char *at_ptr;
    int iSize;

    if( !full_location || !location ) return FALSE;
    if( *full_location ) PR_Free(*full_location);

    iSize = strlen(location) + 4;
    if( user_name ) iSize += strlen(user_name);
    if( password ) iSize += strlen(password);

    *full_location = (char*)PR_Malloc(iSize);
    if( !*full_location ){
        /* Return an empty string */
        *full_location = strdup("");
        return FALSE;
    }
    **full_location = '\0';

    /* Find start just past http:// or ftp:// */
    start = PL_strstr(location, "//");
    if( !start ) return FALSE;

    /* Point to just past the host part */
    start += 2;
    pSrc = location;
    destination = *full_location;
    /* Copy up to and including "//" */
    while( pSrc < start ) *destination++ = *pSrc++;
    *destination = '\0';

    /* Skip over any user:password in supplied location */
    at_ptr = PL_strchr(start, '@');
    if( at_ptr ){
        start = at_ptr + 1;
    }
    /* Append supplied "user:password@"
     * (This can be used without password)
    */
    if( user_name && PL_strlen(user_name) > 0 ){
        PL_strcat(*full_location, user_name);
        if ( password  && PL_strlen(password) > 0 ){
            PL_strcat(*full_location,":");
            PL_strcat(*full_location, password);
        }
        PL_strcat(*full_location, "@");
    }
    /* Append the rest of location */
    PL_strcat(*full_location, start);

    return TRUE;
}

/* extract the username, password, and reassembled location string 
 * from an ftp:// or http:// URL
*/
PUBLIC Bool
NET_ParseUploadURL( char *full_location, char **location, 
                    char **user_name, char **password )
{
    char *unamePwd;
    char *username = NULL;
    char *password_ = NULL;
    char *colon;

    if( full_location == NULL || location == NULL )
        return FALSE;

    /* Empty exitisting strings... */   
    if(*location) PR_Free(*location);
    if( user_name && *user_name) PR_Free(*user_name);
    if( password && *password) PR_Free(*password);

    /* Separate the username and password from the full URL */    
    *location = NET_ParseURL(full_location, GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
    unamePwd = NET_ParseURL(full_location, GET_USERNAME_PART | GET_PASSWORD_PART);

    /* get the username & password out of the combo string */
    if( (colon = PL_strchr(unamePwd, ':')) != NULL )
    {
        *colon='\0';
        username = PL_strdup(unamePwd);
        password_ = PL_strdup(colon+1);

        *colon=':';
        PR_Free(unamePwd);
    } else {
        username=unamePwd;
    }
    if( user_name )
        *user_name = username;
    else
        PR_Free(username);
    
    if( password )
    {
        /* we always return at least an empty string */
        if( password_ )
            *password = password_;
        else
            *password = PL_strdup("");
    }
    else if( password_ )
        PR_Free(password_);
            
    return TRUE;
}


/* returns a malloc'd string containing a unique id 
 * generated from the sec random stuff.
 */
PUBLIC char * 
NET_GetUniqueIdString(void)
{
#define BYTES_OF_RANDOMNESS 20
    char rand_buf[BYTES_OF_RANDOMNESS+10]; 
    char *rv=0;


    RNG_GenerateGlobalRandomBytes(rand_buf, BYTES_OF_RANDOMNESS);

    /* now uuencode it so it goes across the wire right */
    rv = (char *) PR_Malloc((BYTES_OF_RANDOMNESS * (4/3)) + 10);
    
    if(rv)
        NET_UUEncode((unsigned char *)rand_buf, (unsigned char *)rv, BYTES_OF_RANDOMNESS);

    return(rv);

#undef BYTES_OF_RANDOMNESS
}


/* The magic set of 64 chars in the uuencoded data */
PRIVATE unsigned char uuset[] = {
'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+'
,'/' };

MODULE_PRIVATE int
NET_UUEncode(unsigned char *src, unsigned char *dst, int srclen)
{
   int  i, r;
   unsigned char *p;

/* To uuencode, we snip 8 bits from 3 bytes and store them as
6 bits in 4 bytes.   6*4 == 8*3 (get it?) and 6 bits per byte
yields nice clean bytes

It goes like this:
    AAAAAAAA BBBBBBBB CCCCCCCC
turns into the standard set of uuencode ascii chars indexed by numbers:
    00AAAAAA 00AABBBB 00BBBBCC 00CCCCCC

Snip-n-shift, snip-n-shift, etc....

*/

   for (p=dst,i=0; i < srclen; i += 3) {
        /* Do 3 bytes of src */
        register unsigned char b0, b1, b2;

        b0 = src[0];
        if (i==srclen-1)
            b1 = b2 = '\0';
        else if (i==srclen-2) {
            b1 = src[1];
            b2 = '\0';
        }
        else {
            b1 = src[1];
            b2 = src[2];
        }

        *p++ = uuset[b0>>2];
        *p++ = uuset[(((b0 & 0x03) << 4) | ((b1 & 0xf0) >> 4))];
        *p++ = uuset[(((b1 & 0x0f) << 2) | ((b2 & 0xc0) >> 6))];
        *p++ = uuset[b2 & 0x3f];
        src += 3;
   }
   *p = 0;      /* terminate the string */
   r = (unsigned char *)p - (unsigned char *)dst;       /* remember how many we
did */

   /* Always do 4-for-3, but if not round threesome, have to go
      clean up the last extra bytes */

   for( ; i != srclen; i--)
        *--p = '=';

   return r;
}


PRIVATE char *
NET_RemoveQuotes(char * string)
{
    char *rv;
    char *end;

    rv = XP_StripLine(string);

    if(*rv == '"' || *rv == '\'') 
        rv++;

    end = &rv[PL_strlen(rv)-1];

    if(*end == '"' || *end == '\'')
        *end = '\0';

    return(rv); }

#define POST_DATA_BUFFER_SIZE 2048

struct WritePostDataData {
    char    *buffer;
    XP_Bool  last_line_was_complete;
    int32    amt_in_buffer;
    int32    amt_sent;
    int32    total_amt_sent;
    int32    file_size;
    XP_File  fp;
    int32   headerSize;
    int32   headerAmountSent;
    XP_Bool CRSent;
    XP_Bool LFSent;
};

PUBLIC void
NET_free_write_post_data_object(struct WritePostDataData *obj)
{
    PR_ASSERT(obj);

    if(!obj)
        return;

    if (obj->fp)
        NET_XP_FileClose(obj->fp);

    FREEIF(obj->buffer);
    FREE(obj);
}

/* returns whether or not the socket error was expected. If it was not, all the write post data 
   object was free'd, and the url_s->error_msg was set */
PRIVATE XP_Bool
net_expected_error(URL_Struct *URL_s, struct WritePostDataData **data_obj)
{
    int err = PR_GetError();
    if(err == PR_WOULD_BLOCK_ERROR || err == PR_IS_CONNECTED_ERROR)
        return TRUE;
    URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR, err);
    NET_free_write_post_data_object(*data_obj);
    *data_obj = NULL;
    return FALSE;
}

/* this function is called repeatably to write
 * the post data body and headers onto the network.
 *
 * Returns negative on fatal error
 * Returns zero when done
 * Returns positive if not yet completed
 */
PUBLIC int
NET_WritePostData(MWContext  *context,
                  URL_Struct *URL_s,
                  PRFileDesc   *sock,
                  void      **write_post_data_data,
                  PRBool     add_crlf_to_line_endings)
{
    struct WritePostDataData *data_obj = (struct WritePostDataData *) 
                                                        *write_post_data_data;

    /* init the data object */
    if(!data_obj)
      {
        data_obj = PR_NEW(struct WritePostDataData);
        *write_post_data_data = data_obj;

        if(!data_obj)
            return(MK_OUT_OF_MEMORY);

        memset(data_obj, 0, sizeof(struct WritePostDataData));

        data_obj->last_line_was_complete = TRUE;
        data_obj->buffer = (char *) PR_Calloc(1, POST_DATA_BUFFER_SIZE);

        if(!data_obj->buffer)
        {
            NET_free_write_post_data_object(data_obj);
            *write_post_data_data=NULL;
            return 0;
        }

        if(URL_s->post_headers)
            data_obj->headerSize = PL_strlen(URL_s->post_headers);
        else
            data_obj->headerSize = 0;

        data_obj->headerAmountSent = 0;
        data_obj->CRSent = FALSE;
        data_obj->LFSent = FALSE;
        data_obj->amt_sent = 0;
      }

    if(!data_obj->fp && URL_s->post_data_is_file)
      {
        XP_StatStruct stat_entry;

        /* Send the headers */
        if( URL_s->post_headers && 
            (data_obj->headerSize > data_obj->headerAmountSent) )
        {
            int32 amtWritten = 0;
            amtWritten = PR_Write(sock, 
                    URL_s->post_headers + data_obj->headerAmountSent, 
                    data_obj->headerSize - data_obj->headerAmountSent);
            
            /* if there was a problem */
            if( amtWritten < 0 ) {
                /* determine whether it was expected */
                if( net_expected_error(URL_s, (struct WritePostDataData **) write_post_data_data) )
                    return 1;
                else 
                    return(MK_TCP_WRITE_ERROR);
            }

#ifdef DEBUG
            NET_NTrace("Tx: ", 4);
            NET_NTrace(URL_s->post_headers + data_obj->headerAmountSent, 
                           amtWritten);
#endif

            data_obj->headerAmountSent += amtWritten;

            if(data_obj->headerSize > data_obj->headerAmountSent)
                return 1;

        } /* END: URL_s->post_headers ... */

        /* stat the file to get the size
         */
        if(-1 != NET_XP_Stat(URL_s->post_data, &stat_entry, xpFileToPost))
          {
            data_obj->file_size = stat_entry.st_size;
          }
            
        /* open the post data file
         */
        data_obj->fp = NET_XP_FileOpen(URL_s->post_data, xpFileToPost, XP_FILE_READ_BIN);

        if(!data_obj->fp)
          {
            URL_s->error_msg = NET_ExplainErrorDetails(
                                                    MK_UNABLE_TO_LOCATE_FILE,
                                                    URL_s->post_data);
            NET_free_write_post_data_object(data_obj);
            *write_post_data_data = NULL;
            return(MK_UNABLE_TO_LOCATE_FILE);
          }
      }

    if(URL_s->post_data_is_file)
      {
        int32 amt_to_wrt;
        int32 amt_wrt;

        int type = NET_URL_Type (URL_s->address);
        XP_Bool quote_lines_p = (type == MAILTO_TYPE_URL ||
                                 type == NEWS_TYPE_URL);

        /* do file based operations
         */
        if(data_obj->amt_in_buffer < 1 || data_obj->amt_sent >= data_obj->amt_in_buffer)
          {
            /* read more data from the file
             */
            if (quote_lines_p || add_crlf_to_line_endings)
              {
                char *line;
                char *b = data_obj->buffer;
                int32 bsize = POST_DATA_BUFFER_SIZE;
                data_obj->amt_in_buffer =  0;
                do {
                  int L;

                  line = NET_XP_FileReadLine(b, bsize-5, data_obj->fp);

                  if (!line)
                    break;

                  L = PL_strlen(line);

                  /* escape periods only if quote_lines_p is set
                   */
                  if (quote_lines_p &&
                      data_obj->last_line_was_complete && line[0] == '.')
                    {
                      /* This line begins with "." so we need to quote it
                         by adding another "." to the beginning of the line.
                       */
                      int i;
                      line[L+1] = 0;
                      for (i = L; i > 0; i--)
                        line[i] = line[i-1];
                      L++;
                    }

                  /* set default */
                  data_obj->last_line_was_complete = TRUE;

                  if (L > 1 && line[L-2] == CR && line[L-1] == LF)
                    {
                        /* already ok */
                    }
                  else if(L > 0 && (line[L-1] == LF || line[L-1] == CR))
                    {
                      /* only add the crlf if required
                       * we still need to do all the
                       * if comparisons here to know
                       * if the line was complete
                       */
                      if(add_crlf_to_line_endings)
                        {
                          /* Change newline to CRLF. */
                          L--;
                          line[L++] = CR;
                          line[L++] = LF;
                          line[L] = 0;
                        }
                    }
                  else
                    {
                      data_obj->last_line_was_complete = FALSE;
                    }

                  bsize -= L;
                  b += L;
                  data_obj->amt_in_buffer += L;
                } while (line && bsize > 100);
              }
            else
              {
                data_obj->amt_in_buffer = NET_XP_FileRead(data_obj->buffer,
                                               POST_DATA_BUFFER_SIZE-1,
                                               data_obj->fp);
              }

HG29784
            
            if(data_obj->amt_in_buffer < 1)
              {
                /* end of file, all done
                 */
                /* 
                 * handled by NET_free_write_post_data_object 
                 *
                NET_XP_FileClose(data_obj->fp);
                */
                PR_ASSERT(data_obj->total_amt_sent >= data_obj->file_size);
                NET_free_write_post_data_object(data_obj);
                *write_post_data_data = NULL;
                return(0);
              }

            data_obj->amt_sent = 0;
          }

        amt_to_wrt = data_obj->amt_in_buffer-data_obj->amt_sent;

        /* write some data to the socket
         */
        amt_wrt = PR_Write(sock,
                           data_obj->buffer+data_obj->amt_sent,
                           amt_to_wrt);

#ifdef DEBUG
        NET_NTrace("Tx: ", 4);
        NET_NTrace(data_obj->buffer+data_obj->amt_sent,
                   amt_to_wrt);
#endif /* DEBUG */

        if(amt_wrt < 0) 
          {
            int err = PR_GetError();
            if(err == PR_WOULD_BLOCK_ERROR 
                || err == PR_IS_CONNECTED_ERROR)
                return(1); /* continue */

            URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_WRITE_ERROR, err);
            /*
             * handled by net_free_write_post_data
            NET_XP_FileClose(data_obj->fp);
            */

            NET_free_write_post_data_object(data_obj);
            *write_post_data_data = NULL;

            return(MK_TCP_WRITE_ERROR);
          }

#if defined(XP_UNIX) && defined(DEBUG)
        if(amt_wrt < amt_to_wrt)
           fprintf(stderr, "Fwrite wrote less than requested!\n");
#endif

        /* safety for broken write */
        if(amt_wrt > amt_to_wrt)
            amt_wrt = amt_to_wrt;

#if defined(XP_UNIX) && defined(DEBUG)
    if(MKLib_trace_flag)
      {
        fwrite("Tx: ", 1, 4, stderr);
        fwrite(data_obj->buffer+data_obj->amt_sent, 1, amt_wrt, stderr);
        fwrite("\n", 1, 1, stderr);
      }
#endif

        data_obj->amt_sent += amt_wrt;
        data_obj->total_amt_sent += amt_wrt;

        /* return the amount of data written
         * so that callers can provide progress
         * if necessary.  If the amt_wrt was
         * zero (I don't think it can ever happen)
         * return 1 because zero means all done
         */
        if(amt_wrt > 0)
            return(amt_wrt);
        else
            return(1);
      }
    else
      {
        /* do memory based operations */
        int32 amtWritten;

        /* send the headers if there are any && and they haven't been sent */
        if( URL_s->post_headers && (data_obj->headerSize > data_obj->headerAmountSent) )
          {
            amtWritten = PR_Write(sock, 
                    URL_s->post_headers + data_obj->headerAmountSent, 
                    data_obj->headerSize - data_obj->headerAmountSent);

            /* if there was a problem */
            if(amtWritten < 0) {
                /* determine whether it was expected */
                if( net_expected_error(URL_s, (struct WritePostDataData **) write_post_data_data) )
                    return 1;
                else 
                    return(MK_TCP_WRITE_ERROR);
            }

#ifdef DEBUG
            NET_NTrace("Tx: ", 4);
            NET_NTrace(URL_s->post_headers + data_obj->headerAmountSent, 
                           amtWritten);
            NET_NTrace("\n", 1);
#endif

            data_obj->headerAmountSent += amtWritten;

            /* If all the header data hasn't been sent, return 1 (not done). */
            if(data_obj->headerSize > data_obj->headerAmountSent)
                return 1;
          }

        /* Separate the headers from the data with a CR and LF. */
        if(URL_s->post_headers && !data_obj->CRSent) {
            data_obj->buffer[0] = CR;
            data_obj->buffer[1] = LF;
            data_obj->buffer[2] = '\0';
            amtWritten = PR_Write(sock, data_obj->buffer, PL_strlen(data_obj->buffer));

            if(amtWritten < 0) {
                /* determine whether it was expected */
                if( net_expected_error(URL_s, (struct WritePostDataData **) write_post_data_data) )
                    return 1;
                else 
                    return(MK_TCP_WRITE_ERROR);
              }
            /* If no data was sent, return 1 (not done). */
            if(amtWritten == 0)
                return 1;

            if(amtWritten >= 1)
                data_obj->CRSent = TRUE;
            if(amtWritten >= 2)
                data_obj->LFSent = TRUE;
        }

        if(URL_s->post_headers && !data_obj->LFSent) {
            data_obj->buffer[0] = LF;
            data_obj->buffer[1] = '\0';
            amtWritten = PR_Write(sock, data_obj->buffer, PL_strlen(data_obj->buffer));

            if(amtWritten < 0) {
                /* determine whether it was expected */
                if( net_expected_error(URL_s, (struct WritePostDataData **) write_post_data_data) )
                    return 1;
                else 
                    return(MK_TCP_WRITE_ERROR);
              }
            /* If no data was sent, return 1 (not done). */
            if(amtWritten == 0)
                return 1;
            data_obj->LFSent = TRUE;

            TRACEMSG(("Adding \\n to command"));
        }

        if(!URL_s->post_data)
          {
            NET_free_write_post_data_object(data_obj);
            *write_post_data_data = NULL;
            return(MK_OUT_OF_MEMORY);
          }

        /* Send the data if we haven't already sent it */
        if(URL_s->post_data_size > data_obj->amt_sent) {
            amtWritten = PR_Write(sock,
                                URL_s->post_data+data_obj->amt_sent,
                                URL_s->post_data_size-data_obj->amt_sent);

            if(amtWritten < 0) {
                /* determine whether it was expected */
                if( net_expected_error(URL_s, (struct WritePostDataData **) write_post_data_data) )
                    return 1;
                else 
                    return(MK_TCP_WRITE_ERROR);
              }
            /* If no data was sent, return 1 (not done). */ 
            if(amtWritten == 0)
                return 1;

#ifdef DEBUG
            NET_NTrace("Tx: ", 4);
            NET_NTrace(URL_s->post_data+data_obj->amt_sent,
                       amtWritten);
            NET_NTrace("\n", 1);
#endif

            data_obj->amt_sent += amtWritten;

            /* if all data has been written, return done */
            if(data_obj->amt_sent >= URL_s->post_data_size) {
                NET_free_write_post_data_object(data_obj);
                *write_post_data_data = NULL;
                return 0;
            }
            else {
                return amtWritten;
            }
        }
        else {
            NET_free_write_post_data_object(data_obj);
            *write_post_data_data = NULL;
            return 0;
        }
      }

    /* By this time all headers and data should have been sent, shouldn't get here */
    PR_ASSERT(0);
    NET_free_write_post_data_object(data_obj);
    *write_post_data_data = NULL;
    return(-1); /* shouldn't ever get here */

}

void
NET_ParseContentTypeHeader(MWContext *context, char *value, URL_Struct *URL_s, PRBool is_http)
{
    char *first_arg, *next_arg;

    if(URL_s->preset_content_type)
        return;

    first_arg = strtok(value, ";");

    StrAllocCopy(URL_s->content_type, XP_StripLine(value));
    TRACEMSG(("Found content_type: %s",URL_s->content_type));
   
    /* assign and compare
     *
     * search for charset
     * and boundry
     */
    while((next_arg = strtok(NULL, ";")) != NULL)
      {
        next_arg = XP_StripLine(next_arg);

        if(!PL_strncasecmp(next_arg,"CHARSET=", 8))
          {
#ifdef MOZILLA_CLIENT
            char *charset_tag = NET_RemoveQuotes(next_arg+8);
            /* 
             * To make http LOAD-FROM-NET and LOAD-FROM-CACHE
             * charset tag handling consistant just put the http 
             * charset info in the url struct.
             * We will use this later when setting up charset converter.
             * (we do this because: on a reload from cache this code will not get called)
             */
            if (is_http)
            {
                /* record HTTP charset tag so we can: */
                /*   1) use it to figure out the doc_csid */
                /*   2) report it to the user */
                StrAllocCopy(URL_s->charset, charset_tag);
                TRACEMSG(("Found HTTP charset: %s", charset_tag));
            }
            else
            {
                INTL_CCCReportMetaCharsetTag(context, charset_tag);
                TRACEMSG(("Found Meta charset: %s", charset_tag));
            }
#else
            PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
          }
        else if(!PL_strncasecmp(next_arg,"BOUNDARY=", 9))
          {
            StrAllocCopy(URL_s->boundary, NET_RemoveQuotes(next_arg+9));
            TRACEMSG(("Found boundary: %s", URL_s->boundary));
          }
        else if(!PL_strncasecmp(next_arg,"AUTOSCROLL", 10))
          {

#define DEFAULT_AUTO_SCROLL_BUFFER 100
            if(*next_arg+10 == '=')
                URL_s->auto_scroll = atoi(NET_RemoveQuotes(next_arg+11));

            if(!URL_s->auto_scroll)
                URL_s->auto_scroll = DEFAULT_AUTO_SCROLL_BUFFER;

            TRACEMSG(("Found autoscroll attr.: %ud", URL_s->auto_scroll));
          }
      }
}

/* parse a mime header.
 * 
 * Both name and value strings will be modified during
 * parsing.  If you need to retain your strings make a
 * copy before passing them in.
 *
 * Values will be placed in the URL struct.
 *
 * returns TRUE if it found a valid header
 * and FALSE if it didn't
 */

PUBLIC Bool
NET_ParseMimeHeader(FO_Present_Types outputFormat,
                    MWContext  *context, 
                    URL_Struct *URL_s, 
                    char       *name, 
                    char       *value,
                    XP_Bool    is_http)
{
    Bool  found_one = FALSE;
    Bool  ret_value = FALSE;
    char  empty_string[2];
    char  *colon_ptr;

    if(!name || !URL_s)
        return(FALSE);

    name = XP_StripLine(name);  

    if(value)
      {
        value = XP_StripLine(value);    
      }
    else
      {
        *empty_string = '\0';
        value = empty_string;
      }

    colon_ptr = PL_strchr(name, ':');
    if (colon_ptr) {
        *colon_ptr = '\0';
        }
    ret_value = NET_AddToAllHeaders(URL_s, name, value);
    if (colon_ptr) {
        *colon_ptr = ':';
        }
    if (!ret_value) {
        return(FALSE);
        }

    switch(toupper(*name))
      {
        case 'A':
            if(!PL_strncasecmp(name,"ACCEPT-RANGES:",14)
                || !PL_strncasecmp(name, "ALLOW-RANGES:",13))
              {
                char * next_arg = strtok(value, ";");

                found_one = TRUE;

                while(next_arg)
                  {
                    next_arg = XP_StripLine(next_arg);

                    if(!PL_strncasecmp(next_arg,"BYTES", 5))
                      {
                        TRACEMSG(("Document Allows for BYTERANGES!"));
                        URL_s->server_can_do_byteranges = TRUE;
                      }

                    next_arg = strtok(NULL, ";");
                  }
              }
            else if(!PL_strncasecmp(name, "AGE:",4)) 
            {
                long age;

                strtok(value, ";"); /* terminate at ';' */

                age = atol(value);
                /* Small deviation from the spec. Assumption being made-
                   Req. time ~= Resp Time ~= now */
                if (URL_s->server_date)
                {
                    time_t now = time(NULL);
                    time_t correction = now - MAX(age, MAX(0, now- URL_s->server_date));
                    if (URL_s->expires)
                        URL_s->expires += correction;
                }
            }
        case 'C':
            if (!PL_strncasecmp(name,"CACHE-CONTROL:",14))
            {
            /* Potential values include */
                /* public */
                /* private[=field-name] */
                /* no-cache [=field-name] */
                /* no-store */
                /* no-transform - this is only applicable for proxies and hence not put here*/
                /* must-revalidate */
                /* proxy-revalidate */
                /* max-age=delta-seconds */
                /* s-maxage=delta-seconds */

                /* any cache extension */

                char * control = NET_RemoveQuotes(value);

                if (!PL_strncasecmp(control, "NO-CACHE", 8))
                {
                    /* same as pragma=no-cache */
                    URL_s->dont_cache = TRUE;
                }
                else if (!PL_strncasecmp(control, "MAX-AGE=", 8))
                {
                    /* Takes precedence over the Expires header. 
                       Corrected Expires is Max-age + Server Date */
                    if (URL_s->server_date)
                    {
                        URL_s->expires = URL_s->server_date + atol(strtok(control+8,";")); 
                    }
                }
#if 0   /* Unimplemented */
                else if (!PL_strncasecmp(control, "PUBLIC", 6))
                {
                    /* Place holder for shared cache concepts */
                }
                else if (!PL_strncasecmp(control, "PRIVATE", 7)) 
                {
                    /* Place holder for shared cache concepts */
                }
                else if (!PL_strncasecmp(control, "NO-STORE", 8))
                {
                    /* According to the spec this "should" disable explicit saving 
                       of the document outside the caching system, eg. in "Save As..."
                       dialogs. Since this idea sucks, this is a place holder. */
                }
                else if (!PL_strncasecmp(control, "MUST-REVALIDATE", 15))
                {
                    /* The cache must do an end-to-end revalidation every time, if
                       the response headers suggest an entity is stale. We do this
                       anyway. */
                }
                else if (!PL_strncasecmp(control, "PROXY-REVALIDATE", 16))
                {
                    /* Same as must-revalidate except that it does not apply to 
                       non-shared user agents (like us) */
                }
                else if (!PL_strncasecmp(control, "S-MAXAGE", 8))
                {
                    /* Applicable for shared caches. Place holder. */
                }
                else 
                {
                    /* A new cache extension */
                }
#endif /* Unimplemented */
            }
            else if(!PL_strncasecmp(name,"CONTENT-DISPOSITION:",20))
              {
                char *next_arg;

                found_one = TRUE;

                next_arg = strtok(value, ";");

                while(next_arg)
                  {  
                    next_arg = XP_StripLine(next_arg);
                
                    if(!PL_strncasecmp(next_arg,"filename=", 9))
                      {
                        StrAllocCopy(URL_s->content_name,
                                     NET_RemoveQuotes(next_arg+9));
                      }
                    next_arg = strtok(NULL, ";");
                  }
    
              }
            else if(!PL_strncasecmp(name,"CONTENT-TYPE:",13))
              {
                found_one = TRUE;
                NET_ParseContentTypeHeader(context, value, URL_s, is_http);
              }
            else if(!PL_strncasecmp(name,"CONTENT-LENGTH:",15))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */
                
                /* don't reset the content-length if
                 * we have already set it from the content-range
                 * header.  If high_range is set then we must
                 * have seen a content-range header.
                 */
                if(!URL_s->high_range)
                    URL_s->content_length = atol(value);
              }
            else if(!PL_strncasecmp(name,"CONTENT-ENCODING:",17))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */
                StrAllocCopy(URL_s->content_encoding, value);
              }
            else if(!PL_strncasecmp(name,"CONTENT-RANGE:",14))
              {
                unsigned long low, high, length;
                strtok(value, ";"); /* terminate at ';' */

                /* range header looks like:
                 * Range: bytes x-y/z
                 * where:
                 *
                 * X     is the number of the first byte returned
                 *       (the first byte is byte number zero).
                 *
                 * Y     is the number of the last byte returned
                 *       (in case of the end of the document this
                 *       is one smaller than the size of the document
                 *       in bytes).
                 *
                 * Z     is the total size of the document in bytes.
                 *
                 * Scan through temp variables because %l is 64 bits on OSF1
                 */
                sscanf(value, "bytes %lu-%lu/%lu", &low, &high, &length);
                URL_s->low_range = (int32) low;
                URL_s->high_range = (int32) high;
                URL_s->content_length = (int32) length;

                /* if we get a range header set the "can-do-byteranges"
                 * since it must be doing byteranges
                 */
                URL_s->server_can_do_byteranges = TRUE;
              }
            else if(!PL_strncasecmp(name,"CONNECTION:",11))
              {
                strtok(value, ";"); /* terminate at ';' */
                strtok(value, ","); /* terminate at ',' */
                if(!PL_strcasecmp("KEEP-ALIVE", XP_StripLine(value)))
                    URL_s->can_reuse_connection = TRUE;
                if(!PL_strcasecmp("CLOSE", XP_StripLine(value)))
                    URL_s->can_reuse_connection = FALSE;
              }
            break;

        case 'D':
            if(!PL_strncasecmp(name,"DATE:",5))
              {
                found_one = TRUE;
                URL_s->server_date = NET_ParseDate(value);
              }
#if 0
            else if(!PL_strncasecmp(name,"DEST-IP:",8))
              {
                found_one = TRUE;
                URL_s->destIP = PL_strdup(value);
              }
#endif
            break;
        case 'E':
            if(!PL_strncasecmp(name,"EXPIRES:",8))
              {
                char *cp, *expires = NET_RemoveQuotes(value);
                Bool is_number=TRUE;
                
                /* Expires: 123   - number of seconds
                 * Expires: date  - absolute date
                 */

                strtok(value, ";"); /* terminate at ';' */

                /* check to see if the expires date is just a
                 * value in seconds
                 */
                for(cp = expires; *cp != '\0'; cp++)
                    if(!isdigit(*cp))
                      {
                        is_number=FALSE;
                        break;
                      }
                
                if(is_number)
                    URL_s->expires = time(NULL) + atol(expires);
                else
                    URL_s->expires = NET_ParseDate(expires);

                /* if we couldn't parse the date correctly
                 * make it already expired, as per the HTTP spec
                 */
                if(!URL_s->expires)
                    URL_s->expires = 1;

                found_one = TRUE;
              }
            else if(!PL_strncasecmp(name,"EXT-CACHE:",10))
#ifdef NU_CACHE
            {
                PR_ASSERT(0);
        /* Cool find... somebody uses this? let me know -Gagan */
            }
#else
            {
#ifdef MOZILLA_CLIENT
                char * next_arg = strtok(value, ";");
                char * name=0;
                char * instructions=0;
                found_one = TRUE;

                while(next_arg)
                  {
                    next_arg = XP_StripLine(next_arg);

                    if(!PL_strncasecmp(next_arg,"name=", 5))
                      {
                        TRACEMSG(("Found external cache name: %s", next_arg+5));
                        name = NET_RemoveQuotes(next_arg+5);
                      }
                    else if(!PL_strncasecmp(next_arg,"instructions=", 13))
                      {
                        TRACEMSG(("Found external cache instructions: %s", 
                                  next_arg+13));
                        instructions = NET_RemoveQuotes(next_arg+13);
                      }

                    next_arg = strtok(NULL, ";");
                  }
    
                if(name)
                    NET_OpenExtCacheFAT(context, name, instructions);
#else
                PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
              }
#endif /* NU_CACHE */
            else if (!PL_strncasecmp(name, "ETAG:",5))
            {
                /* Weak Validators are skipped for now*/
                char* etag;
                if (!PL_strncasecmp(value, "W/", 2))
                    etag = NET_RemoveQuotes(value+2);
                else
                    etag = NET_RemoveQuotes(value);

                StrAllocCopy(URL_s->etag,etag);
            }
            break;

        case 'L':
            if(!PL_strncasecmp(name,"LOCATION:",9))
              {
                found_one = TRUE;

                /* don't do this here because a url can
                 * contain a ';'
                 * strtok(value, ";"); 
                 */

                URL_s->redirecting_url = NET_MakeAbsoluteURL(
                                                    URL_s->address,
                                                    XP_StripLine(value));

                TRACEMSG(("Found location: %s\n",URL_s->redirecting_url));

                if(MOCHA_TYPE_URL == NET_URL_Type(URL_s->redirecting_url))
                  {
                    /* don't allow mocha URL's as refresh
                     */
                    FREE_AND_CLEAR(URL_s->redirecting_url);
                  }
              }
            else if(!PL_strncasecmp(name,"LAST-MODIFIED:",14))
              {
                found_one = TRUE;

                URL_s->last_modified = NET_ParseDate(value);
                TRACEMSG(("Found last modified date: %d\n",URL_s->last_modified));
              }
            else if(!PL_strncasecmp(name,"LINK:",5))
              {
#define PAGE_SERVICES_REL "pageServices"

#define PRIVACY_POLICY_REL "privacyPolicy"
                enum { UNKNOWN_REL_TYPE, PRIVACY_POLICY_REL_TYPE, 
                    PAGE_SERVICES_REL_TYPE } rel_type;

                char * next_arg = strtok(value, ";");
                char * link_val;

                found_one = TRUE;

                rel_type = UNKNOWN_REL_TYPE;

                /* strip the < and > from the url */
                if(*value == '<')
                {
                   value++;
                   /* strip the end one too */
                   value[PL_strlen(value)-1] = '\0';
                }

                /* ok if malloc fails */
                link_val = NET_MakeAbsoluteURL(URL_s->address,
                                               XP_StripLine(value));
                while(next_arg)
                  {
                    next_arg = XP_StripLine(next_arg);

                    if(!PL_strncasecmp(next_arg,"rel=", 4))
                      {
                        char * rel = NET_RemoveQuotes(next_arg+4);

                        if(!PL_strcasecmp(rel, PAGE_SERVICES_REL))
                            rel_type = PAGE_SERVICES_REL_TYPE;
                        else if (!PL_strcasecmp(rel, PRIVACY_POLICY_REL))
                            rel_type = PRIVACY_POLICY_REL_TYPE; 
                     }

                    next_arg = strtok(NULL, ";");
                  }

                /* if we fount a rel for page services assign it */
                if(rel_type == PAGE_SERVICES_REL_TYPE)
                    URL_s->page_services_url = link_val;
                else if (rel_type == PRIVACY_POLICY_REL_TYPE)
                    URL_s->privacy_policy_url = link_val;
                else
                    PR_FREEIF(link_val);
    
              }
            break;

        case 'P':
            if(!PL_strncasecmp(name,"PROXY-AUTHENTICATE:",19))
              {
                char *auth = value;

                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */

                if (net_IsBetterAuth(auth, URL_s->proxy_authenticate))
                  StrAllocCopy(URL_s->proxy_authenticate, auth);
              }
            else if(!PL_strncasecmp(name,"PROXY-CONNECTION:",17))
              {
                strtok(value, ";"); /* terminate at ';' */
                strtok(value, ","); /* terminate at ',' */
                if(!PL_strcasecmp("KEEP-ALIVE", XP_StripLine(value)))
                    URL_s->can_reuse_connection = TRUE;
                if(!PL_strcasecmp("CLOSE", XP_StripLine(value)))
                    URL_s->can_reuse_connection = FALSE;
              }
            else if(!PL_strncasecmp(name,"PRAGMA:",7))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */

                if(!PL_strcasecmp(value, "NO-CACHE"))
                    URL_s->dont_cache = TRUE;
#ifdef TRUST_LABELS
            }
            else if ( IsTrustLabelsEnabled() && !PL_strncasecmp(name,"PICS-LABEL:",11))
              {
                /* we are looking for trust labels and a PICS-label was found
                 * Parse the label to see if it is a trust label.  If it is a 
                 * trust label then it will be added to the TrustList in URL_s.
                 */
                PICS_ExtractTrustLabel( URL_s, value );
                /* Then when we are all done with the header the Set-Cookie fields
                 * will be processed and matched to the trust labels.
                 */
#endif
              }
            break;

        case 'R':
            
            if(!PL_strncasecmp(name,"RANGE:",6))
              {
                unsigned long low, high, length;
                strtok(value, ";"); /* terminate at ';' */

                /* range header looks like:
                 * Range: bytes x-y/z
                 * where:
                 *
                 * X     is the number of the first byte returned 
                 *       (the first byte is byte number zero).
                 *
                 * Y     is the number of the last byte returned 
                 *       (in case of the end of the document this 
                 *       is one smaller than the size of the document
                 *       in bytes).
                 *
                 * Z     is the total size of the document in bytes.
                 *
                 * Scan through temp variables because %l is 64 bits on OSF1
                 */
                sscanf(value, "bytes %lu-%lu/%lu", &low, &high, &length); 
                URL_s->low_range = (int32) low;
                URL_s->high_range = (int32) high;
                URL_s->content_length = (int32) length;

                /* if we get a range header set the "can-do-byteranges"
                 * since it must be doing byteranges
                 */
                URL_s->server_can_do_byteranges = TRUE;
              }
            else if(!PL_strncasecmp(name,"REFRESH:",8) && !EDT_IS_EDITOR(context))
              {
                char *first_arg, *next_arg;

                found_one = TRUE;

                /* clear any previous refresh URL */
                if(URL_s->refresh_url)
                  {
                    FREE(URL_s->refresh_url);
                    URL_s->refresh_url=0;
                  }

                first_arg = strtok(value, ";");

                URL_s->refresh = atol(value);
                TRACEMSG(("Found refresh header: %d",URL_s->refresh));

                /* assign and compare
                 */
                while((next_arg = strtok(NULL, ";")) != NULL)
                  {
                    next_arg = XP_StripLine(next_arg);


                    if(!PL_strncasecmp(next_arg,"URL=", 4))
                      {
                        URL_s->refresh_url = NET_MakeAbsoluteURL(
                                                URL_s->address,
                                                NET_RemoveQuotes(next_arg+4));
                        TRACEMSG(("Found refresh url: %s",
                                                URL_s->refresh_url));

                        if(MOCHA_TYPE_URL == NET_URL_Type(URL_s->refresh_url))
                          {
                            /* don't allow mocha URL's as refresh
                             */
                            FREE_AND_CLEAR(URL_s->refresh_url);
                          }
                      }
                  }

                if(!URL_s->refresh_url)
                    StrAllocCopy(URL_s->refresh_url, URL_s->address);
              }
            break;

        case 'S':
#ifdef TRUST_LABELS
            if( !IsTrustLabelsEnabled() && !PL_strncasecmp(name,"SET-COOKIE:",11))
#else
            if(!PL_strncasecmp(name,"SET-COOKIE:",11))
#endif
              {
                found_one = TRUE;
                NET_SetCookieStringFromHttp(outputFormat, URL_s, context, URL_s->address, value);
              }
            else if(!PL_strncasecmp(name, "SERVER:", 7))
              {
                found_one = TRUE;

                if(PL_strcasestr(value, "NETSITE"))
                    URL_s->is_netsite = TRUE;
                else if(PL_strcasestr(value, "NETSCAPE"))
                    URL_s->is_netsite = TRUE;
              }
            break;

        case 'T':
            if(!PL_strncasecmp(name,"TRANSFER-ENCODING:",18))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */
                StrAllocCopy(URL_s->transfer_encoding, value);
              }

        case 'W':
            if(!PL_strncasecmp(name,"WWW-AUTHENTICATE:",17))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */

                StrAllocCopy(URL_s->authenticate, value);
              }
            else if(!PL_strncasecmp(name, "WWW-PROTECTION-TEMPLATE:", 24))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */

                StrAllocCopy(URL_s->protection_template, value);
               }
            else if(!PL_strncasecmp(name, "WINDOW-TARGET:", 14))
              {
                found_one = TRUE;

                strtok(value, ";"); /* terminate at ';' */

                if (URL_s->window_target == NULL)
          {
            if ((NET_IS_ALPHA(value[0]) != FALSE)||
                (NET_IS_DIGIT(value[0]) != FALSE)||
                (value[0] == '_'))
              {
                StrAllocCopy(URL_s->window_target, value);
              }
          }
               }
            break;

        default:
        /* ignore other headers */
            break;
      }

    return(found_one);
}



/* scans a line for references to URL's and turns them into active
 * links.  If the output size is exceeded the line will be
 * truncated.  "output" must be at least "output_size" characters
 * long
 *
 * This also quotes other HTML forms, and italicizes citations,
 * unless `urls_only' is true.
 */

#ifndef MOZILLA_CLIENT
 /* If we're not in the client, stub out the libmsg interface to the 
    citation-highlighting code.
  */
# define MSG_PlainFont      0
# define MSG_BoldFont       1
# define MSG_ItalicFont     2
# define MSG_BoldItalicFont 3
# define MSG_NormalSize     4
# define MSG_Bigger         5
# define MSG_Smaller        6
#endif /* MOZILLA_CLIENT */

static MSG_FONT CitationFont = MSG_ItalicFont;
static int32 CitationSize = 0;
static char *CitationColor = 0;
static int CitationDataValid = -1; /* -1=first time, 0=changed; 1=data ok  */



/* fix Mac warning about missing prototype */
int PR_CALLBACK
net_citation_style_changed(const char* name, void* closure);

int PR_CALLBACK
net_citation_style_changed(const char* name, void* closure)
{
  CitationDataValid = 0;
  return 0;
}





PUBLIC int
NET_ScanForURLs(MSG_Pane* pane, const char *input, int32 input_size,
                char *output, int output_size, XP_Bool urls_only)
{
  int col = 0;
  const char *cp;
  const char *end = input + input_size;
  char *output_ptr = output;
  char *end_of_buffer = output + output_size - 40; /* add safty zone :( */
  Bool line_is_citation = FALSE;
  const char *cite_open1, *cite_close1;
  const char *cite_open2, *cite_close2;
  const char* color = NULL;

  if (urls_only)
    {
      cite_open1 = cite_close1 = "";
      cite_open2 = cite_close2 = "";
    }
  else
    {
#ifdef MOZILLA_CLIENT
      if (CitationDataValid != 1) {
          int32 value = (int32) MSG_ItalicFont;
          if (CitationDataValid < 0) {
              PREF_RegisterCallback("mail.quoted_style",
                                    net_citation_style_changed,
                                    NULL);
              PREF_RegisterCallback("mail.quoted_size",
                                    net_citation_style_changed,
                                    NULL);
              PREF_RegisterCallback("mail.citation_color",
                                    net_citation_style_changed,
                                    NULL);
          }
          if ( (PREF_OK == PREF_GetIntPref(pref_mailQuotedStyle, &value)) ) {
            CitationFont = (MSG_FONT) value;
          } else {
            CitationFont = MSG_ItalicFont;
          }
          
          CitationSize = 0;
          if ( (PREF_OK != PREF_GetIntPref(pref_mailQuotedStyle, &CitationSize)) ) {
              CitationSize = 0;
          }
          FREEIF(CitationColor);
          CitationColor = NULL;
          PREF_CopyCharPref("mail.citation_color", &CitationColor);
          CitationDataValid = 1;
      }

      switch (CitationFont)
        {
        case MSG_PlainFont:
          cite_open1 = "", cite_close1 = "";
          break;
        case MSG_BoldFont:
          cite_open1 = "<B>", cite_close1 = "</B>";
          break;
        case MSG_ItalicFont:
          cite_open1 = "<I>", cite_close1 = "</I>";
          break;
        case MSG_BoldItalicFont:
          cite_open1 = "<B><I>", cite_close1 = "</I></B>";
          break;
        default:
          PR_ASSERT(0);
          cite_open1 = cite_close1 = "";
          break;
        }
      switch (CitationSize) {
      case 0:                   /* Normal */
          cite_open2 = "", cite_close2 = "";
          break;
      case 1:                   /* Bigger */
          cite_open2 = "<FONT SIZE=+1>", cite_close2 = "</FONT>";
          break;
      case 2:                   /* Smaller */
      case -1:              /* backwards compatability with some old code */
          cite_open2 = "<FONT SIZE=-1>", cite_close2 = "</FONT>";
          break;
      default:
          PR_ASSERT(0);
          cite_open2 = cite_close2 = "";
          break;
      }

#else
        PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
    }

  if (!urls_only)
    {
      /* Decide whether this line is a quotation, and should be italicized.
         This implements the following case-sensitive regular expression:

            ^[ \t]*[A-Z]*[]>]

         Which matches these lines:

            > blah blah blah
                 > blah blah blah
            LOSER> blah blah blah
            LOSER] blah blah blah
       */
      const char *s = input;
      while (s < end && NET_IS_SPACE (*s)) s++;
      while (s < end && *s >= 'A' && *s <= 'Z') s++;

      if (s >= end)
        ;
      else if (input_size >= 6 && *s == '>' &&
               !PL_strncmp (input, ">From ", 6))    /* sendmail... */
        ;
      else if (*s == '>' || *s == ']')
        {
          line_is_citation = TRUE;
          PL_strcpy(output_ptr, cite_open1);
          output_ptr += PL_strlen(cite_open1);
          PL_strcpy(output_ptr, cite_open2);
          output_ptr += PL_strlen(cite_open2);
          if (CitationColor &&
              output_ptr + PL_strlen(CitationColor) + 20 < end_of_buffer) {
            PL_strcpy(output_ptr, "<FONT COLOR=");
            output_ptr += PL_strlen(output_ptr);
            PL_strcpy(output_ptr, CitationColor);
            output_ptr += PL_strlen(output_ptr);
            PL_strcpy(output_ptr, ">");
            output_ptr += PL_strlen(output_ptr);
          }
        }
    }

  /* Normal lines are scanned for buried references to URL's
     Unfortunately, it may screw up once in a while (nobody's perfect)
   */
  for(cp = input; cp < end && output_ptr < end_of_buffer; cp++)
    {
      /* if NET_URL_Type returns true then it is most likely a URL
         But only match protocol names if at the very beginning of
         the string, or if the preceeding character was not alphanumeric;
         this lets us match inside "---HTTP://XXX" but not inside of
         things like "NotHTTP://xxx"
       */
      int type = 0;
      if(!NET_IS_SPACE(*cp) &&
         (cp == input || (!NET_IS_ALPHA(cp[-1]) && !NET_IS_DIGIT(cp[-1]))) &&
         (type = NET_URL_Type(cp)) != 0)
        {
          const char *cp2;

          for(cp2=cp; cp2 < end; cp2++)
            {
              /* These characters always mark the end of the URL. */
              if (NET_IS_SPACE(*cp2) ||
                  *cp2 == '<' || *cp2 == '>' ||
                  *cp2 == '`' || *cp2 == ')' ||
                  *cp2 == '\'' || *cp2 == '"' ||
                  *cp2 == ']' || *cp2 == '}') break;
            }

          /* Check for certain punctuation characters on the end, and strip
             them off. */
          while (cp2 > cp && 
                 (cp2[-1] == '.' || cp2[-1] == ',' || cp2[-1] == '!' ||
                  cp2[-1] == ';' || cp2[-1] == '-' || cp2[-1] == '?' ||
                  cp2[-1] == '#'))
            cp2--;

          col += (cp2 - cp);

          /* if the url is less than 7 characters then we screwed up
           * and got a "news:" url or something which is worthless
           * to us.  Exclude the A tag in this case.
           *
           * Also exclude any URL that ends in a colon; those tend
           * to be internal and magic and uninteresting.
           *
           * And also exclude the builtin icons, whose URLs look
           * like "internal-gopher-binary".
           */
          if (cp2-cp < 7 ||
              (cp2 > cp && cp2[-1] == ':') ||
              !PL_strncmp(cp, "internal-", 9))
            {
              memcpy(output_ptr, cp, cp2-cp);
              output_ptr += (cp2-cp);
              *output_ptr = 0;
            }
          else
            {
              char *quoted_url;
              int32 size_left = output_size - (output_ptr-output);

              if(cp2-cp > size_left)
                return MK_OUT_OF_MEMORY;

              memcpy(output_ptr, cp, cp2-cp);
              output_ptr[cp2-cp] = 0;
              quoted_url = NET_EscapeHTML(output_ptr);
              if (!quoted_url) return MK_OUT_OF_MEMORY;
              PR_snprintf(output_ptr, size_left,
                          "<A HREF=\"%s\">%s</A>",
                          quoted_url,
                          quoted_url);
              output_ptr += PL_strlen(output_ptr);
              PR_Free(quoted_url);
            }

          cp = cp2-1;  /* go to next word */
        }
      else
        {
          /* Make sure that special symbols don't screw up the HTML parser
           */
          if(*cp == '<')
            {
              PL_strcpy(output_ptr, "&lt;");
              output_ptr += 4;
              col++;
            }
          else if(*cp == '>')
            {
              PL_strcpy(output_ptr, "&gt;");
              output_ptr += 4;
              col++;
            }
          else if(*cp == '&')
            {
              PL_strcpy(output_ptr, "&amp;");
              output_ptr += 5;
              col++;
            }
          else
            {
              *output_ptr++ = *cp;
              col++;
            }
        }
    }

  *output_ptr = 0;

  if (line_is_citation) /* Close off the highlighting */
    {
      if (CitationColor) {
        PL_strcpy(output_ptr, "</FONT>");
        output_ptr += PL_strlen(output_ptr);
      }

      PL_strcpy(output_ptr, cite_close2);
      output_ptr += PL_strlen (cite_close2);
      PL_strcpy(output_ptr, cite_close1);
      output_ptr += PL_strlen (cite_close1);
    }

  return 0;
}


static void
Append(char** output, int32* output_max, char** curoutput, const char* buf,
       int32 length)
{
    if (length + (*curoutput) - (*output) >= *output_max) {
        int offset = (*curoutput) - (*output);
        do {
            (*output_max) += 1024;
        } while (length + (*curoutput) - (*output) >= *output_max);
        *output = PR_Realloc(*output, *output_max);
        if (!*output) return;
        *curoutput = *output + offset;
    }
    memcpy(*curoutput, buf, length);
    *curoutput += length;
}


char*
NET_ScanHTMLForURLs(const char* input)
{
    char* output = NULL;
    char* curoutput;
    int32 output_max;
    char* tmpbuf = NULL;
    int32 tmpbuf_max;
    int32 inputlength;
    const char* inputend;
    const char* linestart;
    const char* lineend;

    PR_ASSERT(input);
    if (!input) return NULL;
    inputlength = PL_strlen(input);

    output_max = inputlength + 1024; /* 1024 bytes ought to be enough to quote
                                        several URLs, which ought to be as many
                                        as most docs use. */
    output = PR_Malloc(output_max);
    if (!output) goto FAIL;

    tmpbuf_max = 1024;
    tmpbuf = PR_Malloc(tmpbuf_max);
    if (!tmpbuf) goto FAIL;

    inputend = input + inputlength;

    linestart = input;
    curoutput = output;


    /* Here's the strategy.  We find a chunk of plainish looking text -- no
       embedded CR or LF, no "<" or "&".  We feed that off to NET_ScanForURLs,
       and append the result.  Then we skip to the next bit of plain text.  If
       we stopped at an "&", go to the terminating ";".  If we stopped at a
       "<", well, if it was a "<A>" tag, then skip to the closing "</A>".
       Otherwise, skip to the end of the tag.
       */


    lineend = linestart;
    while (linestart < inputend && lineend <= inputend) {
        switch (*lineend) {
        case '<':
        case '>':
        case '&':
        case CR:
        case LF:
        case '\0':
            if (lineend > linestart) {
                int length = lineend - linestart;
                if (length * 3 > tmpbuf_max) {
                    tmpbuf_max = length * 3 + 512;
                    PR_Free(tmpbuf);
                    tmpbuf = PR_Malloc(tmpbuf_max);
                    if (!tmpbuf) goto FAIL;
                }
                if (NET_ScanForURLs(NULL, linestart, length,
                                    tmpbuf, tmpbuf_max, TRUE) < 0) {
                    goto FAIL;
                }
                length = PL_strlen(tmpbuf);
                Append(&output, &output_max, &curoutput, tmpbuf, length);
                if (!output) goto FAIL;

            }
            linestart = lineend;
            lineend = NULL;
            if (inputend - linestart < 5) {
                /* Too little to worry about; shove the rest out. */
                lineend = inputend;
            } else {
                switch (*linestart) {
                case '<':
                    if ((linestart[1] == 'a' || linestart[1] == 'A') &&
                        linestart[2] == ' ') {
                        lineend = PL_strcasestr(linestart, "</a");
                        if (lineend) {
                            lineend = PL_strchr(lineend, '>');
                            if (lineend) lineend++;
                        }
                    } else {
                        lineend = PL_strchr(linestart, '>');
                        if (lineend) lineend++;
                    }
                    break;
                case '&':
                    lineend = PL_strchr(linestart, ';');
                    if (lineend) lineend++;
                    break;
                default:
                    lineend = linestart + 1;
                    break;
                }
            }
            if (!lineend) lineend = inputend;
            Append(&output, &output_max, &curoutput, linestart,
                   lineend - linestart);
            if (!output) goto FAIL;
            linestart = lineend;
            break;
        default:
            lineend++;
        }
    }
    PR_Free(tmpbuf);
    *curoutput = '\0';
    return output;

FAIL:
    if (tmpbuf) PR_Free(tmpbuf);
    if (output) PR_Free(output);
    return NULL;
}


/* try to make sure that this is a fully qualified
 * email address including a host and domain
 */
PUBLIC Bool 
NET_IsFQDNMailAddress(const char * string)
{
    /* first make sure that an @ exists
     */
    char * at_sign = PL_strchr(string, '@');

    if(at_sign)
      {
        /* make sure it has at least one period 
         */
        if(PL_strchr(at_sign, '.'))
            return(TRUE);
      }

    return(FALSE);
}

HG83778




/* returns true if the URL is a secure URL address
 */
PUBLIC Bool
NET_IsURLSecure(char * address)
{
   int type = NET_URL_Type (address);

   TRACEMSG(("NET_IsURLSecure called, type: %d", type));

    if(HG83773 type == INTERNAL_IMAGE_TYPE_URL
        || type == SECURE_LDAP_TYPE_URL)
        return(TRUE);

    if(!PL_strncasecmp(address, "/mc-icons/", 10) ||
       !PL_strncasecmp(address, "/ns-icons/", 10))
        return(TRUE);

    if(!PL_strncasecmp(address, "internal-external-reconnect:", 28))
        return(TRUE);

    if(!PL_strcasecmp(address, "internal-external-plugin"))
        return(TRUE);

    if(!PL_strncasecmp(address, "snews:", 6))
        return TRUE;

        /*
         * IMAP URLs begin with "mailbox://" unlike POP URLs which begin
         * with "mailbox:".
         */
    HG35632

    
    TRACEMSG(("NET_IsURLSecure: URL NOT SECURE"));

    return(FALSE);
}

/* escapes all '<', '>' and '&' characters in a string
 * returns a string that must be freed
 */
PUBLIC char *
NET_EscapeHTML(const char * string)
{
    char *rv = (char *) PR_Malloc(PL_strlen(string)*4 + 1); /* The +1 is for
                                                              the trailing
                                                              null! */
    char *ptr = rv;

    if(rv)
      {
        for(; *string != '\0'; string++)
          {
            if(*string == '<')
              {
                *ptr++ = '&';
                *ptr++ = 'l';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '>')
              {
                *ptr++ = '&';
                *ptr++ = 'g';
                *ptr++ = 't';
                *ptr++ = ';';
              }
            else if(*string == '&')
              {
                *ptr++ = '&';
                *ptr++ = 'a';
                *ptr++ = 'm';
                *ptr++ = 'p';
                *ptr++ = ';';
              }
            else
              {
                *ptr++ = *string;
              }
          }
        *ptr = '\0';
      }

    return(rv);
}

/* URL-encode all '"' characters in a string into %22.
 * returns a string that must be freed
 */
PUBLIC char *
NET_EscapeDoubleQuote(const char * string)
{
    char *rv = (char *) PR_Malloc(PL_strlen(string)*3 + 1);
    char *ptr = rv;
    if (rv) 
      {
        for (; *string != '\0'; string++) 
          {
            if (*string == '"')
              {
                *ptr++ = '%';
                *ptr++ = '2';
                *ptr++ = '2';
              }
            else
              {
                *ptr++ = *string;
              }
          }
        *ptr = '\0';
      }
    return rv;
}


PUBLIC char *
NET_SpaceToPlus(char * string)
{

    char * ptr = string;

    if(!ptr)
        return(NULL);

    for(; *ptr != '\0'; ptr++)
        if(*ptr == ' ')
            *ptr = '+';

    return(string);
}


/* returns true if the functions thinks the string contains
 * HTML
 */
PUBLIC PRBool
NET_ContainsHTML(char * string, int32 length)
{
    char * ptr = string;
    register int32 count=length;

    /* restrict searching to first K to limit false positives */
    if(count > 1024)
        count = 1024;

    /* if the string begins with "#!" or "%!" then it's a script of some kind,
       and it doesn't matter how many HTML tags that program references in its
       source -- it ain't HTML.  This false match happened all the time with,
       for example, CGI scripts written in sh or perl that emit HTML. */
    if (count > 2 &&
        (string[0] == '#' || string[0] == '%') &&
        string[1] == '!')
      return FALSE;

    /* If it begins with a mailbox delimiter, it's not HTML. */
    if (count > 5 &&
        (!PL_strncmp(string, "From ", 5) ||
         !PL_strncmp(string, ">From ", 6)))
      return FALSE;

    for(; count > 0; ptr++, count--)
        if(*ptr == '<')
          {
            if(count > 3 && !PL_strncasecmp(ptr+1, "HTML", 4))
                return(TRUE);

            if(count > 4 && !PL_strncasecmp(ptr+1, "TITLE", 5))
                return(TRUE);
        
            if(count > 3 && !PL_strncasecmp(ptr+1, "FRAMESET", 8))
                return(TRUE);

            if(count > 2 && 
                toupper(*(ptr+1)) == 'H' 
                        && isdigit(*(ptr+2)) && *(ptr+3) == '>')
                return(TRUE);
          }

    return(FALSE);
}

/* take a Layout generated LO_FormSubmitData_struct
 * and use it to add post data to the URL Structure
 * generated by CreateURLStruct
 *
 * DOES NOT Generate the URL Struct, it must be created prior to
 * calling this function
 *
 * returns 0 on failure and 1 on success
 */

PUBLIC int
NET_AddLOSubmitDataToURLStruct(LO_FormSubmitData * sub_data, 
                               URL_Struct * url_struct)
{
#if 0
    int32 i;
    int32 total_size;
    char *end, *tmp_ptr;
    char *encoding;
    char *target;
    char **name_array;
    char **value_array;
    uint8 *type_array;
    uint8 *encoding_array;
    char *esc_string;
    int32 len = 0;

    if(!sub_data || !url_struct)
        return(0);

    if(sub_data->method == FORM_METHOD_GET)
        url_struct->method = URL_GET_METHOD;
    else
        url_struct->method = URL_POST_METHOD;
    if (!PL_strncasecmp(url_struct->address, "mailto:", 7)) {
        url_struct->mailto_post = TRUE;
    }

    /* free any previous url_struct->post_data
     */
    FREEIF(url_struct->post_data);

    PA_LOCK(name_array,  char**, sub_data->name_array);
    PA_LOCK(value_array, char**, sub_data->value_array);
    PA_LOCK(type_array,  uint8*, sub_data->type_array);
    PA_LOCK(encoding_array, uint8*, sub_data->encoding_array);
    PA_LOCK(encoding, char*, sub_data->encoding);

    /* free any previous target
     */
    FREEIF(url_struct->window_target);
    PA_LOCK(target, char*, sub_data->window_target);
    if (target == NULL)
        url_struct->window_target = NULL;
    else
        url_struct->window_target = PL_strdup (target);

    FREEIF (url_struct->post_headers);

    /* If we're posting to mailto, then generate the full complement
       of mail headers; and allow the url to specify additional headers
       as well. */
#if defined(OLD_MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
    if (!PL_strncasecmp(url_struct->address, "mailto:", 7))
      {
#ifdef MOZILLA_CLIENT
        int status;
        char *new_url = 0;
        char *headers = 0;

        status = MIME_GenerateMailtoFormPostHeaders (url_struct->address,
                                                     url_struct->referer,
                                                     &new_url, &headers);
        if (status < 0)
          {
            FREEIF (new_url);
            FREEIF (headers);
            return status;
          }
        PR_ASSERT (new_url);
        PR_ASSERT (headers);
        url_struct->address_modified = TRUE;
        PR_Free (url_struct->address);
        url_struct->address = new_url;
        url_struct->post_headers = headers;
#else
        PR_ASSERT(0);
#endif /* MOZILLA_CLIENT */
      }
#endif /* OLD_MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */

    if(encoding && !PL_strcasecmp(encoding, "text/plain"))
      {
        char *tmpfilename;
        char buffer[512];
        XP_File fp;

        /* always use post for this encoding type
         */
        url_struct->method = URL_POST_METHOD;

        /* write all the post data to a file first
         * so that we can send really big stuff
         */
        tmpfilename = WH_TempName (xpFileToPost, "nsform");
        if (!tmpfilename) return 0;
        fp = NET_XP_FileOpen (tmpfilename, xpFileToPost, XP_FILE_WRITE_BIN);
        if (!fp) {
            PR_Free(tmpfilename);
            return 0;
        }

        if (url_struct->post_headers)
          {
            len = NET_XP_FileWrite(url_struct->post_headers,
                         PL_strlen (url_struct->post_headers),
                         fp);
            PR_Free (url_struct->post_headers);
            url_struct->post_headers = 0;
            if (len < 0)
            {
                NET_XP_FileClose(fp);
                return 0;
            }
          }

        PL_strcpy (buffer,
                   "Content-type: text/plain" CRLF
                   "Content-Disposition: inline; form-data" CRLF CRLF);
        len = NET_XP_FileWrite(buffer, PL_strlen(buffer), fp);

        for(i=0; (len >= 0) && (i < sub_data->value_cnt); i++)
          {
            if(name_array[i])
                NET_XP_FileWrite(name_array[i], PL_strlen(name_array[i]), fp);
            NET_XP_FileWrite("=", 1, fp);
            if(value_array[i])
                NET_XP_FileWrite(value_array[i], PL_strlen(value_array[i]), fp);
            len = NET_XP_FileWrite(CRLF, 2, fp);
          }
        NET_XP_FileClose(fp);
    
        StrAllocCopy(url_struct->post_data, tmpfilename);
        PR_Free(tmpfilename);
        if (len < 0)
            return 0;
        url_struct->post_data_is_file = TRUE;
      }
    else if(encoding && !PL_strcasecmp(encoding, "multipart/form-data"))
      {
        /* encoding using a variant of multipart/mixed
         * and add files to it as well
         */
        char *tmpfilename;
        char separator[80];
        char buffer[512];
        XP_File fp;
        int boundary_len;
        int cont_disp_len;
        NET_cinfo * ctype;


        /* always use post for this encoding type
         */
        url_struct->method = URL_POST_METHOD;

        /* write all the post data to a file first
         * so that we can send really big stuff
         */
        tmpfilename = WH_TempName (xpFileToPost, "nsform");
        if (!tmpfilename) return 0;
        fp = NET_XP_FileOpen (tmpfilename, xpFileToPost, XP_FILE_WRITE_BIN);
        if (!fp) {
            PR_Free(tmpfilename);
            return 0;
        }

        sprintf(separator, "---------------------------%d%d%d",
                rand(), rand(), rand());

        if(url_struct->post_headers)
          {
            len = NET_XP_FileWrite(url_struct->post_headers,
                        PL_strlen (url_struct->post_headers),
                        fp);
            PR_Free (url_struct->post_headers);
            url_struct->post_headers = 0;
            if (len < 0)
                return 0;
          }

        sprintf(buffer,
                "Content-type: multipart/form-data;"
                " boundary=%s" CRLF,
                separator);
        len = NET_XP_FileWrite(buffer, PL_strlen(buffer), fp);

#define CONTENT_DISPOSITION "Content-Disposition: form-data; name=\""
#define PLUS_FILENAME "\"; filename=\""
#define CONTENT_TYPE_HEADER "Content-Type: "
#define CONTENT_ENCODING_HEADER "Content-Encoding: "

        /* compute the content length */
        total_size = -2; /* start at negative 2 to disregard the
                          * CRLF that act as a header separator 
                          */
        boundary_len = PL_strlen(separator) + 6;
        cont_disp_len = PL_strlen(CONTENT_DISPOSITION);

        for(i=0; (len >= 0) && (i < sub_data->value_cnt); i++)
          {
            total_size += boundary_len;

            /* The size of the content-disposition line is hard
             * coded and must be modified any time you change
             * the sprintf in the next for loop
             */
            total_size += cont_disp_len;
            if(name_array[i])
                total_size += PL_strlen(name_array[i]);
            total_size += 5;  /* quote plus two CRLF's */

            if(type_array[i] == FORM_TYPE_FILE)
              {
                XP_StatStruct stat_entry;

                /* in this case we are going to send an extra
                 * ; filename="value_array[i]"
                 */
                total_size += PL_strlen(PLUS_FILENAME);
                if(value_array[i])
                {
                    /* only write the filename, not the whole path */
                    char * slash = PL_strrchr(value_array[i], '/');
                    if(slash)
                        slash++;
                    else
                        slash = value_array[i];
                    total_size += PL_strlen(slash);

#ifdef XP_MAC
                    if(encoding_array[i] == INPUT_TYPE_ENCODING_MACBIN)
                    {
                        /* add the size of the content type header */
                        /* NOTE - even though MacBinary is technically an encoding type we send
                                  it as a content type and skip the normal routine of trying
                                  to determine the actual file type
                        */
                        total_size += sizeof(CONTENT_TYPE_HEADER)-1;
                        total_size += sizeof(APPLICATION_MACBINARY)-1;
                        total_size += 2;  /* for the CRLF terminator */
                    }
                    else
#endif /* XP_MAC */
                    {

                        /* try and determine the content-type of the file
                         */
                        ctype = NET_cinfo_find_type(value_array[i]);

                        if(!ctype->is_default)
                        {
                            /* we have determined it's type. Add enough
                             * space for it
                             */
                            total_size += sizeof(CONTENT_TYPE_HEADER)-1;
                            total_size += PL_strlen(ctype->type);
                            total_size += 2;  /* for the CRLF terminator */
                        }
                    }
                }
#ifdef XP_MAC
                if(encoding_array[i] == INPUT_TYPE_ENCODING_MACBIN)
                {
                    /* figure out the size of the macbinary encoded file
                     * and add it to total_size
                     */
                    if(value_array[i] && *value_array[i])
                        if(-1 != MB_Stat (value_array[i], &stat_entry, xpFileToPost))
                            total_size += stat_entry.st_size;
                }
                else
#endif /* XP_MAC */
                {
                    /* if the type is a FILE type then we 
                    * need to stat the file to get the size
                    */
                    if(value_array[i] && *value_array[i])
                        if(-1 != NET_XP_Stat (value_array[i], &stat_entry, xpFileToPost))
                            total_size += stat_entry.st_size;

                    /* if we can't stat the file just add zero */
                }
              }
            else
              {
                if(value_array[i])
                    total_size += PL_strlen(value_array[i]);
              }
          }
        /* add the size of the last separator plus
         * two for the extra two dashes
         */
        total_size += boundary_len+2;

        sprintf(buffer, "Content-Length: %ld%s", total_size, CRLF);
        len = NET_XP_FileWrite(buffer, PL_strlen(buffer), fp);

        for(i=0; (len >= 0) && (i < sub_data->value_cnt); i++)
          {
            sprintf(buffer, "%s--%s%s", CRLF, separator, CRLF);
            NET_XP_FileWrite(buffer, PL_strlen(buffer), fp);
            
            /* WARNING!!! If you change the size of any of the
             * sprintf's here you must change the size
             * in the counting for loop above
             */
            NET_XP_FileWrite(CONTENT_DISPOSITION, cont_disp_len, fp);
            if(name_array[i])
                NET_XP_FileWrite(name_array[i], PL_strlen(name_array[i]), fp);

            if(type_array[i] == FORM_TYPE_FILE)
              {
                NET_XP_FileWrite(PLUS_FILENAME, PL_strlen(PLUS_FILENAME), fp);
                if(value_array[i])
                  {
                    /* only write the filename, not the whole path */
                    char * slash = PL_strrchr(value_array[i], '/');
                    if(slash)
                        slash++;
                    else
                        slash = value_array[i];
                    NET_XP_FileWrite(slash, PL_strlen(slash), fp);

                  }
              }
            /* end the content disposition line */
            len = NET_XP_FileWrite("\"" CRLF, PL_strlen("\"" CRLF), fp);

            if(type_array[i] == FORM_TYPE_FILE && value_array[i])
            {
#ifdef XP_MAC
                if(encoding_array[i] == INPUT_TYPE_ENCODING_MACBIN)
                {
                    /* add the content_type header */
                    NET_XP_FileWrite(CONTENT_TYPE_HEADER, 
                                        PL_strlen(CONTENT_TYPE_HEADER),
                                        fp);
                    NET_XP_FileWrite(APPLICATION_MACBINARY, 
                                        PL_strlen(APPLICATION_MACBINARY),
                                        fp);
                    len = NET_XP_FileWrite(CRLF, PL_strlen(CRLF), fp);
                }
                else
#endif /* XP_MAC */
                {
                    /* try and determine the content-type of the file
                     */
                    ctype = NET_cinfo_find_type(value_array[i]);

                    if(!ctype->is_default)
                    {
                        /* we have determined it's type. Send the
                        * content-type
                        */
                        NET_XP_FileWrite(CONTENT_TYPE_HEADER, 
                                            PL_strlen(CONTENT_TYPE_HEADER),
                                            fp);
                        NET_XP_FileWrite(ctype->type,
                                            PL_strlen(ctype->type),
                                            fp);
                        len = NET_XP_FileWrite(CRLF, PL_strlen(CRLF), fp);
                    }
                }
            }

            /* end the header */
            len = NET_XP_FileWrite(CRLF, PL_strlen(CRLF), fp);

            /* send the value of the form field */

            /* if the type is a FILE type, send the whole file,
             * the filename is in the value field
             */
            if(type_array[i] == FORM_TYPE_FILE)
              {
#ifdef XP_MAC
                if(encoding_array[i] == INPUT_TYPE_ENCODING_MACBIN)
                {
                    MB_FileSpec mbFile;
                    int32       size;
                    OSErr       theErr;
                    
                    if (value_array[i] && *value_array[i])
                    {
                        theErr = MB_Open(value_array[i], &mbFile);

                        if (theErr == noErr)
                          {
                            while((size = MB_Read(NET_Socket_Buffer, 
                                                      NET_Socket_Buffer_Size, 
                                                      &mbFile)) != 0)
                              {
                                NET_XP_FileWrite(NET_Socket_Buffer, size, fp);
                              }
                            MB_Close(&mbFile);
                          }
                    }
                }
                else
#endif /* XP_MAC */
                {
                    XP_File ext_fp=0;
                    int32 size;

                    if(value_array[i] && *value_array[i])
                        ext_fp = NET_XP_FileOpen(value_array[i], 
                                        xpFileToPost, 
                                        XP_FILE_READ_BIN);


                    if(ext_fp)
                      {
                        while((size = NET_XP_FileRead(NET_Socket_Buffer, 
                                                  NET_Socket_Buffer_Size, 
                                                  ext_fp)) != 0)
                          {
                            NET_XP_FileWrite(NET_Socket_Buffer, size, fp);
                          }
                        NET_XP_FileClose(ext_fp);
                      }
                 }
              }
            else
              {
                if(value_array[i])
                    NET_XP_FileWrite(value_array[i], PL_strlen(value_array[i]), fp);
              }
          }

        sprintf(buffer, "%s--%s--%s", CRLF, separator, CRLF);
        NET_XP_FileWrite(buffer, PL_strlen(buffer), fp);

        NET_XP_FileClose(fp);
    
        StrAllocCopy(url_struct->post_data, tmpfilename);
        PR_Free(tmpfilename);
        url_struct->post_data_is_file = TRUE;

      }
    else
      {
        total_size=1; /* start at one for the terminator char */

        /* find out how much space we need total
         *
         * and also convert all spaces to pluses at the same time
         */
        for(i=0; i<sub_data->value_cnt; i++)
          {
            total_size += NET_EscapedSize(name_array[i], URL_XPALPHAS);
            total_size += NET_EscapedSize(value_array[i], URL_XPALPHAS);
            total_size += 2;  /* & = */
          }

        if(sub_data->method == FORM_METHOD_GET)
          {
            char *punc;

            if(!url_struct->address)
                return(0);

            /* get rid of ? or # in the url string since we are adding it
             */
            punc = PL_strchr(url_struct->address, '?');
            if(punc)
               *punc = '\0';  /* terminate here */
            punc = PL_strchr(url_struct->address, '#');
            if(punc)
               *punc = '\0';  /* terminate here */

            /* add the size of the url plus one for the '?'
             */
            total_size += PL_strlen(url_struct->address)+1;
          }

        url_struct->post_data = (char *) PR_Malloc(total_size);

        if(!url_struct->post_data)
          {
            PA_UNLOCK(sub_data->name_array);
            PA_UNLOCK(sub_data->value_array);
            PA_UNLOCK(sub_data->type_array);
            PA_UNLOCK(sub_data->encoding);
            return(0);
          }

        if(sub_data->method == FORM_METHOD_GET)
          {
            end = url_struct->post_data;
            for(tmp_ptr = url_struct->address; *tmp_ptr != '\0'; tmp_ptr++)
                *end++ = *tmp_ptr;

            /* add the '?'
             */
            *end++ = '?';

            /* swap the post data and address data */
            FREE(url_struct->address);
            url_struct->address = url_struct->post_data;
            url_struct->post_data = 0;

            /* perform  hack:
             * To be compatible with old pre-form 
             * indexes, other web browsers had a hack
             * wherein if a form was being submitted, 
             * and its method was get, and it
             * had only one name/value pair, and the 
             * name of that pair was "isindex", then
             * it would create the query url
             *
             * URL?value instead of URL?name=value
             */
            if(sub_data->value_cnt == 1 && !PL_strcasecmp(name_array[0], "isindex"))
              {
                if(value_array && value_array[0])
                    PL_strcpy(end, NET_Escape(value_array[0], URL_XPALPHAS));
                else
                    *end = '\0';
                PA_UNLOCK(sub_data->name_array);
                PA_UNLOCK(sub_data->value_array);
                PA_UNLOCK(sub_data->type_array);
                PA_UNLOCK(sub_data->encoding);
                return(1);
              }
            
            /* the end ptr is still set to the correct address!
             */

          }
        else
          {
            StrAllocCat(url_struct->post_headers,
                        "Content-type: application/x-www-form-urlencoded" CRLF);
    
            if(!url_struct->post_headers)
              {
                FREE_AND_CLEAR(url_struct->post_data);
                PA_UNLOCK(sub_data->name_array);
                PA_UNLOCK(sub_data->value_array);
                PA_UNLOCK(sub_data->type_array);
                PA_UNLOCK(sub_data->encoding);
                return(0);
              }

            end = url_struct->post_data;

          }

        /* build the string 
         */
        for(i=0; i<sub_data->value_cnt; i++)
          {
            /* add the name
             */
            esc_string = NET_Escape(name_array[i], URL_XPALPHAS);
            if(esc_string)
              {
                for(tmp_ptr = esc_string; *tmp_ptr != '\0'; tmp_ptr++)
                    *end++ = *tmp_ptr;
                PR_Free(esc_string);
              }

            /* join the name and value with a '='
             */
            *end++ = '=';

            /* add the value
             */
            esc_string = NET_Escape(value_array[i], URL_XPALPHAS);
            if(esc_string)
              {
                for(tmp_ptr = esc_string; *tmp_ptr != '\0'; tmp_ptr++)
                    *end++ = *tmp_ptr;
                PR_Free(esc_string);
              }

            /* join pairs with a '&' 
             * make sure there is another pair before adding
             */
            if(i+1 < sub_data->value_cnt)
                *end++ = '&';
          }
        
        /* terminate the string
         */
        *end = '\0';

        if(sub_data->method == FORM_METHOD_POST)
          {
            char buffer[64];
            url_struct->post_data_size = PL_strlen(url_struct->post_data);
            sprintf(buffer, 
                       "Content-length: %ld" CRLF, 
                       url_struct->post_data_size);
            StrAllocCat(url_struct->post_headers, buffer);

#ifdef ADD_EXTRA_CRLF_TO_POSTS
            /* munge for broken CGIs.  Add an extra CRLF to the 
             * post data.
             */
            BlockAllocCat(url_struct->post_data, 
                          url_struct->post_data_size,
                          CRLF, 
                          3);
            /* don't add the eol terminator */
            url_struct->post_data_size += 2;
#endif /* ADD_EXTRA_CRLF_TO_POSTS */

          }
      }

    PA_UNLOCK(sub_data->name_array);
    PA_UNLOCK(sub_data->value_array);
    PA_UNLOCK(sub_data->type_array);
    PA_UNLOCK(sub_data->encoding);

    return(1); /* success */
#endif
    return 0;
}

PUBLIC int
NET_AddCoordinatesToURLStruct(URL_Struct * url_struct, int32 x_coord, int32 y_coord)
{

    if(url_struct->address)
      {
        char buffer[32];

        sprintf(buffer, "?%ld,%ld", x_coord, y_coord);

        /* get rid of ? or # in the url string since we are adding it
         */
#undef STRIP_SEARCH_DATA_FROM_ISMAP_URLS
#ifdef STRIP_SEARCH_DATA_FROM_ISMAP_URLS
      {
        char *punc;
        punc = PL_strchr(url_struct->address, '?');
        if(punc)
            *punc = '\0';  /* terminate here */
        punc = PL_strchr(url_struct->address, '#');
        if(punc)
            *punc = '\0';  /* terminate here */
      }
#endif /* STRIP_SEARCH_DATA_FROM_ISMAP_URLS */

        StrAllocCat(url_struct->address, buffer);
      }

    return(1); /* success */
}

/* FREE_AND_CLEAR will free a pointer if it is non-zero and
 * then set it to zero
 */
MODULE_PRIVATE void 
NET_f_a_c (char **pointer)
{
    if(*pointer) {
    PR_Free(*pointer);
        *pointer = 0;
    }
}

/* recognizes URLs and their types.  Returns 0 (zero) if
 * it is unrecognized.
 */
PUBLIC int 
NET_URL_Type (CONST char *URL)
{
    /* Protect from SEGV */
    /* Rhapsody compiler needed to break up this || statement into two if's. */
    if (!URL)
        return(0);
    if (URL && *URL == '\0')
        return(0);

    switch(*URL) {
    case 'a':
    case 'A':
        HG83787
        if(!PL_strncasecmp(URL,"about:",6))
            return(ABOUT_TYPE_URL);
        else if(!PL_strncasecmp(URL,"addbook:",8))
            return(ADDRESS_BOOK_TYPE_URL);
        else if (!PL_strncasecmp(URL, "addbook-ldap", 12)) /*no colon includes addbook-ldaps:*/
            return(ADDRESS_BOOK_LDAP_TYPE_URL);
        break;

    case 'd':
    case 'D':
        if(!PL_strncasecmp(URL,"data:",5))
            return(DATA_TYPE_URL);
        break;

    case 'c':
    case 'C':
        if(!PL_strncasecmp(URL,"castanet:",9))
            return(0);
        break;

    case 'f':
    case 'F':
        if(!PL_strncasecmp(URL,"ftp:",4))
            return(FTP_TYPE_URL);
        else if(!PL_strncasecmp(URL,"file:",5))
            return(FILE_TYPE_URL);
        break;
    case 'g':
    case 'G':
        if(!PL_strncasecmp(URL,"gopher:",7)) 
            return(GOPHER_TYPE_URL);
        break;
    case 'h':
    case 'H':
        if(!PL_strncasecmp(URL,"http:",5))
            return(HTTP_TYPE_URL);
        HG73678
        break;
    case 'i':
    case 'I':
        if(!PL_strncasecmp(URL,"internal-gopher-",16))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-news-",14))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-edit-",14))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-attachment-",20))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-sa-",12))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-smime-",15))
            return(INTERNAL_IMAGE_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-dialog-handler",23))
            return(HTML_DIALOG_HANDLER_TYPE_URL);
        else if(!PL_strncasecmp(URL,"internal-panel-handler",22))
            return(HTML_PANEL_HANDLER_TYPE_URL);
        HG84378
        else if(!PL_strncasecmp(URL,"IMAP:",5))
            return(IMAP_TYPE_URL);
        break;
    case 'j':
    case 'J':
        if(!PL_strncasecmp(URL, "javascript:",11))
            return(MOCHA_TYPE_URL);
        break;
    case 'l':
    case 'L':
        if(!PL_strncasecmp(URL, "livescript:",11))
            return(MOCHA_TYPE_URL);
        else if (!PL_strncasecmp(URL, "ldap:",5))
            return(LDAP_TYPE_URL);
        HG84772
        break;
    case 'm':
    case 'M':
        if(!PL_strncasecmp(URL,"mailto:",7)) 
            return(MAILTO_TYPE_URL);
        else if(!PL_strncasecmp(URL,"mailbox:",8))
            return(MAILBOX_TYPE_URL);
        else if(!PL_strncasecmp(URL, "mocha:",6))
            return(MOCHA_TYPE_URL);
        break;
    case 'n':
    case 'N':
        if(!PL_strncasecmp(URL,"news:",5))
            return(NEWS_TYPE_URL);
        else if(!PL_strncasecmp(URL,"nfs:",4))
            return(NFS_TYPE_URL);
        else if(!PL_strncasecmp(URL, NETHELP_URL_PREFIX, sizeof(NETHELP_URL_PREFIX)-1))
            return(NETHELP_TYPE_URL);
        break;
    case 'p':
    case 'P':
        if(!PL_strncasecmp(URL,"pop3:",5))
            return(POP3_TYPE_URL);
        break;
    case 'r':
    case 'R':
        if(!PL_strncasecmp(URL,"rlogin:",7))
            return(RLOGIN_TYPE_URL);
        break;
    case 's':
    case 'S':
        if(!PL_strncasecmp(URL,"sockstub:",9))
            return(SOCKSTUB_TYPE_URL);
        else if(!PL_strncasecmp(URL,"snews:",6))
            return(NEWS_TYPE_URL);
        else if (!PL_strncasecmp(URL,"search-libmsg:",14))
            return(MSG_SEARCH_TYPE_URL);
    case 't':
    case 'T':
        if(!PL_strncasecmp(URL,"telnet:",7))
            return(TELNET_TYPE_URL);
        else if(!PL_strncasecmp(URL,"tn3270:",7))
            return(TN3270_TYPE_URL);
        break;
    case 'u':
    case 'U':
        if(!PL_strncasecmp(URL,"URN:",4))
            return(URN_TYPE_URL);
        break;
    case 'v':
    case 'V':
        if(!PL_strncasecmp(URL, VIEW_SOURCE_URL_PREFIX, 
                              sizeof(VIEW_SOURCE_URL_PREFIX)-1))
            return(VIEW_SOURCE_TYPE_URL);
        break;
    case 'w':
    case 'W':
        if(!PL_strncasecmp(URL,"wais:",5))
            return(WAIS_TYPE_URL);
        if(!PL_strncasecmp(URL,"wysiwyg:",8))
            return(WYSIWYG_TYPE_URL);
        break;
    }

    /* no type match :( */
    return(0);
}

PUBLIC void
NET_PlusToSpace(char *str)
{
    for (; *str != '\0'; str++)
        if (*str == '+')
            *str = ' ';
}

/* if the url is a local file this function returns
 * the portion of the url that represents the
 * file path as a malloc'd string.  If the
 * url is not a local file url this function
 * returns NULL
 */
PRIVATE char *
net_return_local_file_part_from_url(char *address)
{
    char *host;
#ifdef XP_WIN
    char *new_address;
    char *rv;
#endif
    
    if (address == NULL)
        return NULL;

    /* imap urls are never local */
    if (!PL_strncasecmp(address,"mailbox://",10))
        return NULL;

    /* mailbox url's are always local, but don't always point to a file */
    if(!PL_strncasecmp(address, "mailbox:", 8))
    {
        char *filename = NET_ParseURL(address, GET_PATH_PART);
        if (!filename)
            filename = PL_strdup("");
        return filename;
    }

    if(PL_strncasecmp(address, "file:", 5))
        return(NULL);
    
    host = NET_ParseURL(address, GET_HOST_PART);

    if(!host || *host == '\0' || !PL_strcasecmp(host, "LOCALHOST"))
      {
        PR_FREEIF(host);
        return(NET_UnEscape(NET_ParseURL(address, GET_PATH_PART)));
      }

#ifdef XP_WIN
    /* get the address minus the search and hash data */
    new_address = NET_ParseURL(address, 
                            GET_PROTOCOL_PART | GET_HOST_PART | GET_PATH_PART);
    NET_UnEscape(new_address);

    /* check for local drives as hostnames
     */
    if(PL_strlen(host) == 2
        && isalpha(host[0]) 
        && (host[1] == '|' || host[1] == ':'))
      {
        PR_Free(host);
        /* skip "file:/" */
        rv = PL_strdup(new_address+6);
        PR_Free(new_address);
        return(rv);
      }

    if(1)
      {
        XP_StatStruct       stat_entry;
        /* try stating the url just in case since
         * the local file system can
         * have url's of the form \\prydain\dist
         */
        if(-1 != NET_XP_Stat(address+5, &stat_entry, xpURL))
          {
            PR_Free(host);
            /* skip "file:" */
            rv = PL_strdup(address+5);
            PR_Free(new_address);
            return(rv);
          }
      }
#endif /* XP_WIN */

    PR_Free(host);

    return(NULL);
}

/* returns TRUE if the url is a local file
 * url
 */
PUBLIC XP_Bool
NET_IsLocalFileURL(char *address)
{
    char * cp = net_return_local_file_part_from_url(address);

    if(cp)
      {
        PR_Free(cp);
        return(TRUE);
      }
    else
      {
        return(FALSE);
      }
}

static int
net_buffer_output_fn ( const char *buf, int32 size, void *closure)
{
  BufferStruct *bs = (BufferStruct *) closure;
  /* if the size greater or equal to the available buffer size
   * reallocate the buffer
   */
  PR_ASSERT (buf && bs && size > 0);
  if ( !buf || !bs || size <= 0 )
    return -1;

  if (size >= bs->size - bs->pos)
    {
      int32 len;
      char *newBuffer;
      
      len = (bs->size << 1) - bs->pos + size + 1; /* null terminated */
      if (bs->buffer)
        newBuffer = PR_Realloc (bs->buffer, len);
      else
        newBuffer = PR_Malloc(len);
      if (!newBuffer)
        return MK_OUT_OF_MEMORY;
      memset(newBuffer+bs->pos, 0, len-bs->pos);
      bs->size = len;
      bs->buffer = newBuffer;
    }
  memcpy (bs->buffer+bs->pos, buf, size);
  bs->pos += size;
  return 0;
}

PRIVATE int32
net_URNProtoLoad(ActiveEntry *ce)
{
    char buffer[256];

    PL_strcpy(buffer, XP_GetString(XP_ALERT_URN_USEHTTP));
    PL_strncat(buffer, ce->URL_s->address, 150);
    buffer[255] = '\0'; /* in case strncat doesn't add one */
    FE_Alert(ce->window_id, buffer);

    return -1;
}

PRIVATE int32
net_URNProtoStub(ActiveEntry *ce)
{
    PR_ASSERT(0);
    return -1;
}

PRIVATE void
net_URNProtoCleanupStub(void)
{
}

/* a stub function for URN protocol converter.
 * right now we only proxy URN's
 * if URN's ever get worked on move this to another file
 */
PUBLIC void
NET_InitURNProtocol(void)
{
    static NET_ProtoImpl urn_proto_impl;

    urn_proto_impl.init = net_URNProtoLoad;
    urn_proto_impl.process = net_URNProtoStub;
    urn_proto_impl.interrupt = net_URNProtoStub;
    urn_proto_impl.cleanup = net_URNProtoCleanupStub;

    NET_RegisterProtocolImplementation(&urn_proto_impl, URN_TYPE_URL);
}

PRIVATE int32
net_NFSProtoLoad(ActiveEntry *ce)
{
        char buffer[256];

        PL_strcpy(buffer, XP_GetString(XP_ALERT_NFS_USEHTTP));
        PL_strncat(buffer, ce->URL_s->address, 150);
        buffer[255] = '\0'; /* in case strncat doesn't add one */
        FE_Alert(ce->window_id, buffer);

        return -1;
}

PRIVATE int32
net_NFSProtoStub(ActiveEntry *ce)
{
    PR_ASSERT(0);
    return -1;
}

PRIVATE void
net_NFSProtoCleanupStub(void)
{
}

/* a stub function for NFS protocol converter.
 * right now we only proxy NFS's
 * if NFS's ever get worked on move this to another file
 */
PUBLIC void
NET_InitNFSProtocol(void)
{
    static NET_ProtoImpl nfs_proto_impl;

    nfs_proto_impl.init = net_NFSProtoLoad;
    nfs_proto_impl.process = net_NFSProtoStub;
    nfs_proto_impl.interrupt = net_NFSProtoStub;
    nfs_proto_impl.cleanup = net_NFSProtoCleanupStub;

    NET_RegisterProtocolImplementation(&nfs_proto_impl, NFS_TYPE_URL);
}

PRIVATE int32
net_WAISProtoLoad(ActiveEntry *ce)
{
    char * alert = NET_ExplainErrorDetails(MK_NO_WAIS_PROXY);

    FE_Alert(ce->window_id, alert);
    FREE(alert);

    return -1;
}

PRIVATE int32
net_WAISProtoStub(ActiveEntry *ce)
{
    PR_ASSERT(0);
    return -1;
}

PRIVATE void
net_WAISProtoCleanupStub(void)
{
}

/* a stub function for WAIS protocol converter.
 * right now we only proxy WAIS's
 * if WAIS's ever get worked on move this to another file
 */
PUBLIC void
NET_InitWAISProtocol(void)
{
    static NET_ProtoImpl wais_proto_impl;

    wais_proto_impl.init = net_WAISProtoLoad;
    wais_proto_impl.process = net_WAISProtoStub;
    wais_proto_impl.interrupt = net_WAISProtoStub;
    wais_proto_impl.cleanup = net_WAISProtoCleanupStub;

    NET_RegisterProtocolImplementation(&wais_proto_impl, WAIS_TYPE_URL);
}
#if defined(OLD_MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)

PUBLIC char *
NET_Base64Encode (char *src, int32 srclen)
{
  BufferStruct bs;
  MimeEncoderData *encoder_data = NULL;

  PR_ASSERT (src);
  if (!src)
    return NULL;
  else if (srclen == 0)
    return PL_strdup("");

  memset (&bs, 0, sizeof (BufferStruct));
  encoder_data = MimeB64EncoderInit(net_buffer_output_fn, (void *) &bs);
  if (!encoder_data)
    return NULL;

  if (MimeEncoderWrite(encoder_data, src, srclen) < 0)
    {
      MimeEncoderDestroy(encoder_data, FALSE);
      PR_FREEIF(bs.buffer);
      return NULL;
    }
  
  MimeEncoderDestroy(encoder_data, FALSE);
  /* caller must free the returned pointer to prevent
   * memory leak problem.
   */
  return bs.buffer;
}



PUBLIC char *
NET_Base64Decode (char *src, 
                  int32 srclen)
{
  BufferStruct bs;
  MimeDecoderData *decoder_data = NULL;

  PR_ASSERT (src);
  if (!src)
    return NULL;
  else if (srclen == 0)
    return PL_strdup("");

  memset (&bs, 0, sizeof (BufferStruct));
  decoder_data = MimeB64DecoderInit(net_buffer_output_fn, (void *) &bs);
  if (!decoder_data)
    return NULL;

  if (MimeDecoderWrite(decoder_data, src, srclen) < 0)
    {
      MimeDecoderDestroy(decoder_data, FALSE);
      PR_FREEIF(bs.buffer);
      return NULL;
    }
  
  MimeDecoderDestroy(decoder_data, FALSE);
  /* caller must free the returned pointer to prevent
   * memory leak problem.
   */
  return bs.buffer;
}

#endif /* OLD_MOZ_MAIL_NEWS || MOZ_MAIL_COMPOSE */

/* A utility function to fetch a file from cache right away, 
 * and update it (from the original server) after its used.
 * Used in/Required by Mr. R.D.F. Guha 
 *
 * Note that if the item is not in the cache, this defaults
 * to retrieving it from the server.
 *
 * Variables and returns : see NET_GetURL in include/net.h
 *
 * Warning: Possibility of a stale copy!
 *
 */
PUBLIC int
NET_GetURLQuick (URL_Struct * URL_s,
        FO_Present_Types output_format,
        MWContext * context,
        Net_GetUrlExitFunc* exit_routine)
{
#ifdef NU_CACHE
    if (!CacheManager_Contains(URL_s->address))
#else
    if (!NET_FindURLInMemCache(URL_s, context) &&
        !NET_FindURLInExtCache(URL_s, context))
#endif
    {
        /* default */
        return NET_GetURL(
            URL_s, 
            output_format, 
            context, 
            exit_routine);
    }
    else
    {
        /* Only from cache */
        int status =
            NET_GetURL(
                URL_s,
                FO_ONLY_FROM_CACHE || output_format,
                context,
                exit_routine);

        /* Update request */
        NET_GetURL(
            URL_s,
            FO_CACHE_ONLY,
            context,
            NULL);
        
        return status;
    }
}


