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
#define CMD_STUB 1	/* this is just a stub for security/cmd/*  */
#define MOZ_LITE 1	/* don't need that other stuff, either */

#if 0
#include "stdafx.h"
#include "dialog.h"
#include "mainfrm.h"
#include "custom.h"
#include "shcut.h"
#include "edt.h"
#include "prmon.h"
#include "fegui.h"
#include "prefapi.h"
#include <io.h>
#include "secrng.h"
#include "mailass.h"
#include "ipframe.h"
#include "mnprefs.h"

#else

#include <xp_core.h>
#include <xp_file.h>
#include <xp_mcom.h>

#define ASSERT assert
#endif

#ifdef XP_MAC
#include "prpriv.h" /* For PR_NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif

#ifdef XP_WIN

#ifndef _AFXDLL
/* MSVC Debugging new...goes to regular new in release mode */
#define new DEBUG_NEW  
#endif

#ifdef __BORLANDC__
    #define _lseek lseek
#endif
       
#define WIDTHBYTES(i) ((i + 31) / 32 * 4)
#define MAXREAD 32767
       
MODULE_PRIVATE char * XP_CacheFileName();

#define MAXSTRINGLEN 8192 

#ifndef CMD_STUB

/* Return a string the same as the In string 
** but with all of the \n's replaced with \r\n's  
*/
MODULE_PRIVATE char * 
FE_Windowsify(const char * In)
{
    char *Out;
    char *o, *i; 
    int32 len;

    if(!In)
        return NULL;

    /* if every character is a \n then new string is twice as long */
    len = (int32) XP_STRLEN(In) * 2 + 1;
    Out = (char *) XP_ALLOC(len);
    if(!Out)
        return NULL;

  /* Move the characters over one by one.  If the current character is */
  /*  a \n replace add a \r before moving the \n over */
    for(i = (char *) In, o = Out; i && *i; *o++ = *i++)
       if(*i == '\n')
            *o++ = '\r';

    /* Make sure our new string is NULL terminated */
    *o = '\0';
    return(Out);
}


/*  Return some adjusted out full path on the mac. */
/*  For windows, we just return what was passed in. */
/*  We must provide it in a seperate buffer, otherwise they might change */
/*      the original and change also what they believe to be saved. */
char *
WH_FilePlatformName(const char *pName)    
{

    if(pName)   {
        return XP_STRDUP(pName);
    }
    
    return NULL;
}


char *
FE_GetProgramDirectory(char *buffer, int length)
{
    ::GetModuleFileName(theApp.m_hInstance, buffer, length);

    /*	Find the trailing slash. */
    char *pSlash = ::strrchr(buffer, '\\');
    if(pSlash)	{
	*(pSlash+1) = '\0';
    } else {
	buffer[0] = '\0';
    }
    return (buffer);
}


char*
XP_TempDirName(void)
{
    char *tmp = theApp.m_pTempDir;
    if (!tmp)
        return XP_STRDUP(".");
    return XP_STRDUP(tmp);
}

