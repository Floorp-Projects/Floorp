/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bill Law    <law@netscape.com>
 *  Syd Logan   <syd@netscape.com> added turbo mode stuff
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

// Implementation utilities.
#include "nsIDOMWindowInternal.h"
#include "nsIServiceManager.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsIAllocator.h"
#include "nsICmdLineService.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsNetCID.h"
// The order of these headers is important on Win2K because CreateDirectory
// is |#undef|-ed in nsFileSpec.h, so we need to pull in windows.h for the
// first time after nsFileSpec.h.
#include "nsWindowsHooksUtil.cpp"
#include "nsWindowsHooks.h"
#include <windows.h>
#include <shlobj.h>
#include <shlguid.h>

// Objects that describe the Windows registry entries that we need to tweak.
static ProtocolRegistryEntry
    http( "http" ),
    https( "https" ),
    ftp( "ftp" ),
    chrome( "chrome" ),
    gopher( "gopher" );
const char *jpgExts[] = { ".jpg", ".jpeg", 0 };
const char *gifExts[] = { ".gif", 0 };
const char *pngExts[] = { ".png", 0 };
const char *xmlExts[] = { ".xml", 0 };
const char *xulExts[] = { ".xul", 0 };
const char *htmExts[] = { ".htm", ".html", 0 };

static FileTypeRegistryEntry
    jpg( jpgExts, "MozillaJPEG", "Mozilla Joint Photographic Experts Group Image File" ),
    gif( gifExts, "MozillaGIF",  "Mozilla Graphics Interchange Format Image File" ),
    png( pngExts, "MozillaPNG",  "Mozilla Portable Network Graphic Image File" ),
    xml( xmlExts, "MozillaXML",  "Mozilla XML File Document" ),
    xul( xulExts, "MozillaXUL",  "Mozilla XUL File Document" );

static EditableFileTypeRegistryEntry
    mozillaMarkup( htmExts, "MozillaHTML", "Mozilla HyperText Markup Language Document" );

// Implementation of the nsIWindowsHooksSettings interface.
// Use standard implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS1( nsWindowsHooksSettings, nsIWindowsHooksSettings );

nsWindowsHooksSettings::nsWindowsHooksSettings() {
    NS_INIT_ISUPPORTS();
}

nsWindowsHooksSettings::~nsWindowsHooksSettings() {
}

// Generic getter.
NS_IMETHODIMP
nsWindowsHooksSettings::Get( PRBool *result, PRBool nsWindowsHooksSettings::*member ) {
    NS_ENSURE_ARG( result );
    NS_ENSURE_ARG( member );
    *result = this->*member;
    return NS_OK;
}

// Generic setter.
NS_IMETHODIMP
nsWindowsHooksSettings::Set( PRBool value, PRBool nsWindowsHooksSettings::*member ) {
    NS_ENSURE_ARG( member );
    this->*member = value;
    return NS_OK;
}

// Macros to define specific getter/setter methods.
#define DEFINE_GETTER_AND_SETTER( attr, member ) \
NS_IMETHODIMP \
nsWindowsHooksSettings::Get##attr ( PRBool *result ) { \
    return this->Get( result, &nsWindowsHooksSettings::member ); \
} \
NS_IMETHODIMP \
nsWindowsHooksSettings::Set##attr ( PRBool value ) { \
    return this->Set( value, &nsWindowsHooksSettings::member ); \
}

