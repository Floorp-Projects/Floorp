#include "nsNetFile.h"
#include "prmem.h"
#include "nsString.h"
#include "plstr.h"
#include "prerror.h"

#ifndef XP_UNIX
#include "direct.h"
#endif

#include <time.h>

#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

char USER_DIR[_MAX_PATH];
char CACHE_DIR[_MAX_PATH];
char DEF_DIR[_MAX_PATH];

static NS_DEFINE_IID(kINetFileIID, NS_INETFILE_IID);

NS_IMPL_ISUPPORTS(nsNetFile, kINetFileIID);

typedef struct _nsFileAssoc {
    nsString name;
    nsString dirTok;
} nsFileAssoc;

// Comparison routine for the fileAssoc objects.
PR_IMPLEMENT(int)
PL_CompareFileAssoc(const void *v1, const void *v2)
{
    nsFileAssoc *a = (nsFileAssoc*)v1;
    nsFileAssoc *b = (nsFileAssoc*)v2;

    return *a->name == *b->name;
}

nsNetFile::nsNetFile() {
  NS_INIT_REFCNT();
#ifdef XP_PC
    mDirDel = '\\';
#elif XP_MAC
    mDirDel = ':';
#else
    mDirDel = '/';
#endif

    mTokDel = '%';

    mHTDirs = PL_NewHashTable(5, PL_HashString, PL_CompareStrings,
                                    PL_CompareStrings, nsnull, nsnull);

    mHTFiles = PL_NewHashTable(10, PL_HashString, PL_CompareStrings,
                                    PL_CompareFileAssoc, nsnull, nsnull);
}

PR_CALLBACK PRIntn
NS_RemoveStringEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  char *entry = (char*)he->value;

  NS_ASSERTION(entry != nsnull, "null entry");
  PR_Free(entry);
  return HT_ENUMERATE_REMOVE;
}

nsNetFile::~nsNetFile() {
    // Clear the hash table.
    PL_HashTableEnumerateEntries(mHTDirs, NS_RemoveStringEntries, 0);
    PL_HashTableDestroy(mHTDirs);

    PL_HashTableEnumerateEntries(mHTFiles, NS_RemoveStringEntries, 0);
    PL_HashTableDestroy(mHTFiles);
}