/* Windows _tempnam() lets the TMP environment variable override things sent 
** in so it look like we're going to have to make a temp name by hand.

** The user should *NOT* free the returned string.  
** It is stored in static space 
** and so is not valid across multiple calls to this function.

** The names generated look like 
**   c:\netscape\cache\m0.moz 
**   c:\netscape\cache\m1.moz 
** up to... 
**   c:\netscape\cache\m9999999.moz 
** after that if fails 
** */
PUBLIC char *
xp_TempFileName(int type, const char * request_prefix, const char * extension,
				char* file_buf)
{
    const char * directory = NULL; 
    char * ext = NULL;           /* file extension if any */
    char * prefix = NULL;        /* file prefix if any */
  
  
    XP_StatStruct statinfo;
    int status;     

    /* */
    /* based on the type of request determine what directory we should be */
    /*   looking into */
    /*   */
    switch(type) {
    case xpCache:
        directory = theApp.m_pCacheDir;
        ext = ".MOZ";
        prefix = CACHE_PREFIX;
        break;
#ifndef MOZ_LITE        
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
	/*changed to support multi-profile */
	/*directory = theApp.m_pInstallDir->GetCharValue(); */
	directory = (const char *)theApp.m_UserDirectory;
	if ((request_prefix == 0) || (XP_STRLEN (request_prefix) == 0))
		prefix = "abook";
	ext = ".nab";
	break;
#endif /* MOZ_LITE       */
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

  /*  We need to base our temporary file names on time, and not on sequential */
  /*    addition because of the cache not being updated when the user */
  /*    crashes and files that have been deleted are over written with */
  /*    other files; bad data. */
  /*  The 52 valid DOS file name characters are */
  /*    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'() */
  /*  We will only be using the first 32 of the choices. */
  /* */
  /*  Time name format will be M+++++++.MOZ */
  /*    Where M is the single letter prefix (can be longer....) */
  /*    Where +++++++ is the 7 character time representation (a full 8.3 */
  /*      file name will be made). */
  /*    Where .MOZ is the file extension to be used. */
  /* */
  /*  In the event that the time requested is the same time as the last call */
  /*    to this function, then the current time is incremented by one, */
  /*    as is the last time called to facilitate unique file names. */
  /*  In the event that the invented file name already exists (can't */
  /*    really happen statistically unless the clock is messed up or we */
  /*    manually incremented the time), then the times are incremented */
  /*    until an open name can be found. */
  /* */
  /*  time_t (the time) has 32 bits, or 4,294,967,296 combinations. */
  /*  We will map the 32 bits into the 7 possible characters as follows: */
  /*    Starting with the lsb, sets of 5 bits (32 values) will be mapped */
  /*      over to the appropriate file name character, and then */
  /*      incremented to an approprate file name character. */
  /*    The left over 2 bits will be mapped into the seventh file name */
  /*      character. */
  /* */
    
    int i_letter, i_timechars, i_numtries = 0;
    char ca_time[8];
    time_t this_call = (time_t)0;
    
    /*  We have to base the length of our time string on the length */
    /*    of the incoming prefix.... */
    /* */
    i_timechars = 8 - strlen(prefix);
    
    /*  Infinite loop until the conditions are satisfied. */
    /*  There is no danger, unless every possible file name is used. */
    /* */
    while(1)    {
        /*    We used to use the time to generate this. */
        /*    Now, we use some crypto to avoid bug #47027 */
        RNG_GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));

        /*  Convert the time into a 7 character string. */
        /*  Strip only the signifigant 5 bits. */
        /*  We'll want to reverse the string to make it look coherent */
        /*    in a directory of file names. */
        /* */
        for(i_letter = 0; i_letter < i_timechars; i_letter++) {
            ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);
            
          /*  Convert any numbers to their equivalent ascii code */
          /* */
            if(ca_time[i_letter] <= 9)    {
                ca_time[i_letter] += '0';
            }
          /*  Convert the character to it's equivalent ascii code */
          /* */
            else    {
                ca_time[i_letter] += 'A' - 10;
            }
        }
        
        /*  End the created time string. */
        /* */
        ca_time[i_letter] = '\0';
        
        /*  Reverse the time string. */
        /* */
        _strrev(ca_time);
        
        /*  Create the fully qualified path and file name. */
        /* */
        sprintf(file_buf, "%s\\%s%s%s", directory, prefix, ca_time, ext);

        /*  Determine if the file exists, and mark that we've tried yet */
        /*    another file name (mark to be used later). */
        /*   */
        /*  Use the system call instead of XP_Stat since we already */
        /*  know the name and we don't want recursion */
        /* */
        status = _stat(file_buf, &statinfo);
        i_numtries++;

        /*  If it does not exists, we are successful, return the name. */
        /* */
        if(status == -1)    {
           /* don't generate a directory as part of the cache temp names.    
	    * When the cache file name is passed into the other XP_File 
	    * functions we will append the cache directory to the name
            * to get the complete path.
            * This will allow the cache to be moved around
            * and for netscape to be used to generate external cache FAT's.    
            */
            if(type == xpCache )
		sprintf(file_buf, "%s%s%s", prefix, ca_time, ext);

            TRACE("Temp file name is %s\n", file_buf);
            return(file_buf);
        }
        
        /*  If there is no room for additional characters in the time, */
        /*    we'll have to return NULL here, or we go infinite. */
        /*    This is a one case scenario where the requested prefix is */
        /*    actually 8 letters long!   */
        /*  Infinite loops could occur with a 7, 6, 5, etc character prefixes */
        /*    if available files are all eaten up (rare to impossible), in */
        /*    which case, we should check at some arbitrary frequency of */
        /*    tries before we give up instead of attempting to Vulcanize */
        /*    this code.  Live long and prosper. */
        /* */
        if(i_timechars == 0)    {
            break;
        } else if(i_numtries == 0x00FF) {
            break;
        }
    }
    
    /*  Requested name is thought to be impossible to generate. */
    /* */
    TRACE("No more temp file names....\n");
    return(NULL);

}

