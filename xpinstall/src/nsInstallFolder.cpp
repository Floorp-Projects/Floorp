/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Douglas Turner <dougt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsInstall.h"
#include "nsInstallFolder.h"

#include "nscore.h"
#include "prtypes.h"
#include "nsIComponentManager.h"

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsXPIDLString.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#ifdef XP_WIN
#include <stdarg.h>
#include <winbase.h>
#include <winreg.h>
#endif

struct DirectoryTable
{
	const char * directoryName;		/* The formal directory name */
	PRInt32 folderEnum;		        /* Directory ID */
};

struct DirectoryTable DirectoryTable[] = 
{
  {"Plugins",                       PLUGIN_DIR                       },
  {"Program",                       PROGRAM_DIR                      },
  {"Communicator",                  PROGRAM_DIR                      }, // "Communicator" is deprecated

  {"Temporary",                     TEMP_DIR                         },
  {"OS Home",                       OS_HOME_DIR                      },
  {"Profile",                       PROFILE_DIR                      },
  {"Current User",                  PROFILE_DIR                      }, // "Current User" is deprecated
  {"Preferences",                   PREFERENCES_DIR                  },
  {"OS Drive",                      OS_DRIVE                         },
  {"file:///",                      FILE_TARGET                      },
          
  {"Components",                    COMPONENTS_DIR                   },
  {"Chrome",                        CHROME_DIR                       },

  {"Win System",                    WIN_SYS_DIR                      },
  {"Windows",                       WINDOWS_DIR                      },
  {"Win Desktop",                   WIN_DESKTOP_DIR                  },
  {"Win Desktop Common",            WIN_DESKTOP_COMMON               },
  {"Win StartMenu",                 WIN_STARTMENU                    },
  {"Win StartMenu Common",          WIN_STARTMENU_COMMON             },
  {"Win Programs",                  WIN_PROGRAMS_DIR                 },
  {"Win Programs Common",           WIN_PROGRAMS_COMMON              },
  {"Win Startup",                   WIN_STARTUP_DIR                  },
  {"Win Startup Common",            WIN_STARTUP_COMMON               },
  {"Win AppData",                   WIN_APPDATA_DIR                  },
  {"Win Program Files",             WIN_PROGRAM_FILES                },
  {"Win Common Files",              WIN_COMMON_FILES                 },

  {"Mac System",                    MAC_SYSTEM                       },
  {"Mac Desktop",                   MAC_DESKTOP                      },
  {"Mac Trash",                     MAC_TRASH                        },
  {"Mac Startup",                   MAC_STARTUP                      },                                        
  {"Mac Shutdown",                  MAC_SHUTDOWN                     },
  {"Mac Apple Menu",                MAC_APPLE_MENU                   },
  {"Mac Control Panel",             MAC_CONTROL_PANEL                },
  {"Mac Extension",                 MAC_EXTENSION                    },
  {"Mac Fonts",                     MAC_FONTS                        },
  {"Mac Preferences",               MAC_PREFERENCES                  },
  {"Mac Documents",                 MAC_DOCUMENTS                    },

  {"MacOSX Home",                   MACOSX_HOME                      },
  {"MacOSX Default Download",       MACOSX_DEFAULT_DOWNLOAD          },
  {"MacOSX User Desktop",           MACOSX_USER_DESKTOP              },
  {"MacOSX Local Desktop",          MACOSX_LOCAL_DESKTOP             },
  {"MacOSX User Applications",      MACOSX_USER_APPLICATIONS         },
  {"MacOSX Local Applications",     MACOSX_LOCAL_APPLICATIONS        },
  {"MacOSX User Documents",         MACOSX_USER_DOCUMENTS            },
  {"MacOSX Local Documents",        MACOSX_LOCAL_DOCUMENTS           },
  {"MacOSX User Internet PlugIn",   MACOSX_USER_INTERNET_PLUGIN      },
  {"MacOSX Local Internet PlugIn",  MACOSX_LOCAL_INTERNET_PLUGIN     },
  {"MacOSX User Frameworks",        MACOSX_USER_FRAMEWORKS           },
  {"MacOSX Local Frameworks",       MACOSX_LOCAL_FRAMEWORKS          },
  {"MacOSX User Preferences",       MACOSX_USER_PREFERENCES          },
  {"MacOSX Local Preferences",      MACOSX_LOCAL_PREFERENCES         },
  {"MacOSX Picture Documents",      MACOSX_PICTURE_DOCUMENTS         },
  {"MacOSX Movie Documents",        MACOSX_MOVIE_DOCUMENTS           },
  {"MacOSX Music Documents",        MACOSX_MUSIC_DOCUMENTS           },
  {"MacOSX Internet Sites",         MACOSX_INTERNET_SITES            },

  {"Unix Local",                    UNIX_LOCAL                       },
  {"Unix Lib",                      UNIX_LIB                         },