nsresult nsNetFile::GetFilePath(const char *aName, char **aRes) {
    nsString newPath, fileName;
    char *tmpString;
    char *path = nsnull, *locName = nsnull;
    char *tokBegin, *dirTok, *fileTok, *nextTok;
    nsFileAssoc *faFile = nsnull;


    NS_PRECONDITION( (aName != nsnull), "Null pointer.");

    locName = URLSyntaxToLocal(aName);
    if (!locName)
        return NS_ERROR_OUT_OF_MEMORY;

    // If we were handed an absolute path, spit it back.
    if ( (*locName == mDirDel)                     // mac and unix
         || 
         (*locName && (*(locName + 1) == ':') ) )     // pc
    {
        newPath = locName;
        PR_Free(locName);
        locName = nsnull;
        tmpString = newPath.ToNewCString();
        if (!tmpString)
            return NS_ERROR_OUT_OF_MEMORY;
        path = PL_strdup(tmpString);
        delete[] tmpString;
        tmpString = nsnull;

        *aRes = path;
        return NS_OK;
    }

    // First see if this has been explicitly registered
    faFile = (nsFileAssoc*)PL_HashTableLookup(mHTFiles, locName);
    if (faFile) {
        char *dir = nsnull;
        if (faFile->dirTok.Length()) {
            char *csDT;
            tmpString = faFile->dirTok.ToNewCString();
            if (!tmpString) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
            csDT = PL_strdup(tmpString);
            delete[] tmpString;
            tmpString = nsnull;

            dir = (char*)PL_HashTableLookup(mHTDirs, csDT);
            PR_Free(csDT);
        }
        // If it wasn't in the table give the file a default dir.
        if (!dir) {
            dir = (char*)PL_HashTableLookup(mHTDirs, DEF_DIR_TOK);
            NS_PRECONDITION( (dir != nsnull), "No default dir registered.");
            return NS_ERROR_FAILURE;
        }
        newPath = dir;
        if (dir[PL_strlen(path)-1] != mDirDel) {
            newPath.Append(mDirDel);
        }
        newPath.Append(faFile->name);
        tmpString = newPath.ToNewCString();
        *aRes = PL_strdup(tmpString);
        delete[] tmpString;
        return NS_OK;
    }

    // If we've gotten this far, this file wasn't registered, or the user wants
    // to use a different directory with the file.

    // Lookup the token(s) in our hash table(s).
    dirTok = nsnull;
    fileTok = nsnull;

    tokBegin = PL_strchr(locName, mTokDel);
    if (tokBegin) {
        nextTok = PL_strchr(tokBegin+1, mTokDel);
        if (nextTok) {
            // Check for more (could be another token or a filename).
            if (*(nextTok+1)) {
                // Set the first one as the dir.
                char tmp = *(nextTok+1);
                *(nextTok+1) = '\0';
                dirTok = PL_strdup(tokBegin);
                *(nextTok+1) = tmp;
                tokBegin = nextTok+1;
                nextTok = PL_strchr(tokBegin+1, mTokDel);
            }
            if (nextTok) {
                *(nextTok+1) = '\0';
                fileTok = PL_strdup(tokBegin);
                *(nextTok+1) = mTokDel;
            } else {
                // no file token
                fileTok = PL_strdup(tokBegin);
            }
        }
    }

    // If the user passed in a dir token, use it.
    if (dirTok) {
        char *path = (char*)PL_HashTableLookup(mHTDirs, dirTok);
        PR_Free(dirTok);
        // If it wasn't in the table, fail.
        if (!path) {
            PR_Free(fileTok);
            return NS_ERROR_FAILURE;
        }
        newPath = path;
        if (path[PL_strlen(path)-1] != mDirDel) {
            newPath.Append(mDirDel);
        }
    }

    if (fileTok) {
	faFile = (nsFileAssoc*)PL_HashTableLookup(mHTFiles, fileTok);
	if (faFile)
	    fileName = faFile->name;
	else
	    fileName = fileTok; // It wasn't a token after all;
    } else {
        fileName = (char *)aName;
    }

    newPath.Append(fileName);
    PR_Free(fileTok);
    tmpString = newPath.ToNewCString();
    *aRes = PL_strdup(tmpString);
    delete[] tmpString;
    PR_FREEIF(locName);
    return NS_OK;
}

nsresult nsNetFile::GetTemporaryFilePath(const char *aName, char **aRes) {

    return NS_OK;
}

nsresult nsNetFile::GetUniqueFilePath(const char *aName, char **aRes) {

    return NS_OK;
}

void nsNetFile::GenerateGlobalRandomBytes(void *aDest, size_t aLen) {
	/* This is a temporary implementation to avoid */
	/* the cache filename horkage. This needs to have a more */
	/* secure/free implementation here - Gagan */

	char* output=(char*)aDest;
	size_t i;
	srand((unsigned int) PR_IntervalToMilliseconds(PR_IntervalNow()));
	for (i=0;i<aLen; i++)
	{
		int t = rand();
		*output = (char) (t % 256);
		output++;
	}
}

#define MAX_PATH_LEN 512

#ifdef XP_UNIX

// Checked this in to fix the build. I have no idea where this lives
// on Unix. I have no idea if this implementation does the right thing
// or even works.
static void
_strrev(char* s)
{
    // Find the end of the string.
    char* p = s;
    while (*p)
        ++p;

    --p; // back to the last character

    // Now reverse the string: s and p will eventually meet in the middle.
    while (s < p) {
        char c = *p;
        *p = *s;
        *s = c;

        ++s;
        --p;
    }
}
#endif