PUBLIC char *
WH_TempFileName(int type, const char * request_prefix, const char * extension)
{
    static char file_buf[_MAX_PATH]; /* protected by _pr_TempName_lock */
    char* result;

    if (_pr_TempName_lock == NULL)
	_pr_TempName_lock = PR_NewNamedMonitor("TempName-lock");
    PR_EnterMonitor(_pr_TempName_lock);

    result = XP_STRDUP(xp_TempFileName(type, request_prefix, extension, file_buf));
    PR_ExitMonitor(_pr_TempName_lock);
    return result;
}

#endif /* CMD_STUB */

/* */
/* Return a string that is equal to the NetName string but with the */
/*  cross-platform characters changed back into DOS characters */
/* The caller is responsible for XP_FREE()ing the string */
/* */
MODULE_PRIVATE char * 
XP_NetToDosFileName(const char * NetName)
{
    char *p, *newName;
    BOOL bChopSlash = FALSE;

    if(!NetName)
        return NULL;
        
    /*  If the name is only '/' or begins '//' keep the */
    /*    whole name else strip the leading '/' */

    if(NetName[0] == '/')
        bChopSlash = TRUE;

    /* save just / as a path */
    if(NetName[0] == '/' && NetName[1] == '\0')
	bChopSlash = FALSE;

    /* spanky Win9X path name */
    if(NetName[0] == '/' && NetName[1] == '/')
	bChopSlash = FALSE;

    if(bChopSlash)
        newName = XP_STRDUP(&(NetName[1]));
    else
        newName = XP_STRDUP(NetName);

    if(!newName)
        return NULL;

    for(p = newName; *p; p++) {
        switch(*p) {
            case '|':
                *p = ':';
                break;
            case '/':
                *p = '\\';
                break;
            default:
                break;
        }
    }

    return(newName);

}