  {"",                              -1                               }
};


MOZ_DECL_CTOR_COUNTER(nsInstallFolder)

nsInstallFolder::nsInstallFolder()
{
    MOZ_COUNT_CTOR(nsInstallFolder);
}

nsresult
nsInstallFolder::Init(nsIFile* rawIFile, const nsString& aRelativePath)
{
    mFileSpec = rawIFile;

    if (!aRelativePath.IsEmpty())
        AppendXPPath(aRelativePath);

    return NS_OK;
}

nsresult
nsInstallFolder::Init(const nsAString& aFolderID, const nsString& aRelativePath)
{
    SetDirectoryPath( aFolderID, aRelativePath );

    if (mFileSpec)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

nsresult
nsInstallFolder::Init(nsInstallFolder& inFolder, const nsString& subString)
{
    if (!inFolder.mFileSpec)
        return NS_ERROR_NULL_POINTER;

    inFolder.mFileSpec->Clone(getter_AddRefs(mFileSpec));
    
    if (!mFileSpec)
        return NS_ERROR_FAILURE;

    if(!subString.IsEmpty())
        AppendXPPath(subString);

    return NS_OK;
}


nsInstallFolder::~nsInstallFolder()
{
   MOZ_COUNT_DTOR(nsInstallFolder);
}

void 
nsInstallFolder::GetDirectoryPath(nsCString& aDirectoryPath)
{
  PRBool flagIsDir;
  nsCAutoString thePath;
  
  aDirectoryPath.SetLength(0);

    if (mFileSpec != nsnull)
    {
      // We want the NATIVE path.
      mFileSpec->GetNativePath(thePath);
      aDirectoryPath.Assign(thePath);

      mFileSpec->IsDirectory(&flagIsDir);
      if (flagIsDir)
      {
        if (aDirectoryPath.Last() != FILESEP)
            aDirectoryPath.Append(FILESEP);
      }
    }
}

void
nsInstallFolder::SetDirectoryPath(const nsAString& aFolderID, const nsString& aRelativePath)
{
    nsresult rv = NS_OK;

    // reset mFileSpec in case of error
    mFileSpec = nsnull;
    
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    if (!directoryService)
          return;

    PRInt32 dirID = MapNameToEnum(aFolderID);
    switch ( dirID )
    {
        case PLUGIN_DIR:
            if (!nsSoftwareUpdate::GetProgramDirectory())
                directoryService->Get(NS_APP_PLUGINS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            else
            {
                rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));

                if (NS_SUCCEEDED(rv))
                    mFileSpec->AppendNative(INSTALL_PLUGINS_DIR);
                else
                    mFileSpec = nsnull;
            }
            break; 


        case PROGRAM_DIR:
            if (!nsSoftwareUpdate::GetProgramDirectory())  //Not in stub installer
                directoryService->Get(NS_OS_CURRENT_PROCESS_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            else //In stub installer.  mProgram has been set so 
                rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));
            break;

        
        case TEMP_DIR:
            directoryService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            break;


        case PROFILE_DIR:
            directoryService->Get(NS_APP_USER_PROFILE_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            break;
            
        case OS_HOME_DIR:
            directoryService->Get(NS_OS_HOME_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            break;

        case PREFERENCES_DIR:
                directoryService->Get(NS_APP_PREFS_50_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            break;

        case OS_DRIVE:
            directoryService->Get(NS_OS_DRIVE_DIR, NS_GET_IID(nsIFile), getter_AddRefs(mFileSpec));
            break;

        case FILE_TARGET:
            {
                if (!aRelativePath.IsEmpty())
                {
                    nsFileSpec             tmpSpec;
                    nsCAutoString          tmpPath("file:///");
                    nsCAutoString          nativePath;
                    nsCOMPtr<nsILocalFile> localFile;

                    NS_CopyUnicodeToNative(aRelativePath, nativePath);
                    tmpPath.Append(nativePath);
                    tmpSpec =  nsFileURL(tmpPath.get());

                    rv = NS_FileSpecToIFile( &tmpSpec, getter_AddRefs(localFile) );
                    if (NS_SUCCEEDED(rv))
                    {
                        mFileSpec = do_QueryInterface(localFile);
                    }
                }

                // file:// is a special case where it returns and does not
                // go to the standard relative path code below.  This is
                // so that nsFile(Spec|Path) will work properly.  (ie. Passing
                // just "file://" to the nsFileSpec && nsFileURL is wrong).

                return;
            }
            break;

        case COMPONENTS_DIR:
            if (!nsSoftwareUpdate::GetProgramDirectory())
            {
                directoryService->Get(NS_XPCOM_COMPONENT_DIR, 
                                       NS_GET_IID(nsIFile), 
                                       getter_AddRefs(mFileSpec));
            }
            else
            {
                rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));

                if (NS_SUCCEEDED(rv))
                    mFileSpec->AppendNative(INSTALL_COMPONENTS_DIR);
                else
                    mFileSpec = nsnull;
            }
            break;
        
        case CHROME_DIR:
            if (!nsSoftwareUpdate::GetProgramDirectory())
            { 
                directoryService->Get(NS_APP_CHROME_DIR, 
                                      NS_GET_IID(nsIFile), 
                                      getter_AddRefs(mFileSpec));
            }
            else
            {
                rv = nsSoftwareUpdate::GetProgramDirectory()->Clone(getter_AddRefs(mFileSpec));
                if (NS_SUCCEEDED(rv))
                    mFileSpec->AppendNative(INSTALL_CHROME_DIR);
            }
            break;

#if defined(XP_WIN)
        case WIN_SYS_DIR:
            directoryService->Get(NS_OS_SYSTEM_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));

            break;

        case WINDOWS_DIR:
            directoryService->Get(NS_WIN_WINDOWS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case WIN_DESKTOP_DIR:
            directoryService->Get( NS_WIN_DESKTOP_DIRECTORY,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_DESKTOP_COMMON:
            directoryService->Get( NS_WIN_COMMON_DESKTOP_DIRECTORY,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_STARTMENU:
            directoryService->Get( NS_WIN_STARTMENU_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_STARTMENU_COMMON:
            directoryService->Get( NS_WIN_COMMON_STARTMENU_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_PROGRAMS_DIR:
            directoryService->Get( NS_WIN_PROGRAMS_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_PROGRAMS_COMMON:
            directoryService->Get( NS_WIN_COMMON_PROGRAMS_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_STARTUP_DIR:
            directoryService->Get( NS_WIN_STARTUP_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_STARTUP_COMMON:
            directoryService->Get( NS_WIN_COMMON_STARTUP_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_APPDATA_DIR:
            directoryService->Get( NS_WIN_APPDATA_DIR,
                                   NS_GET_IID(nsIFile),
                                   getter_AddRefs(mFileSpec) );
            break;

        case WIN_PROGRAM_FILES:
        case WIN_COMMON_FILES:
            {
                HKEY key;
                LONG result = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
                    0, KEY_QUERY_VALUE, &key );

                if ( result != ERROR_SUCCESS )
                    break;

                BYTE path[_MAX_PATH + 1] = { 0 };
                DWORD type;
                DWORD pathlen = sizeof(path);
                const char *value = (dirID==WIN_PROGRAM_FILES) ?
                                "ProgramFilesDir" :
                                "CommonFilesDir";
                result = RegQueryValueEx( key, value, 0, &type, path, &pathlen );
                if ( result == ERROR_SUCCESS && type == REG_SZ )
                {
                    nsCOMPtr<nsILocalFile> tmp;
                    NS_NewNativeLocalFile( nsDependentCString((char*)path),
                                           PR_FALSE, getter_AddRefs(tmp) );
                    mFileSpec = do_QueryInterface(tmp);
                }
           }
           break;
#endif

#if defined (XP_MACOSX)
        case MAC_SYSTEM:
            directoryService->Get(NS_OS_SYSTEM_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_DESKTOP:
            directoryService->Get(NS_MAC_DESKTOP_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_TRASH:
            directoryService->Get(NS_MAC_TRASH_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_STARTUP:
            directoryService->Get(NS_MAC_STARTUP_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_SHUTDOWN:
            directoryService->Get(NS_MAC_SHUTDOWN_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_APPLE_MENU:
            directoryService->Get(NS_MAC_APPLE_MENU_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_CONTROL_PANEL:
            directoryService->Get(NS_MAC_CONTROL_PANELS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_EXTENSION:
            directoryService->Get(NS_MAC_EXTENSIONS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_FONTS:
            directoryService->Get(NS_MAC_FONTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MAC_PREFERENCES:
            directoryService->Get(NS_MAC_PREFS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;
                
        case MAC_DOCUMENTS:
            directoryService->Get(NS_MAC_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_HOME:
            directoryService->Get(NS_OSX_HOME_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_DEFAULT_DOWNLOAD:
            directoryService->Get(NS_OSX_DEFAULT_DOWNLOAD_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_DESKTOP:
            directoryService->Get(NS_OSX_USER_DESKTOP_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_DESKTOP:
            directoryService->Get(NS_OSX_LOCAL_DESKTOP_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_APPLICATIONS:
            directoryService->Get(NS_OSX_USER_APPLICATIONS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_APPLICATIONS:
            directoryService->Get(NS_OSX_LOCAL_APPLICATIONS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_DOCUMENTS:
            directoryService->Get(NS_OSX_USER_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_DOCUMENTS:
            directoryService->Get(NS_OSX_LOCAL_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_INTERNET_PLUGIN:
            directoryService->Get(NS_OSX_USER_INTERNET_PLUGIN_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_INTERNET_PLUGIN:
            directoryService->Get(NS_OSX_LOCAL_INTERNET_PLUGIN_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_FRAMEWORKS:
            directoryService->Get(NS_OSX_USER_FRAMEWORKS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_FRAMEWORKS:
            directoryService->Get(NS_OSX_LOCAL_FRAMEWORKS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_USER_PREFERENCES:
            directoryService->Get(NS_OSX_USER_PREFERENCES_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_LOCAL_PREFERENCES:
            directoryService->Get(NS_OSX_LOCAL_PREFERENCES_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_PICTURE_DOCUMENTS:
            directoryService->Get(NS_OSX_PICTURE_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_MOVIE_DOCUMENTS:
            directoryService->Get(NS_OSX_MOVIE_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;
                
        case MACOSX_MUSIC_DOCUMENTS:
            directoryService->Get(NS_OSX_MUSIC_DOCUMENTS_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case MACOSX_INTERNET_SITES:
            directoryService->Get(NS_OSX_INTERNET_SITES_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;
#endif

#if defined(XP_UNIX) && !defined(XP_MACOSX)                
        case UNIX_LOCAL:
            directoryService->Get(NS_UNIX_LOCAL_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;

        case UNIX_LIB:
            directoryService->Get(NS_UNIX_LIB_DIR, 
                                  NS_GET_IID(nsIFile), 
                                  getter_AddRefs(mFileSpec));
            break;
#endif

        case -1:
        default:
            mFileSpec = nsnull;
            break;
    }

    if (mFileSpec && !aRelativePath.IsEmpty())
    {
        AppendXPPath(aRelativePath);
    }
}


void
nsInstallFolder::AppendXPPath(const nsString& aRelativePath)
{
    nsAutoString segment;
    PRUint32 start = 0;
    PRUint32 curr;

    NS_ASSERTION(!aRelativePath.IsEmpty(),"InstallFolder appending null path");
    do {
        curr = aRelativePath.FindChar('/',start);
        if ( curr == start )
        {
            // illegal, two slashes in a row (or not a relative path)
            mFileSpec = nsnull;
            break;
        }
        else if ( curr == kNotFound )
        {
            // last segment
            aRelativePath.Right(segment,aRelativePath.Length() - start);
            start = aRelativePath.Length();
        }
        else
        {
            // found a segment
            aRelativePath.Mid(segment,start,curr-start);
            start = curr+1;
        }
            
        nsresult rv = mFileSpec->Append(segment);
        if (NS_FAILED(rv))
        {
            // Unicode converters not present (likely wizard case)
            // so do our best with the vanilla conversion.
            mFileSpec->AppendNative(NS_LossyConvertUCS2toASCII(segment));
        }
    } while ( start < aRelativePath.Length() );
}


/* MapNameToEnum
 * maps name from the directory table to its enum */
PRInt32 
nsInstallFolder::MapNameToEnum(const nsAString& name)
{
	int i = 0;

	if ( name.IsEmpty())
        return -1;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
    // safe compare because all strings in DirectoryTable are ASCII
    if ( name.Equals(NS_ConvertASCIItoUCS2(DirectoryTable[i].directoryName), nsCaseInsensitiveStringComparator()) )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return -1;
}



nsIFile*
nsInstallFolder::GetFileSpec()
{
  return mFileSpec;
}

PRInt32
nsInstallFolder::ToString(nsAutoString* outString)
{
  //XXX: May need to fix. Native charset paths will be converted into Unicode when the get to JS
  //     This will appear to work on Latin-1 charsets but won't work on Mac or other charsets.
  //     On the other hand doing it right requires intl charset converters
  //     which we don't yet have in the initial install case.
  //
  //     However, nsLocalFile now uses the native charset conversion implementation, so this
  //     code should always work.

  if (!mFileSpec || !outString)
      return NS_ERROR_NULL_POINTER;

  nsresult rv = mFileSpec->GetPath(*outString);
  if (NS_FAILED(rv))
  {
    // converters not present, most likely in wizard case;
    // do best we can with stock ASCII conversion

    // Since bug 100676 was fixed we should never get here

    // XXX NOTE we can make sure our filenames are ASCII, but we have no
    // control over the directory name which might be localized!!!
    NS_ASSERTION(PR_FALSE, "Couldn't get Unicode path, using broken conversion!");
    nsCAutoString temp;
    rv = mFileSpec->GetNativePath(temp);
    CopyASCIItoUTF16(temp, *outString);
  }

  PRBool flagIsFile = PR_FALSE;
  mFileSpec->IsFile(&flagIsFile);
  if (!flagIsFile)
  {
      // assume directory, thus end with slash.
      outString->Append(PRUnichar(FILESEP));
  }

  return rv;
}