nsresult nsNetFile::GetCacheFileName(char *aDirTok, char **aRes) {
    char *file_buf = nsnull;
    char *ext = ".MOZ";
    char *prefix = "M";
    PRStatus status;
    PRFileInfo statinfo;
    char *dir = (char*)PL_HashTableLookup(mHTDirs, aDirTok);

    file_buf = (char*)PR_Calloc(1, MAX_PATH_LEN);
    if (!dir)
        return NS_ERROR_FAILURE;


    //  We need to base our temporary file names on time, and not on sequential
    //    addition because of the cache not being updated when the user
    //    crashes and files that have been deleted are over written with
    //    other files; bad data.
    //  The 52 valid DOS file name characters are
    //    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'()
    //  We will only be using the first 32 of the choices.
    //
    //  Time name format will be M+++++++.MOZ
    //    Where M is the single letter prefix (can be longer....)
    //    Where +++++++ is the 7 character time representation (a full 8.3
    //      file name will be made).
    //    Where .MOZ is the file extension to be used.
    //
    //  In the event that the time requested is the same time as the last call
    //    to this function, then the current time is incremented by one,
    //    as is the last time called to facilitate unique file names.
    //  In the event that the invented file name already exists (can't
    //    really happen statistically unless the clock is messed up or we
    //    manually incremented the time), then the times are incremented
    //    until an open name can be found.
    //
    //  time_t (the time) has 32 bits, or 4,294,967,296 combinations.
    //  We will map the 32 bits into the 7 possible characters as follows:
    //    Starting with the lsb, sets of 5 bits (32 values) will be mapped
    //      over to the appropriate file name character, and then
    //      incremented to an approprate file name character.
    //    The left over 2 bits will be mapped into the seventh file name
    //      character.
    //
  
    int i_letter, i_timechars, i_numtries = 0;
    char ca_time[8];
    time_t this_call = (time_t)0;

    //  We have to base the length of our time string on the length
    //    of the incoming prefix....
    //
    i_timechars = 8 - PL_strlen(prefix);

    //  Infinite loop until the conditions are satisfied.
    //  There is no danger, unless every possible file name is used.
    //
    while(1)  {
        //    We used to use the time to generate this.
        //    Now, we use some crypto to avoid bug #47027
        GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));

        //  Convert the time into a 7 character string.
        //  Strip only the signifigant 5 bits.
        //  We'll want to reverse the string to make it look coherent
        //    in a directory of file names.
        //
        for(i_letter = 0; i_letter < i_timechars; i_letter++) {
            ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);

            //  Convert any numbers to their equivalent ascii code
            //
            if(ca_time[i_letter] <= 9)  {
                ca_time[i_letter] += '0';
            //  Convert the character to it's equivalent ascii code
            //
            } else  {
                ca_time[i_letter] += 'A' - 10;
            }
        }

        //  End the created time string.
        //
        ca_time[i_letter] = '\0';

        //  Reverse the time string.
        //
        _strrev(ca_time);

        //  Create the fully qualified path and file name.
        //
        sprintf(file_buf, "%s\\%s%s%s", dir, prefix, ca_time, ext);

        //  Determine if the file exists, and mark that we've tried yet
        //    another file name (mark to be used later).
        //  
        //  Use the system call instead of XP_Stat since we already
        //  know the name and we don't want recursion
        //
        status = PR_GetFileInfo(file_buf, &statinfo);
        i_numtries++;

        //  If it does not exists, we are successful, return the name.
        //
        if(status == PR_FAILURE)  {
            /* don't generate a directory as part of the 
             * cache temp names.  When the cache file name
             * is passed into the other XP_File functions
             * we will append the cache directory to the name
             * to get the complete path.
             * This will allow the cache to be moved around
             * and for netscape to be used to generate external
             * cache FAT's.  :lou
             */
            sprintf(file_buf, "%s%s%s", prefix, ca_time, ext);

            *aRes = file_buf;
            break;
        }
    } // End while

    return NS_OK;
}

#if 0