/* */
/* Returns the absolute name of a file  */
/*  */
/* The result of this function can be used with the standard */
/* open/fopen ansi file functions */
/* */
PUBLIC char * 
xp_FileName(const char * name, XP_FileType type, char* *myName)
{
    char * newName    = NULL;
    char * netLibName = NULL;
    char * tempName   = NULL;
    char * prefStr    = NULL;
    BOOL   bNeedToRegister = FALSE;   /* == do we need to register a new  */
                                      /*   newshost->file name mapping */
    struct _stat sInfo;
    int iDot;
    int iColon;

#ifndef CMD_STUB
    CString csHostName;
    CString csHost;
    CString fileName;
#endif

    switch(type) {

#ifndef CMD_STUB
    case xpCacheFAT:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\fat.db", (const char *)theApp.m_pCacheDir);
	break;
    case xpExtCacheIndex:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\extcache.fat", (const char *)theApp.m_pCacheDir);
	break;

    case xpSARCacheIndex:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf(newName, "%s\\archive.fat", theApp.m_pSARCacheDir);
        break;

    case xpHTTPCookie:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	/*sprintf(newName, "%s\\cookies.txt", theApp.m_pInstallDir->GetCharValue()); */
	/* changed to support multi-profile */
	sprintf(newName, "%s\\cookies.txt", (const char *)theApp.m_UserDirectory);
	break;
#ifndef MOZ_LITE      
    case xpSNewsRC:
    case xpNewsRC:
        /* see if we are asking about the default news host */
        /* else look up in netlib */
        if ( !name || !strlen(name) )
            name = g_MsgPrefs.m_csNewsHost;

        netLibName = NET_MapNewsrcHostToFilename((char *)name, 
                                                 (type == xpSNewsRC),
                                                 FALSE);
        
        /* if we found something in our map just skip the rest of this */
        if(netLibName && *netLibName) {
            newName = XP_STRDUP(netLibName);
            break;
        }

        /* whatever name we eventually end up with we will need to register it */
        /*   before we leave the function */
        bNeedToRegister = TRUE;

        /* If we are on the default host see if there is a newsrc file in the */
        /*   news directory.  If so, that is what we want */
        if(!stricmp(name, g_MsgPrefs.m_csNewsHost)) {
            csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += "\\newsrc";
            if(_stat((const char *) csHostName, &sInfo) == 0) {
                newName = XP_STRDUP((const char *) csHostName);
                break;
            }
        }
                        
        /* See if we are going to be able to build a file name based  */
        /*   on the hostname */
        csHostName = g_MsgPrefs.m_csNewsDir;
        csHostName += '\\';

        /* build up '<hostname>.rc' so we can tell how long its going to be */
        /*   we will use that as the default name to try */
		if(type == xpSNewsRC)
			csHost += 's';
        csHost += name;

        /* if we have a news host news.foo.com we just want to use the "news" */
        /*   part */
        iDot = csHost.Find('.');
        if(iDot != -1)
            csHost = csHost.Left(iDot);

#ifdef XP_WIN16
        if(csHost.GetLength() > 8)
            csHost = csHost.Left(8);
#endif

	iColon = csHost.Find(':');
	if (iColon != -1) {
		/* Windows file system seems to do horrid things if you have */
		/* a filename with a colon in it. */
		csHost = csHost.Left(iColon);
	}

        csHost += ".rc";

        /* csHost is now of the form <hostname>.rc and is in 8.3 format */
        /*   if we are on a Win16 box */

        csHostName += csHost;

        /* looks like a file with that name already exists -- panic */
        if(_stat((const char *) csHostName, &sInfo) != -1) {
            
            char host[5];

            /* else generate a new file in news directory */
            strncpy(host, name, 4);
            host[4] = '\0';

            newName = WH_TempFileName(type, host, ".rc");
            if(!newName)
                return(NULL);

        } else {

            newName = XP_STRDUP((const char *) csHostName);

        }

 		break;
    case xpNewsrcFileMap:
        /* return name of FAT file in news directory */
		newName = (char *) XP_ALLOC(_MAX_PATH);
		sprintf(newName, "%s\\fat", (const char *)g_MsgPrefs.m_csNewsDir);
		break;
    case xpNewsgroups:
    case xpSNewsgroups:
        /* look up in netlib */
        if ( !name || !strlen(name) )
            name = g_MsgPrefs.m_csNewsHost;

        netLibName = NET_MapNewsrcHostToFilename((char *)name, 
                                                 (type == xpSNewsgroups),
                                                 TRUE);

        if(!netLibName) {

            csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += '\\';

	    if(type == xpSNewsgroups)
		    csHost += 's';
            csHost += name;

            /* see if we can just use "<hostname>.rcg" */
            /* it might be news.foo.com so just take "news" */
            int iDot = csHost.Find('.');
            if(iDot != -1)
                csHost = csHost.Left(iDot);

#ifdef XP_WIN16
            if(csHost.GetLength() > 8)
                csHost = csHost.Left(8);
#endif

	    iColon = csHost.Find(':');
	    if (iColon != -1) {
		    /* Windows file system seems to do horrid things if you have */
		    /* a filename with a colon in it. */
		    csHost = csHost.Left(iColon);
	    }

            csHost += ".rcg";

            /* csHost is now of the form <hostname>.rcg */

            csHostName += csHost;

            /* looks like a file with that name already exists -- panic */
            if(_stat((const char *) csHostName, &sInfo) != -1) {
                
                char host[5];

                /* else generate a new file in news directory */
                strncpy(host, name, 4);
                host[4] = '\0';

                newName = WH_TempFileName(type, host, ".rcg");
                if(!newName)
                    return(NULL);

            } else {

                newName = XP_STRDUP((const char *) csHostName);

            }

            if ( !name || !strlen(name))
                NET_RegisterNewsrcFile(newName,(char *)(const char *)g_MsgPrefs.m_csNewsHost,
                    (type == xpSNewsgroups), TRUE );
            else
                NET_RegisterNewsrcFile(newName,(char*)name,(type == xpSNewsgroups), TRUE );

        } else {

            newName = XP_STRDUP(netLibName);

        }
        break;
    case xpMimeTypes:
	name = NULL;
	break;
#endif /* MOZ_LITE       */
    case xpGlobalHistory:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	/* changed to support multi-profile */
	/*sprintf(newName, "%s\\netscape.hst", theApp.m_pInstallDir->GetCharValue()); */
	sprintf(newName, "%s\\netscape.hst", (const char *)theApp.m_UserDirectory);
	break;
    case xpGlobalHistoryList:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf( newName, "%s\\ns_hstry.htm" );
        break;
    case xpKeyChain:
	name = NULL;
	break;

      /* larubbio */
    case xpSARCache:
        if(!name) {
            return NULL;
        }
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\%s", theApp.m_pSARCacheDir, name);
	break;

    case xpCache:
        if(!name) {
            tempName = WH_TempFileName(xpCache, NULL, NULL);
	    if (!tempName) return NULL;
	    name = tempName;
	}
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ((strchr(name,'|')  || strchr(name,':')))  { /* Local File URL if find a | */
            if(name[0] == '/')
            	strcpy(newName,name+1); /* skip past extra slash */
	    else
		strcpy(newName,name); /* absolute path is valid */
        } else {
	    sprintf(newName, "%s\\%s", (const char *)theApp.m_pCacheDir, name);
	}
        break;
    case xpBookmarks:
    case xpHotlist: 
	if (!name || !strlen(name)) 
	    name = theApp.m_pBookmarkFile;
	break;
#endif /* CMD_STUB */

    case xpSocksConfig:
	prefStr = NULL;
#ifndef CMD_STUB
	PREF_CopyCharPref("browser.socksfile_location", &prefStr);
#else
	ASSERT(0);
#endif
	name = prefStr;
	break;		

#ifndef CMD_STUB
    case xpCertDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ( name ) {
	    sprintf(newName, "%s\\cert%s.db", (const char *)theApp.m_UserDirectory, name);
        } else {
	    sprintf(newName, "%s\\cert.db", (const char *)theApp.m_UserDirectory);
        }
	break;
    case xpCertDBNameIDX:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\certni.db", (const char *)theApp.m_UserDirectory);
	break;
    case xpKeyDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	if ( name ) {
	  sprintf(newName, "%s\\key%s.db", (const char *)theApp.m_UserDirectory, name);
	} else {
	  sprintf(newName, "%s\\key.db", (const char *)theApp.m_UserDirectory);
	}
	break;
    case xpSecModuleDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\secmod.db", (const char *)theApp.m_UserDirectory);
	break;
    case xpSignedAppletDB:
        newName = (char *) XP_ALLOC(_MAX_PATH);
	if ( name ) {
	  sprintf(newName, "%s\\signed%s.db", (const char *)theApp.m_UserDirectory, name);
	} else {
	  sprintf(newName, "%s\\signed.db", (const char *)theApp.m_UserDirectory);
	}
		break;
#ifndef MOZ_LITE      
    case xpAddrBook:
#ifdef XP_WIN16
	if(!name || !strlen(name) )
	    newName = WH_TempName(type, NULL);
#else
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the extension */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;

	pEnd = strchr(pEnd, '.');
	if(pEnd)
	    *pEnd = '\0';
	}
	strcat(newName, ".nab");
#endif
        break;
    case xpAddrBookNew:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\%s", (const char *)theApp.m_UserDirectory, name);
	break;
    case xpVCardFile:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the extension */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;

	pEnd = strchr(pEnd, '.');
	if(pEnd)
	    *pEnd = '\0';
	}		
	strcat(newName, ".vcf");
        break;
    case xpLDIFFile:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the extension */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;

	pEnd = strchr(pEnd, '.');
	if(pEnd)
	    *pEnd = '\0';
	}		