// Define all the getter/setter methods:
DEFINE_GETTER_AND_SETTER( IsHandlingHTML,   mHandleHTML   )
DEFINE_GETTER_AND_SETTER( IsHandlingJPEG,   mHandleJPEG   )
DEFINE_GETTER_AND_SETTER( IsHandlingGIF,    mHandleGIF    )
DEFINE_GETTER_AND_SETTER( IsHandlingPNG,    mHandlePNG    )
DEFINE_GETTER_AND_SETTER( IsHandlingXML,    mHandleXML    )
DEFINE_GETTER_AND_SETTER( IsHandlingXUL,    mHandleXUL    )
DEFINE_GETTER_AND_SETTER( IsHandlingHTTP,   mHandleHTTP   )
DEFINE_GETTER_AND_SETTER( IsHandlingHTTPS,  mHandleHTTPS  )
DEFINE_GETTER_AND_SETTER( IsHandlingFTP,    mHandleFTP    )
DEFINE_GETTER_AND_SETTER( IsHandlingCHROME, mHandleCHROME )
DEFINE_GETTER_AND_SETTER( IsHandlingGOPHER, mHandleGOPHER )
DEFINE_GETTER_AND_SETTER( ShowDialog,       mShowDialog   )
DEFINE_GETTER_AND_SETTER( HaveBeenSet,      mHaveBeenSet  )


// Implementation of the nsIWindowsHooks interface.
// Use standard implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS1( nsWindowsHooks, nsIWindowsHooks );

nsWindowsHooks::nsWindowsHooks() {
  NS_INIT_ISUPPORTS();
}

nsWindowsHooks::~nsWindowsHooks() {
}