// Here for reference.
PUBLIC char *
xp_TempFileName(int type, const char * request_prefix, const char * extension,
				char* file_buf)
{
  const char * directory = NULL; 
  char * ext = NULL;           // file extension if any
  char * prefix = NULL;        // file prefix if any
  XP_Bool bDirSlash = FALSE;
  
  XP_StatStruct statinfo;
  int status;     

  //
  // based on the type of request determine what directory we should be
  //   looking into
  //  
  switch(type) {
    case xpCache:
        directory = theApp.m_pCacheDir;
        ext = ".MOZ";
        prefix = CACHE_PREFIX;
        break;
#ifdef OLD_MOZ_MAIL_NEWS        
	case xpSNewsRC:
	case xpNewsRC:
    case xpNewsgroups:
    case xpSNewsgroups:
    case xpTemporaryNewsRC:
        directory = g_MsgPrefs.m_csNewsDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
        break;
	case xpMailFolderSummary:
	case xpMailFolder:
		directory = g_MsgPrefs.m_csMailDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
		break;
	case xpAddrBook:
		//changed jonm-- to support multi-profile
		//directory = theApp.m_pInstallDir->GetCharValue();
		directory = (const char *)theApp.m_UserDirectory;
		if ((request_prefix == 0) || (XP_STRLEN (request_prefix) == 0))
			prefix = "abook";
		ext = ".nab";
		break;
#endif // OLD_MOZ_MAIL_NEWS
	case xpCacheFAT:
		directory = theApp.m_pCacheDir;
		prefix = "fat";
		ext = "db";
		break;
	case xpJPEGFile:
        directory = theApp.m_pTempDir;
		ext = ".jpg";
        prefix = (char *)request_prefix;
		break;
	case xpPKCS12File:
		directory = theApp.m_pTempDir;
		ext = ".p12";
		prefix = (char *)request_prefix;
		break;
    case xpURL:
      {
         if (request_prefix)
         {
            if ( XP_STRRCHR(request_prefix, '/') )
            {
               const char *end;
               XP_StatStruct st;

               directory = (char *)request_prefix;
               end = directory + XP_STRLEN(directory) - 1;
               if ( *end == '/' || *end == '\\' ) {
                 bDirSlash = TRUE;
               }
               if (XP_Stat (directory, &st, xpURL))
                  XP_MakeDirectoryR (directory, xpURL);
               ext = (char *)extension;
               prefix = (char *)"su";
               break;
            }
        }
        // otherwise, fall through
      }
    case xpTemporary:
    default:
        directory = theApp.m_pTempDir;
        ext = (char *)extension;
        prefix = (char *)request_prefix;
        break;
    }

  if(!directory)
    return(NULL);

  if(!prefix)
    prefix = "X";

  if(!ext)
    ext = ".TMP";

  //  We need to base our temporary file names on time, and not on sequential
  //    addition because of the cache not being updated when the user
  //    crashes and files that have been deleted are over written with
  //    other files; bad data.
  //  The 52 valid DOS file name characters are
  //    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'()
  //  We will only be using the first 32 of the choices.
  //
  //  Time name format will be M+++++++.MOZ
  //    Where M is the single letter prefix (can be longer....)
  //    Where +++++++ is the 7 character time representation (a full 8.3
  //      file name will be made).
  //    Where .MOZ is the file extension to be used.
  //
  //  In the event that the time requested is the same time as the last call
  //    to this function, then the current time is incremented by one,
  //    as is the last time called to facilitate unique file names.
  //  In the event that the invented file name already exists (can't
  //    really happen statistically unless the clock is messed up or we
  //    manually incremented the time), then the times are incremented
  //    until an open name can be found.
  //
  //  time_t (the time) has 32 bits, or 4,294,967,296 combinations.
  //  We will map the 32 bits into the 7 possible characters as follows:
  //    Starting with the lsb, sets of 5 bits (32 values) will be mapped
  //      over to the appropriate file name character, and then
  //      incremented to an approprate file name character.
  //    The left over 2 bits will be mapped into the seventh file name
  //      character.
  //
  
  int i_letter, i_timechars, i_numtries = 0;
  char ca_time[8];
  time_t this_call = (time_t)0;
  
  //  We have to base the length of our time string on the length
  //    of the incoming prefix....
  //
  i_timechars = 8 - strlen(prefix);
  
  //  Infinite loop until the conditions are satisfied.
  //  There is no danger, unless every possible file name is used.
  //
  while(1)  {
    //    We used to use the time to generate this.
    //    Now, we use some crypto to avoid bug #47027
    RNG_GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));

    //  Convert the time into a 7 character string.
    //  Strip only the signifigant 5 bits.
    //  We'll want to reverse the string to make it look coherent
    //    in a directory of file names.
    //
    for(i_letter = 0; i_letter < i_timechars; i_letter++) {
      ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);
      
      //  Convert any numbers to their equivalent ascii code
      //
      if(ca_time[i_letter] <= 9)  {
        ca_time[i_letter] += '0';
      }
      //  Convert the character to it's equivalent ascii code
      //
      else  {
        ca_time[i_letter] += 'A' - 10;
      }
    }
    
    //  End the created time string.
    //
    ca_time[i_letter] = '\0';
    
    //  Reverse the time string.
    //
    _strrev(ca_time);
    
    //  Create the fully qualified path and file name.
    //
    if (bDirSlash)
      sprintf(file_buf, "%s%s%s%s", directory, prefix, ca_time, ext);
    else
      sprintf(file_buf, "%s\\%s%s%s", directory, prefix, ca_time, ext);

    //  Determine if the file exists, and mark that we've tried yet
    //    another file name (mark to be used later).
    //  
	//  Use the system call instead of XP_Stat since we already
	//  know the name and we don't want recursion
	//
	status = _stat(file_buf, &statinfo);
    i_numtries++;
    
    //  If it does not exists, we are successful, return the name.
    //
    if(status == -1)  {
      /* don't generate a directory as part of the 
       * cache temp names.  When the cache file name
       * is passed into the other XP_File functions
       * we will append the cache directory to the name
       * to get the complete path.
       * This will allow the cache to be moved around
       * and for netscape to be used to generate external
       * cache FAT's.  :lou
       */
      if(type == xpCache )
          sprintf(file_buf, "%s%s%s", prefix, ca_time, ext);

//      TRACE("Temp file name is %s\n", file_buf);
      return(file_buf);
    }
    
    //  If there is no room for additional characters in the time,
    //    we'll have to return NULL here, or we go infinite.
    //    This is a one case scenario where the requested prefix is
    //    actually 8 letters long.
    //  Infinite loops could occur with a 7, 6, 5, etc character prefixes
    //    if available files are all eaten up (rare to impossible), in
    //    which case, we should check at some arbitrary frequency of
    //    tries before we give up instead of attempting to Vulcanize
    //    this code.  Live long and prosper.
    //
    if(i_timechars == 0)  {
      break;
    }
    else if(i_numtries == 0x00FF) {
      break;
    }
  }
  
  //  Requested name is thought to be impossible to generate.
  //
  TRACE("No more temp file names....\n");
  return(NULL);
  
}
#endif // 0