#ifdef XP_WIN16
	strcat(newName, ".ldi");
#else
	strcat(newName, ".ldif");
#endif
        break;
    case xpTemporaryNewsRC:
        {
            CString csHostName = g_MsgPrefs.m_csNewsDir;
            csHostName += "\\news.tmp";
            newName = XP_STRDUP((const char *) csHostName);
        }
        break;
#endif /* MOZ_LITE         */
    case xpPKCS12File:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the extension */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;

	pEnd = strchr(pEnd, '.');
	if(pEnd)
	    *pEnd = '\0';
	}		
	strcat(newName, ".p12");
	break;	
    case xpTemporary:
        if(!name || !strlen(name) )
            newName = WH_TempName(type, NULL);
        break;
#ifndef MOZ_LITE        
    case xpMailFolder:
	if(!name)
	    name = g_MsgPrefs.m_csMailDir;
        break;
    case xpMailFolderSummary:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the extension */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;

#ifdef XP_WIN16	/* backend won't allow '.' in win16 folder names, but just to be safe. */
	pEnd = strchr(pEnd, '.');
	if(pEnd)
	    *pEnd = '\0';
#endif
	}
	strcat(newName, ".snm");
	break;
    case xpMailSort:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf(newName, "%s\\rules.dat", (const char *)g_MsgPrefs.m_csMailDir);
	break;
    case xpMailFilterLog:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\mailfilt.log", (const char *)g_MsgPrefs.m_csMailDir);
	break;
    case xpNewsFilterLog:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\newsfilt.log", (const char *)g_MsgPrefs.m_csNewsDir);
	break;
    case xpMailPopState:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        sprintf(newName, "%s\\popstate.dat", (const char *)g_MsgPrefs.m_csMailDir);
	break;
    case xpMailSubdirectory:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	strcpy(newName, name);

	/* strip off the trailing slash if any */
	{
	char * pEnd = max(strrchr(newName, '\\'), strrchr(newName, '/'));
	if(!pEnd)
	    pEnd = newName;
	}

	strcat(newName, ".sbd");
	break;