// Internal GetPreferences.
NS_IMETHODIMP
nsWindowsHooks::GetSettings( nsWindowsHooksSettings **result ) {
    nsresult rv = NS_OK;

    // Validate input arg.
    NS_ENSURE_ARG( result );

    // Allocate prefs object.
    nsWindowsHooksSettings *prefs = *result = new nsWindowsHooksSettings;
    NS_ENSURE_TRUE( prefs, NS_ERROR_OUT_OF_MEMORY );

    // Got it, increment ref count.
    NS_ADDREF( prefs );

    // Get each registry value and copy to prefs structure.
    prefs->mHandleHTTP   = (void*)( BoolRegistryEntry( "isHandlingHTTP"   ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleHTTPS  = (void*)( BoolRegistryEntry( "isHandlingHTTPS"  ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleFTP    = (void*)( BoolRegistryEntry( "isHandlingFTP"    ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleCHROME = (void*)( BoolRegistryEntry( "isHandlingCHROME" ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleGOPHER = (void*)( BoolRegistryEntry( "isHandlingGOPHER" ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleHTML   = (void*)( BoolRegistryEntry( "isHandlingHTML"   ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleJPEG   = (void*)( BoolRegistryEntry( "isHandlingJPEG"   ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleGIF    = (void*)( BoolRegistryEntry( "isHandlingGIF"    ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandlePNG    = (void*)( BoolRegistryEntry( "isHandlingPNG"    ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleXML    = (void*)( BoolRegistryEntry( "isHandlingXML"    ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHandleXUL    = (void*)( BoolRegistryEntry( "isHandlingXUL"    ) ) ? PR_TRUE : PR_FALSE;
    prefs->mShowDialog   = (void*)( BoolRegistryEntry( "showDialog"       ) ) ? PR_TRUE : PR_FALSE;
    prefs->mHaveBeenSet  = (void*)( BoolRegistryEntry( "haveBeenSet"      ) ) ? PR_TRUE : PR_FALSE;

#ifdef DEBUG_law
NS_WARN_IF_FALSE( NS_SUCCEEDED( rv ), "GetPreferences failed" );
#endif

    return rv;
}

// Public interface uses internal plus a QI to get to the proper result.
NS_IMETHODIMP
nsWindowsHooks::GetSettings( nsIWindowsHooksSettings **_retval ) {
    // Allocate prefs object.
    nsWindowsHooksSettings *prefs;
    nsresult rv = this->GetSettings( &prefs );

    if ( NS_SUCCEEDED( rv ) ) {
        // QI to proper interface.
        rv = prefs->QueryInterface( NS_GET_IID( nsIWindowsHooksSettings ), (void**)_retval );
        // Release (to undo our Get...).
        NS_RELEASE( prefs );
    }

    return rv;
}

static PRBool misMatch( const PRBool &flag, const ProtocolRegistryEntry &entry ) {
    PRBool result = PR_FALSE;
    // Check if we care.
    if ( flag ) { 
        // Compare registry entry setting to what it *should* be.
        if ( entry.currentSetting() != entry.setting ) {
            result = PR_TRUE;
        }
    }

    return result;
}

// isAccessRestricted - Returns PR_TRUE iff this user only has restricted access
// to the registry keys we need to modify.
static PRBool isAccessRestricted() {
    char   subKey[] = "Software\\Mozilla - Test Key";
    PRBool result = PR_FALSE;
    DWORD  dwDisp = 0;
    HKEY   key;
    // Try to create/open a subkey under HKLM.
    DWORD rc = ::RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                                 subKey,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_WRITE,
                                 NULL,
                                 &key,
                                 &dwDisp );

    if ( rc == ERROR_SUCCESS ) {
        // Key was opened; first close it.
        ::RegCloseKey( key );
        // Delete it if we just created it.
        switch( dwDisp ) {
            case REG_CREATED_NEW_KEY:
                ::RegDeleteKey( HKEY_LOCAL_MACHINE, subKey );
                break;
            case REG_OPENED_EXISTING_KEY:
                break;
        }
    } else {
        // Can't create/open it; we don't have access.
        result = PR_TRUE;
    }

    return result;
}



// Implementation of method that checks settings versus registry and prompts user
// if out of synch.
NS_IMETHODIMP
nsWindowsHooks::CheckSettings( nsIDOMWindowInternal *aParent ) {
    nsresult rv = NS_OK;

    // Only do this once!
    static PRBool alreadyChecked = PR_FALSE;
    if ( alreadyChecked ) {
        return NS_OK;
    } else {
        alreadyChecked = PR_TRUE;
        // Don't check further if we don't have sufficient access.
        if ( isAccessRestricted() ) {
            return NS_OK;
        }
    }

    // Get settings.
    nsWindowsHooksSettings *settings;
    rv = this->GetSettings( &settings );

    if ( NS_SUCCEEDED( rv ) && settings ) {
        // If not set previously, set to defaults so that they are
        // set properly when/if the user says to.
        if ( !settings->mHaveBeenSet ) {
            settings->mHandleHTTP   = PR_TRUE;
            settings->mHandleHTTPS  = PR_TRUE;
            settings->mHandleFTP    = PR_TRUE;
            settings->mHandleCHROME = PR_TRUE;
            settings->mHandleGOPHER = PR_TRUE;
            settings->mHandleHTML   = PR_TRUE;
            settings->mHandleJPEG   = PR_TRUE;
            settings->mHandleGIF    = PR_TRUE;
            settings->mHandlePNG    = PR_TRUE;
            settings->mHandleXML    = PR_TRUE;
            settings->mHandleXUL    = PR_TRUE;

            settings->mShowDialog       = PR_TRUE;
        }

        // If launched with "-installer" then override mShowDialog.
        PRBool installing = PR_FALSE;
        if ( !settings->mShowDialog ) {
            // Get command line service.
            nsCID cmdLineCID = NS_COMMANDLINE_SERVICE_CID;
            nsCOMPtr<nsICmdLineService> cmdLineArgs( do_GetService( cmdLineCID, &rv ) );
            if ( NS_SUCCEEDED( rv ) && cmdLineArgs ) {
                // See if "-installer" was specified.
                nsXPIDLCString installer;
                rv = cmdLineArgs->GetCmdLineValue( "-installer", getter_Copies( installer ) );
                if ( NS_SUCCEEDED( rv ) && installer ) {
                    installing = PR_TRUE;
                }
            }
        }

        // First, make sure the user cares.
        if ( settings->mShowDialog || installing ) {
            // Look at registry setting for all things that are set.
            if ( misMatch( settings->mHandleHTTP,   http )
                 ||
                 misMatch( settings->mHandleHTTPS,  https )
                 ||
                 misMatch( settings->mHandleFTP,    ftp )
                 ||
                 misMatch( settings->mHandleCHROME, chrome )
                 ||
                 misMatch( settings->mHandleGOPHER, gopher )
                 ||
                 misMatch( settings->mHandleHTML,   mozillaMarkup )
                 ||
                 misMatch( settings->mHandleJPEG,   jpg )
                 ||
                 misMatch( settings->mHandleGIF,    gif )
                 ||
                 misMatch( settings->mHandlePNG,    png )
                 ||
                 misMatch( settings->mHandleXML,    xml )
                 ||
                 misMatch( settings->mHandleXUL,    xul ) ) {
                // Need to prompt user.
                // First:
                //   o We need the common dialog service to show the dialog.
                //   o We need the string bundle service to fetch the appropriate
                //     dialog text.
                nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
                nsCOMPtr<nsIPromptService> promptService( do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
                nsCOMPtr<nsIStringBundleService> bundleService( do_GetService( bundleCID, &rv ) );

                if ( promptService && bundleService ) {
                    // Next, get bundle that provides text for dialog.
                    nsCOMPtr<nsIStringBundle> bundle;
                    rv = bundleService->CreateBundle( "chrome://global/locale/nsWindowsHooks.properties",
                                                      getter_AddRefs( bundle ) );
                    if ( NS_SUCCEEDED( rv ) && bundle ) {
                        // Get text for dialog and checkbox label.
                        //
                        // The window text depends on whether this is the first time
                        // the user is seeing this dialog.
                        const char *textKey  = "promptText";
                        if ( !settings->mHaveBeenSet ) {
                            textKey  = "initialPromptText";
                        }
                        nsXPIDLString text, label, title;
                        if ( NS_SUCCEEDED( ( rv = bundle->GetStringFromName( NS_ConvertASCIItoUCS2( textKey ).get(),
                                                                             getter_Copies( text ) ) ) )
                             &&
                             NS_SUCCEEDED( ( rv = bundle->GetStringFromName( NS_LITERAL_STRING( "checkBoxLabel" ).get(),
                                                                             getter_Copies( label ) ) ) )
                             &&
                             NS_SUCCEEDED( ( rv = bundle->GetStringFromName( NS_LITERAL_STRING( "title" ).get(),
                                                                             getter_Copies( title ) ) ) ) ) {
                            // Got the text, now show dialog.
                            PRBool  showDialog = settings->mShowDialog;
                            PRInt32 dlgResult  = -1;
                            // No checkbox for initial display.
                            const PRUnichar *labelArg = 0;
                            if ( settings->mHaveBeenSet ) {
                                // Subsequent display uses label string.
                                labelArg = label;
                            }
                            // Note that the buttons need to be passed in this order:
                            //    o Yes
                            //    o Cancel
                            //    o No
                            rv = promptService->ConfirmEx(aParent, title, text,
                                                          (nsIPromptService::BUTTON_TITLE_YES * nsIPromptService::BUTTON_POS_0) +
                                                          (nsIPromptService::BUTTON_TITLE_CANCEL * nsIPromptService::BUTTON_POS_1) +
                                                          (nsIPromptService::BUTTON_TITLE_NO * nsIPromptService::BUTTON_POS_2),
                                                          nsnull, nsnull, nsnull, labelArg, &showDialog, &dlgResult);
                            
                            if ( NS_SUCCEEDED( rv ) ) {
                                // Did they say go ahead?
                                switch ( dlgResult ) {
                                    case 0:
                                        // User says: make the changes.
                                        // Remember "show dialog" choice.
                                        settings->mShowDialog = showDialog;
                                        // Apply settings; this single line of
                                        // code will do different things depending
                                        // on whether this is the first time (i.e.,
                                        // when "haveBeenSet" is false).  The first
                                        // time, this will set all prefs to true
                                        // (because that's how we initialized 'em
                                        // in GetSettings, above) and will update the
                                        // registry accordingly.  On subsequent passes,
                                        // this will only update the registry (because
                                        // the settings we got from GetSettings will
                                        // not have changed).
                                        //
                                        // BTW, the term "prefs" in this context does not
                                        // refer to conventional Mozilla "prefs."  Instead,
                                        // it refers to "Desktop Integration" prefs which
                                        // are stored in the windows registry.
                                        rv = SetSettings( settings );
                                        #ifdef DEBUG_law
                                            printf( "Yes, SetSettings returned 0x%08X\n", (int)rv );
                                        #endif
                                        break;

                                    case 2:
                                        // User says: Don't mess with Windows.
                                        // We update only the "showDialog" and
                                        // "haveBeenSet" keys.  Note that this will
                                        // have the effect of setting all the prefs
                                        // *off* if the user says no to the initial
                                        // prompt.
                                        BoolRegistryEntry( "haveBeenSet" ).set();
                                        if ( showDialog ) {
                                            BoolRegistryEntry( "showDialog" ).set();
                                        } else {
                                            BoolRegistryEntry( "showDialog" ).reset();
                                        }
                                        #ifdef DEBUG_law
                                            printf( "No, haveBeenSet=1 and showDialog=%d\n", (int)showDialog );
                                        #endif
                                        break;

                                    default:
                                        // User says: I dunno.  Make no changes (which
                                        // should produce the same dialog next time).
                                        #ifdef DEBUG_law
                                            printf( "Cancel\n" );
                                        #endif
                                        break;
                                }
                            }
                        }
                    }
                }
            }
            #ifdef DEBUG_law
            else { printf( "Registry and prefs match\n" ); }
            #endif
        }
        #ifdef DEBUG_law
        else { printf( "showDialog is false and not installing\n" ); }
        #endif

        // Release the settings.
        settings->Release();
    }

    return rv;
}

// Utility to set PRBool registry value from getter method.
nsresult putPRBoolIntoRegistry( const char* valueName,
                                nsIWindowsHooksSettings *prefs,
                                nsWindowsHooksSettings::getter memFun ) {
    // Use getter method to extract attribute from prefs.
    PRBool boolValue;
    (void)(prefs->*memFun)( &boolValue );
    // Convert to DWORD.
    DWORD  dwordValue = boolValue;
    // Store into registry.
    BoolRegistryEntry pref( valueName );
    nsresult rv = boolValue ? pref.set() : pref.reset();

    return rv;
}

/* void setPreferences (in nsIWindowsHooksSettings prefs); */
NS_IMETHODIMP
nsWindowsHooks::SetSettings(nsIWindowsHooksSettings *prefs) {
    nsresult rv = NS_ERROR_FAILURE;

    putPRBoolIntoRegistry( "isHandlingHTTP",   prefs, &nsIWindowsHooksSettings::GetIsHandlingHTTP );
    putPRBoolIntoRegistry( "isHandlingHTTPS",  prefs, &nsIWindowsHooksSettings::GetIsHandlingHTTPS );
    putPRBoolIntoRegistry( "isHandlingFTP",    prefs, &nsIWindowsHooksSettings::GetIsHandlingFTP );
    putPRBoolIntoRegistry( "isHandlingCHROME", prefs, &nsIWindowsHooksSettings::GetIsHandlingCHROME );
    putPRBoolIntoRegistry( "isHandlingGOPHER", prefs, &nsIWindowsHooksSettings::GetIsHandlingGOPHER );
    putPRBoolIntoRegistry( "isHandlingHTML",   prefs, &nsIWindowsHooksSettings::GetIsHandlingHTML );
    putPRBoolIntoRegistry( "isHandlingJPEG",   prefs, &nsIWindowsHooksSettings::GetIsHandlingJPEG );
    putPRBoolIntoRegistry( "isHandlingGIF",    prefs, &nsIWindowsHooksSettings::GetIsHandlingGIF );
    putPRBoolIntoRegistry( "isHandlingPNG",    prefs, &nsIWindowsHooksSettings::GetIsHandlingPNG );
    putPRBoolIntoRegistry( "isHandlingXML",    prefs, &nsIWindowsHooksSettings::GetIsHandlingXML );
    putPRBoolIntoRegistry( "isHandlingXUL",    prefs, &nsIWindowsHooksSettings::GetIsHandlingXUL );
    putPRBoolIntoRegistry( "showDialog",       prefs, &nsIWindowsHooksSettings::GetShowDialog );

    // Indicate that these settings have indeed been set.
    BoolRegistryEntry( "haveBeenSet" ).set();

    rv = SetRegistry();

    return rv;
}

// Get preferences and start handling everything selected.
NS_IMETHODIMP
nsWindowsHooks::SetRegistry() {
    nsresult rv = NS_OK;

    // Get raw prefs object.
    nsWindowsHooksSettings *prefs;
    rv = this->GetSettings( &prefs );

    NS_ENSURE_TRUE( NS_SUCCEEDED( rv ), rv );

    if ( prefs->mHandleHTML ) {
        (void) mozillaMarkup.set();
    } else {
        (void) mozillaMarkup.reset();
    }
    if ( prefs->mHandleJPEG ) {
        (void) jpg.set();
    } else {
        (void) jpg.reset();
    }
    if ( prefs->mHandleGIF ) {
        (void) gif.set();
    } else {
        (void) gif.reset();
    }
    if ( prefs->mHandlePNG ) {
        (void) png.set();
    } else {
        (void) png.reset();
    }
    if ( prefs->mHandleXML ) {
        (void) xml.set();
    } else {
        (void) xml.reset();
    }
    if ( prefs->mHandleXUL ) {
        (void) xul.set();
    } else {
        (void) xul.reset();
    }
    if ( prefs->mHandleHTTP ) {
        (void) http.set();
    } else {
        (void) http.reset();
    }
    if ( prefs->mHandleHTTPS ) {
        (void) https.set();
    } else {
        (void) https.reset();
    }
    if ( prefs->mHandleFTP ) {
        (void) ftp.set();
    } else {
        (void) ftp.reset();
    }
    if ( prefs->mHandleCHROME ) {
        (void) chrome.set();
    } else {
        (void) chrome.reset();
    }
    if ( prefs->mHandleGOPHER ) {
        (void) gopher.set();
    } else {
        (void) gopher.reset();
    }

    return NS_OK;
}

// nsIWindowsHooks.idl for documentation

NS_IMETHODIMP nsWindowsHooks::IsStartupTurboEnabled(PRBool *_retval)
{
	*_retval = PR_FALSE;
    HKEY key;
    LONG result = ::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &key );
    if ( result == ERROR_SUCCESS ) {
        result = ::RegQueryValueEx( key, NS_QUICKLAUNCH_RUN_KEY, NULL, NULL, NULL, NULL );
        ::RegCloseKey( key );
        if ( result == ERROR_SUCCESS )
          *_retval = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP nsWindowsHooks::StartupTurboEnable()
{
    PRBool startupFound;
    IsStartupTurboEnabled( &startupFound );

    if ( startupFound == PR_TRUE )
        return NS_OK;               // already enabled, no need to do this

    HKEY hKey;
    LONG res = ::RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL );
    if ( res != ERROR_SUCCESS )
      return NS_OK;
    char fileName[_MAX_PATH];
    int rv = ::GetModuleFileName( NULL, fileName, sizeof fileName );
    if ( rv ) {
      strcat( fileName, " -turbo" );
      ::RegSetValueEx( hKey, NS_QUICKLAUNCH_RUN_KEY, 0, REG_SZ, ( LPBYTE )fileName, strlen( fileName ) );
    }
    ::RegCloseKey( hKey );
    return NS_OK;
}

NS_IMETHODIMP nsWindowsHooks::StartupTurboDisable()
{
    PRBool startupFound;
    IsStartupTurboEnabled( &startupFound );
    if ( !startupFound )
        return NS_OK;               // already disabled, no need to do this

    HKEY hKey;
    LONG res = ::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey );
    if ( res != ERROR_SUCCESS )
      return NS_OK;
    res = ::RegDeleteValue( hKey, NS_QUICKLAUNCH_RUN_KEY );
    ::RegCloseKey( hKey );
    return NS_OK;
}

#if (_MSC_VER == 1100)
#define INITGUID
#include "objbase.h"
DEFINE_OLEGUID(IID_IPersistFile, 0x0000010BL, 0, 0);
#endif