nsresult nsNetFile::OpenFile(const char *aPath, nsFileMode aMode, nsFile** aRes) {
    nsFile *file = nsnull;
    char *path = nsnull;
    NS_PRECONDITION( (aPath != nsnull), "Null pointer.");

    if (!aPath)
        return NS_ERROR_NULL_POINTER;
    if (!*aPath)
        return NS_ERROR_FAILURE;

    file = (nsFile*) PR_Malloc(sizeof(nsFile));
    if (!file)
        return NS_ERROR_OUT_OF_MEMORY;

    // Get the correct path.
    if (GetFilePath(aPath, &path) == NS_ERROR_OUT_OF_MEMORY)
        return NS_ERROR_OUT_OF_MEMORY;

    // We should never be opening a relative file.
    NS_PRECONDITION( ( (*path != mDirDel) && (*path && (*path+1 != ':'))), "Opening a relative path.");

    file->fd = PR_Open(path, (convertToPRFlag(aMode)), 0600);

    PR_Free(path);
    path = nsnull;

    if (!file->fd) {
        PR_Free(file);
        return NS_ERROR_NULL_POINTER;
    }

    *aRes = file;

    return NS_OK;
}
    
nsresult nsNetFile::CloseFile(nsFile* aFile) {

    PRStatus rv;
    NS_PRECONDITION( (aFile != nsnull), "Null pointer.");

    if (!aFile->fd) 
        return NS_ERROR_NULL_POINTER;

    rv = PR_Close(aFile->fd);

    if (rv == PR_FAILURE)
        return NS_ERROR_FAILURE;

    PR_Free(aFile);
    aFile = nsnull;

    return NS_OK;
}