#endif /* MOZ_LITE       */
    /* name of global cross-platform registry */
    case xpRegistry:
        /* eventually need to support arbitrary names; this is the default */
        newName = (char *) XP_ALLOC(_MAX_PATH);
        if ( newName != NULL ) {
            GetWindowsDirectory(newName, _MAX_PATH);
            int namelen = strlen(newName);
            if ( newName[namelen-1] == '\\' )
                namelen--;
            strcpy(newName+namelen, "\\nsreg.dat");
        }
        break;
	/* name of news group database  */
#ifndef MOZ_LITE   
    case xpXoverCache:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\%s", (const char *)g_MsgPrefs.m_csNewsDir, name);
	break;
#endif /* MOZ_LITE */
    case xpProxyConfig:
        newName = (char *) XP_ALLOC(_MAX_PATH);
        /*sprintf(newName, "%s\\proxy.cfg", theApp.m_pInstallDir->GetCharValue()); */
	sprintf(newName, "%s\\proxy.cfg", (const char *)theApp.m_UserDirectory);
	break;

    /* add any cases where no modification is necessary here 	 */
    /* The name is fine all by itself, no need to modify it  */
    case xpFileToPost:
    case xpExtCache:
    case xpURL:
        /* name is OK as it is */
	break;
#ifndef MOZ_LITE      
    case xpNewsHostDatabase:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	sprintf(newName, "%s\\news.db", (const char *)g_MsgPrefs.m_csNewsDir);
	break;

    case xpImapRootDirectory:
	newName = PR_smprintf ("%s\\ImapMail", (const char *)theApp.m_UserDirectory);
	break;
    case xpImapServerDirectory:
	{
	int len = 0;
	char *tempImapServerDir = XP_STRDUP(name);
	char *imapServerDir = tempImapServerDir;
#ifdef XP_WIN16
	if ((len = XP_STRLEN(imapServerDir)) > 8) {
	    imapServerDir = imapServerDir + len - 8;
	}
#endif
	newName = PR_smprintf ("%s\\ImapMail\\%s", (const char *)theApp.m_UserDirectory, imapServerDir);
	if (tempImapServerDir) XP_FREE(tempImapServerDir);
	}
	break;

    case xpJSMailFilters:
	newName = PR_smprintf("%s\\filters.js", (const char *)g_MsgPrefs.m_csMailDir);
	break;
