/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* JAR internal routines */

#include "nspr.h"

/* definitely required */
#include "certdb.h"
#include "key.h"
#include "base64.h"

extern CERTCertDBHandle *JAR_open_database (void);

extern int JAR_close_database (CERTCertDBHandle *certdb);

extern int jar_close_key_database (void *keydb);

extern void *jar_open_key_database (void);

extern JAR_Signer *JAR_new_signer (void);

extern void JAR_destroy_signer (JAR_Signer *signer);

extern JAR_Signer *jar_get_signer (JAR *jar, char *basename);

extern int jar_append (ZZList *list, int type,
   char *pathname, void *data, size_t size);

/* Translate fopen mode arg to PR_Open flags and mode */
PRFileDesc*
JAR_FOPEN_to_PR_Open(const char *name, const char *mode);


#define ADDITEM(list,type,pathname,data,size) \
  { int err; err = jar_append (list, type, pathname, data, size); \
    if (err < 0) return err; }

/* Here is some ugliness in the event it is necessary to link
   with NSPR 1.0 libraries, which do not include an FSEEK. It is 
   difficult to fudge an FSEEK into 1.0 so we use stdio. */

/* stdio */
#if 0
#define JAR_FILE FILE *
#define JAR_FOPEN(fn,mode) fopen(fn,mode)
#define JAR_FCLOSE fclose
#define JAR_FSEEK fseek
#define JAR_FREAD(fp,buf,siz) fread(buf,1,siz,fp)
#define JAR_FWRITE(fp,buf,siz) fwrite(buf,1,siz,fp) 
#endif

#if 0
/* nspr 1.0 suite */
#define JAR_FILE PRFileHandle
#define JAR_FOPEN(fn,mode) PR_OpenFile(fn,0,mode)
#define JAR_FCLOSE PR_CLOSE
#define JAR_FSEEK (no-equivalent)
#define JAR_FREAD PR_READ
#define JAR_FWRITE PR_WRITE
#endif

/* nspr 2.0 suite */
#define JAR_FILE PRFileDesc *
/* #define JAR_FOPEN(fn,mode) PR_Open(fn,0,0) */
#define JAR_FOPEN(fn,mode) JAR_FOPEN_to_PR_Open(fn,mode)
#define JAR_FCLOSE PR_Close
#define JAR_FSEEK PR_Seek
#define JAR_FREAD PR_Read
#define JAR_FWRITE PR_Write

#if 0
/* nav XP suite, note argument order */
#define JAR_FILE XP_File
#define JAR_FOPEN(fn,mode) XP_FileOpen(fn,xpURL,mode)
#define JAR_FCLOSE XP_FileClose
#define JAR_FSEEK XP_FileSeek
#define JAR_FREAD(fp,buf,siz) XP_FileRead(buf,siz,fp)
#define JAR_FWRITE(fp,buf,siz) XP_FileWrite(buf,siz,fp)
#endif

int jar_create_pk7
   (CERTCertDBHandle *certdb, void *keydb,
       CERTCertificate *cert, char *password, JAR_FILE infp, 
       JAR_FILE outfp);