nsresult nsNetFile::FileRead(nsFile *aFile, char **aBuf, 
                             PRInt32 *aBuflen,
                             PRInt32 *aBytesRead) {
    NS_PRECONDITION( (aFile != nsnull), "Null pointer.");

    if (*aBuflen < 1)
        return NS_OK;
    
    *aBytesRead = PR_Read(aFile->fd, *aBuf, *aBuflen);

    if (*aBytesRead == -1) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult nsNetFile::FileReadLine(nsFile *aFile, char **aBuf, 
                        PRInt32 *aBuflen,
                        PRInt32 *aBytesRead) {
    PRInt32 readBytes;
    PRInt32 idx = 0;
    char *tmpBuf = nsnull;

    NS_PRECONDITION( (aFile != nsnull), "Null pointer.");

    if (*aBuflen < 1) {
        *aBytesRead = 0;
        return NS_OK;
    }

    readBytes = PR_Read(aFile->fd, *aBuf, *aBuflen);
    tmpBuf = *aBuf;

    if (readBytes > 0) {
        while (idx < readBytes) {
            if (tmpBuf[idx] == 10) { // LF
                if (tmpBuf[idx+1])
                    tmpBuf[idx+1] = '\0';
                // Back the file pointer up.
                PR_Seek(aFile->fd, ((idx+1) - readBytes), PR_SEEK_CUR);
                *aBytesRead = idx+1;
                return NS_OK;
            }
            idx++;
        }
    }

    if (readBytes == *aBuflen)
        return NS_ERROR_FAILURE;

    return NS_ERROR_FAILURE;
}

nsresult nsNetFile::FileWrite(nsFile *aFile, const char *aBuf, 
                              PRInt32 *aLen,
                              PRInt32 *aBytesWritten) {
    PRErrorCode error; // for testing, not propogated.
    NS_PRECONDITION( (aFile != nsnull), "Null pointer.");

    if (*aLen < 1)
        return NS_OK;
    
    *aBytesWritten = PR_Write(aFile->fd, aBuf, *aLen);

    if (*aBytesWritten == -1) {
        error = PR_GetError();
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult nsNetFile::FileSync(nsFile *aFile) {
    NS_PRECONDITION( (aFile != nsnull), "Null pointer.");

    if (PR_Sync(aFile->fd) == PR_FAILURE)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult nsNetFile::FileRemove(const char *aPath) {
    char *path = nsnull;
    NS_PRECONDITION( (aPath != nsnull), "Null pointer.");

    // Only remove absolute paths.
    if (GetFilePath(aPath, &path) == NS_ERROR_OUT_OF_MEMORY)
        return NS_ERROR_OUT_OF_MEMORY;

    // We should never be deleting a relative file.
    NS_PRECONDITION( ( (*path != mDirDel) && (*path && (*path+1 != ':'))), "Deleting a relative path.");

    PR_Free(path);

    return NS_OK;
}

nsresult nsNetFile::FileRename(const char *aPathOld, const char *aPathNew) {

    return NS_OK;
}

nsresult nsNetFile::OpenDir(const char *aPath, nsDir** aRes) {

    return NS_OK;
}

nsresult nsNetFile::CloseDir(nsDir *aDir) {

    return NS_OK;
}

nsresult nsNetFile::CreateDir(const char *aPath, PRBool aRecurse) {

    return NS_OK;
}

nsresult nsNetFile::SetDirectory(const char *aToken, const char *aDir) {
    nsresult rv = NS_OK;

    if (!PL_HashTableAdd(mHTDirs, PL_strdup(aToken), PL_strdup(aDir))) {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

    // Associate a filename with a token, and optionally a dir token.
nsresult nsNetFile::SetFileAssoc(const char *aToken, 
                                       const char *aFile, 
                                       const char *aDirToken) {
    nsString val(aFile);
    nsFileAssoc *theFile = new nsFileAssoc;

    if (aDirToken) {
        // Check for existence.
        char *dir = (char*)PL_HashTableLookup(mHTDirs, aDirToken);
        if (!dir)
            return NS_ERROR_FAILURE;
        theFile->dirTok = aDirToken;
    }

    theFile->name = aFile;

    if (!PL_HashTableAdd(mHTFiles, aToken, theFile)) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

PRIntn nsNetFile::convertToPRFlag(nsFileMode aMode) {

    switch (aMode) {
        case nsRead:
            return PR_RDONLY;
        case nsWrite:
            return PR_WRONLY;
        case nsReadWrite:
            return PR_RDWR;
        case nsReadBinary:
            return PR_RDONLY;
        case nsWriteBinary:
            return PR_WRONLY;
        case nsReadWriteBinary:
            return PR_RDWR;
        case nsOverWrite:
            return (PR_TRUNCATE | PR_WRONLY | PR_CREATE_FILE);
        default:
            return PR_RDONLY;
    }
}

char *nsNetFile::URLSyntaxToLocal(const char *aPath)

#ifdef XP_PC
{
    char *p, *newName;

    if(!aPath)
        return nsnull;
        
    //  If the name is only '/' or begins '//' keep the
	//    whole name else strip the leading '/'
	PRBool bChopSlash = PR_FALSE;

    if(aPath[0] == '/')
        bChopSlash = PR_TRUE;

	// save just / as a path
	if(aPath[0] == '/' && aPath[1] == '\0')
		bChopSlash = PR_FALSE;

	// spanky Win9X path name
	if(aPath[0] == '/' && aPath[1] == '/')
		bChopSlash = PR_FALSE;

    if(bChopSlash)
        newName = PL_strdup(&(aPath[1]));
    else
        newName = PL_strdup(aPath);

    if(!newName)
        return nsnull;

	if( newName[1] == '|' )
		newName[1] = ':';

    for(p = newName; *p; p++){
		if( *p == '/' )
			*p = '\\';
	}

    return(newName);
}
#elif XP_MAC
{
    return nsnull;
}
#else // UNIX
{
    return nsnull;
}
#endif


extern "C" {
/*
 * Factory for creating instance of the NetFile...
 */
NS_NET nsresult NS_NewINetFile(nsINetFile** aInstancePtrResult,
                                  nsISupports* aOuter)
{
    if (nsnull != aOuter) {
        return NS_ERROR_NO_AGGREGATION;
    }

#ifdef XP_MAC
    // Perform static initialization...
    if (nsnull == netFileInit) {
        netFileInit = new nsNetFileInit;
    }
#endif /* XP_MAC */ // XXX on the mac this never gets shutdown

    // The Netlib Service is created by the nsNetFileInit class...
    if (nsnull == gNetFile) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return gNetFile->QueryInterface(kINetFileIID, (void**)aInstancePtrResult);
}


NS_NET nsresult NS_InitINetFile(void)
{
    /* XXX: For now only allow a single instance of the Netlib Service */
    if (nsnull == gNetFile) {
        gNetFile = new nsNetFile();
        if (nsnull == gNetFile) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    NS_ADDREF(gNetFile);
    return NS_OK;
}

NS_NET nsresult NS_ShutdownINetFile()
{
    nsNetFile *service = gNetFile;

    // Release the container...
    if (nsnull != service) {
        NS_RELEASE(service);
    }
    return NS_OK;
}

}; /* extern "C" */