#endif /* MOZ_LITE */
    case xpFolderCache:
	newName = PR_smprintf ("%s\\summary.dat", (const char *)theApp.m_UserDirectory);
	break;

    case xpCryptoPolicy:
	newName = (char *) XP_ALLOC(_MAX_PATH);
	FE_GetProgramDirectory(newName, _MAX_PATH);
	strcat(newName, "moz40p3");
	break;
#endif /* CMD_STUB */

    default:
	ASSERT(0);  /* all types should be covered */
        break;
    }

#ifndef MOZ_LITE    
    /* make sure we have the correct newsrc file registered for next time */
	if((type == xpSNewsRC || type == xpNewsRC) && bNeedToRegister)
        NET_RegisterNewsrcFile(newName, (char *)name, (type == xpSNewsRC), FALSE );
#endif

    /* determine what file we are supposed to load and make sure it looks */
    /*   like a DOS pathname and not some unix-like name */
    if(newName) {
	*myName = XP_NetToDosFileName((const char*)newName);
	XP_FREE(newName); 
    } else {
	*myName = XP_NetToDosFileName((const char*)name);
    }

    if (tempName) XP_FREE(tempName);

    if (prefStr) XP_FREE(prefStr);        

    /* whee, we're done */
    return(*myName);
}

/* */
/* Open a file with the given name */
/* If a special file type is provided we might need to get the name */
/*  out of the preferences list */
/* */
PUBLIC XP_File 
XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    XP_File fp;
    char *filename = WH_FileName(name, type);

    if(!filename)
	return(NULL);
		
#ifdef DEBUG_nobody
    TRACE("Opening a file type (%d) permissions: %s (%s)\n", type, perm, filename);
#endif

#ifdef XP_WIN32
    if (type == xpURL) {
	HANDLE	hFile;
	DWORD	dwType;

	/* Check if we're trying to open a device. We don't allow this */
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL);

	if (hFile != INVALID_HANDLE_VALUE) {
	    dwType = GetFileType(hFile);
	    CloseHandle(hFile);
	
	    if (dwType != FILE_TYPE_DISK) {
		XP_FREE(filename);
		return NULL;
	    }
	}
    }
#endif

#ifdef XP_WIN16
    /* Windows uses ANSI codepage, DOS uses OEM codepage,  */
    /* fopen takes OEM codepage */
    /* That's why we need to do conversion here. */
    CString oembuff = filename;
    oembuff.AnsiToOem();
    fp = fopen(oembuff, (char *) perm);

    if (fp && type == xpURL) {
	union _REGS	inregs, outregs;

	/* Check if we opened a device. Execute an Interrupt 21h to invoke */
	/* MS-DOS system call 44h */
	inregs.x.ax = 0x4400;	 /* MS-DOS function to get device information */
	inregs.x.bx = _fileno(fp);
	_int86(0x21, &inregs, &outregs);

	if (outregs.x.dx & 0x80) {
	    /* It's a device. Don't allow any reading/writing */
	    fclose(fp);
	    XP_FREE(filename);
	    return NULL;
	}
    }
#else	
    fp = fopen(filename, (char *) perm);
#endif
    XP_FREE(filename);
    return(fp);
}



/******************************************************************************/
/* Thread-safe entry points: */

extern PRMonitor* _pr_TempName_lock;

#ifndef CMD_STUB

char *
WH_TempName(XP_FileType type, const char * prefix)
{
    static char buf[_MAX_PATH];	/* protected by _pr_TempName_lock */
    char* result;

    if (_pr_TempName_lock == NULL)
	_pr_TempName_lock = PR_NewNamedMonitor("TempName-lock");
    PR_EnterMonitor(_pr_TempName_lock);

    result = XP_STRDUP(xp_TempFileName(type, prefix, NULL, buf));

    PR_ExitMonitor(_pr_TempName_lock);

    return result;
}

#endif /* CMD_STUB */

PUBLIC char *
WH_FileName (const char *name, XP_FileType type)
{
    char* myName;
    char* result;
    /*
    ** I'm not sure this lock is really needed by windows, but just
    ** to be safe:
    */
    /*	XP_ASSERT(_pr_TempName_lock); */
    /*	PR_EnterMonitor(_pr_TempName_lock); */
    result = xp_FileName(name, type, &myName);
    /*	PR_ExitMonitor(_pr_TempName_lock); */
    return myName;
}

#endif /* XP_WIN */
