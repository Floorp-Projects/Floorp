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
/* su_unix.c
 * UNIX specific softupdate routines
 */

#include "xp_mcom.h"
#include "su_folderspec.h"
#include "su_instl.h"
#include "fe_proto.h"
#include "xp_str.h"
#include "NSReg.h"

#ifndef MAXPATHLEN
#define  MAXPATHLEN   1024
#endif

extern void fe_GetProgramDirectory( char *, int );
int unlink (const char *);

char * FE_GetDirectoryPath( su_DirSpecID folderID)
{
	char *directory = NULL;
	char  Path[MAXPATHLEN+1];

	switch( folderID)
	{
		case ePluginFolder:
		{
			if ( getenv (UNIX_GLOBAL_FLAG) )
			{	
				if (directory = getenv("MOZILLA_HOME"))
				{
    					PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
				}
				else
					fe_GetProgramDirectory( Path, MAXPATHLEN-1 );

				XP_STRCAT(Path, "plugins/");
			}
			else
			{ 	/* Use local plugins path: $HOME/.netscape/plugins/ */

				char *Home = getenv ( "HOME" );
				if (!Home) 
					Home = "";
				else if (!strcmp (Home, "/"))
					Home = "";
    				PR_snprintf(Path, MAXPATHLEN, "%.900s/.netscape/plugins/", Home);
			}
			directory = XP_STRDUP( Path );
		}
		break;

		case eCommunicatorFolder:
		if (directory = getenv("MOZILLA_HOME"))
		{
    			PR_snprintf( Path, MAXPATHLEN, "%s/", directory );
			directory = XP_STRDUP( Path );
			break;
		}

		/* fall through */
		case eProgramFolder:
		{
			fe_GetProgramDirectory( Path, MAXPATHLEN-1 );
			directory = XP_STRDUP( Path );
		}
		break;

		case eUnix_LocalFolder:
			directory = XP_STRDUP( "/usr/local/netscape/" );
		break;

		case eUnix_LibFolder:
			directory = XP_STRDUP( "/usr/local/lib/netscape/" );
		break;

		case eTemporaryFolder:
		{
			directory = XP_STRDUP( "/tmp/" );
		}
		break;

		case eJavaBinFolder:
		if (directory = getenv("MOZILLA_HOME"))
		{
    			PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
		}
		else
			fe_GetProgramDirectory( Path, MAXPATHLEN-1 );
            XP_STRCAT(Path, "java/bin/");
            directory = XP_STRDUP( Path );
		break;

		case eJavaClassesFolder:
		if (directory = getenv("MOZILLA_HOME"))
		{
    			PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
		}
		else
			fe_GetProgramDirectory( Path, MAXPATHLEN-1 );
		XP_STRCAT(Path, "java/classes/");
		directory = XP_STRDUP( Path );
		break;

		case eJavaDownloadFolder:
		{
			if ( getenv (UNIX_GLOBAL_FLAG) )
			{	
				if (directory = getenv("MOZILLA_HOME"))
				{
    					PR_snprintf( Path, MAXPATHLEN, "%s/", directory);
				}
				else
					fe_GetProgramDirectory( Path, MAXPATHLEN-1 );

				XP_STRCAT(Path, "java/download/");
			}
			else
			{
				char *Home = getenv ( "HOME" );
			
				if (!Home) 
					Home = "";
				else if (!strcmp (Home, "/"))
					Home = "";
			
   				PR_snprintf(Path, MAXPATHLEN, "%.900s/.netscape/java/download/", Home);
			}
			directory = XP_STRDUP( Path );
		}
		break;

		case eNetHelpFolder:
		{
            char* tmpdir = FE_GetNetHelpDir();
            directory = WH_FileName(tmpdir+7, xpURL);   
            XP_FREEIF(tmpdir);
        }
		break;

		case eOSDriveFolder:
			directory = XP_STRDUP( "/" );
		break;

		case eMac_SystemFolder:
		case eMac_DesktopFolder:
		case eMac_TrashFolder:
		case eMac_StartupFolder:
		case eMac_ShutdownFolder:
		case eMac_AppleMenuFolder:
		case eMac_ControlPanelFolder:
		case eMac_ExtensionFolder:
		case eMac_FontsFolder:
		case eMac_PreferencesFolder:
		break;
		
		case eFileURLFolder:
		case ePackageFolder:
		default:
			XP_ASSERT(0);
		break;
	}

	return (directory);
}

/* Simple file execution. For now call system() */
int FE_ExecuteFile( const char * filename, const char * cmdline )
{
    if ( cmdline == NULL )
        return -1;

	return(system( cmdline ));
}


/* Crude file copy */
int FE_CopyFile (const char *in, const char *out)
{
	struct stat in_stat;
	int stat_result = -1;

	char	buf [1024];
	FILE	*ifp, *ofp;
	int	rbytes, wbytes;

	if (!in || !out)
		return -1;

	stat_result = stat (in, &in_stat);

	ifp = fopen (in, "r");
	if (!ifp) 
	{
		unlink(in);
		return -1;
	}

	ofp = fopen (out, "w");
	if (!ofp)
	{
		fclose (ifp);
		unlink(in);
		return -1;
	}

	while ((rbytes = fread (buf, 1, sizeof(buf), ifp)) > 0)
	{
		while (rbytes > 0)
		{
			if ( (wbytes = fwrite (buf, 1, rbytes, ofp)) < 0 )
			{
				fclose (ofp);
				fclose (ifp);
				unlink(in);
				unlink(out);
				return -1;
			}
			rbytes -= wbytes;
		}
	}
	fclose (ofp);
	fclose (ifp);
	unlink(in);

	if (stat_result == 0)
		{
		chmod (out, in_stat.st_mode & 0777);
		}

	return 0;
}

int    FE_ReplaceExistingFile(char *CurrentFname, XP_FileType ctype, 
		char *FinalFname, XP_FileType ftype, XP_Bool force)
{
	int err;
	err = XP_FileRemove( FinalFname, ftype );
	if ( 0 == err )
	{
   		err = XP_FileRename(CurrentFname, ctype, FinalFname, ftype);
	}
	return (err);
}



