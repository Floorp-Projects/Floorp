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
#include <windows.h>
#include <string.h>

#include "nsString.h"

// Where Mozilla stores its own registry values.
const char * const mozillaKeyName = "Software\\Mozilla\\Desktop";

static const char shortcutSuffix[] = " -url \"%1\"";
static const char chromeSuffix[] = " -chrome \"%1\"";

// Returns the (fully-qualified) name of this executable.
static nsCString thisApplication() {
    static nsCAutoString result;

    if ( result.IsEmpty() ) {
        char buffer[MAX_PATH] = { 0 };
    	DWORD len = ::GetModuleFileName( NULL, buffer, sizeof buffer );
        len = ::GetShortPathName( buffer, buffer, sizeof buffer );
    
        result = buffer;
        result.ToUpperCase();
    }

    return result;
}

// Returns this

// RegistryEntry
//
// Generic registry entry (no saving of previous values).  Each is comprised of:
//      o A base HKEY
//      o A subkey name.
//      o An optional value name (empty for the "default" value).
//      o The registry setting we'd like this entry to have when set.
struct RegistryEntry {
    HKEY        baseKey;   // e.g., HKEY_CURRENT_USER
    nsCString   keyName;   // Key name.
    nsCString   valueName; // Value name (can be empty, which implies NULL).
    nsCString   setting;   // What we set it to.

    RegistryEntry( HKEY baseKey, const char* keyName, const char* valueName, const char* setting )
        : baseKey( baseKey ), keyName( keyName ), valueName( valueName ), setting( setting ) {
    }

    PRBool     isAlreadySet() const;
    nsresult   set();
    nsresult   reset();
    nsCString  currentSetting() const;

    // Return value name in proper form for passing to ::Reg functions
    // (i.e., emptry string is converted to a NULL pointer).
    const char* valueNameArg() const {
        return valueName.IsEmpty() ? NULL : valueName.get();
    }

    nsCString  fullName() const;
};

// BoolRegistryEntry
// 
// These are used to store the "windows integration" preferences.
// You can query the value via operator void* (i.e., if ( boolPref )... ).
// These are stored under HKEY_LOCAL_MACHINE\Software\Mozilla\Desktop.
// Set sets the stored value to "1".  Reset deletes it (which implies 0).
struct BoolRegistryEntry : public RegistryEntry {
    BoolRegistryEntry( const char *name )
        : RegistryEntry( HKEY_LOCAL_MACHINE, mozillaKeyName, name, "1" ) {
    }
    operator void*();
};

// SavedRegistryEntry
//
// Like a plain RegistryEntry, but set/reset save/restore the
// it had before we set it.
struct SavedRegistryEntry : public RegistryEntry {
    SavedRegistryEntry( HKEY baseKey, const char *keyName, const char *valueName, const char *setting )
        : RegistryEntry( baseKey, keyName, valueName, setting ) {
    }
    nsresult set();
    nsresult reset();
};

// ProtocolRegistryEntry
//
// For setting entries for a given Internet Shortcut protocol.
// The key name is calculated as
// HKEY_LOCAL_MACHINE\Software\Classes\protocol\shell\open\command.
// The setting is this executable (with appropriate suffix).
// Set/reset are trickier in this case.
struct ProtocolRegistryEntry : public SavedRegistryEntry {
    nsCString protocol;
    ProtocolRegistryEntry( const char* protocol )
        : SavedRegistryEntry( HKEY_LOCAL_MACHINE, "", "", thisApplication().get() ),
          protocol( protocol ) {
        keyName = "Software\\Classes\\";
        keyName += protocol;
        keyName += "\\shell\\open\\command";

        // Append appropriate suffix to setting.
        if ( this->protocol.Equals( "chrome" ) || this->protocol.Equals( "MozillaXUL" ) ) {
            // Use "-chrome" command line flag.
            setting += chromeSuffix;
        } else {
            // Use standard "-url" command line flag.
            setting += shortcutSuffix;
        }
    }
    nsresult set();
    nsresult reset();
};

// DDERegistryEntry
//
// Like a protocol registry entry, but for the shell\open\ddeexec subkey.
//
// We no longer wire in DDE into the registry so we don't set the
// application name or topic.  We do *reset* them (to clean up previous
// installations).
//
// Reset handle setting/resetting various ddeexec sub-entries, also.
struct DDERegistryEntry : public SavedRegistryEntry {
    DDERegistryEntry( const char *protocol )
        : SavedRegistryEntry( HKEY_LOCAL_MACHINE, "", "", "" ),
          app( HKEY_LOCAL_MACHINE, "", "", "Mozilla" ),
          topic( HKEY_LOCAL_MACHINE, "", "", "WWW_OpenURL" ) {
        // Derive keyName from protocol.
        keyName = "Software\\Classes\\";
        keyName += protocol;
        keyName += "\\shell\\open\\ddeexec";
        // Set subkey names.
        app.keyName = keyName;
        app.keyName += "\\Application";
        topic.keyName = keyName;
        topic.keyName += "\\Topic";
    }
    nsresult reset();                                     
    SavedRegistryEntry app, topic;
};

// FileTypeRegistryEntry
//
// For setting entries relating to a file extension (or extensions).
// This object itself is for the "file type" associated with the extension.
// Set/reset manage the mapping from extension to the file type, as well.
struct FileTypeRegistryEntry : public ProtocolRegistryEntry {
    nsCString fileType;
    const char **ext;
    nsCString desc;
    FileTypeRegistryEntry ( const char **ext, const char *fileType, const char *desc )
        : ProtocolRegistryEntry( fileType ),
          fileType( fileType ),
          ext( ext ),
          desc( desc ) {
    }
    nsresult set();
    nsresult reset();
};

// EditableFileTypeRegistryEntry
//
// Extends FileTypeRegistryEntry by setting an additional handler for an "edit" command.
struct EditableFileTypeRegistryEntry : public FileTypeRegistryEntry {
    EditableFileTypeRegistryEntry( const char **ext, const char *fileType, const char *desc )
        : FileTypeRegistryEntry( ext, fileType, desc ) {
    }
    nsresult set();
};

// Generate the "full" name of this registry entry.
nsCString RegistryEntry::fullName() const {
    nsCString result;
    if ( baseKey == HKEY_CURRENT_USER ) {
        result = "HKEY_CURRENT_USER\\";
    } else if ( baseKey == HKEY_LOCAL_MACHINE ) {
        result = "HKEY_LOCAL_MACHINE\\";
    } else {
        result = "\\";
    }
    result += keyName;
    if ( !valueName.IsEmpty() ) {
        result += "[";
        result += valueName;
        result += "]";
    }
    return result;
}

// Tests whether registry entry already has desired setting.
PRBool RegistryEntry::isAlreadySet() const {
    PRBool result = FALSE;

    nsCAutoString current( currentSetting() );

    result = ( current == setting );

    return result;
}

// Gives registry entry the desired setting.
nsresult RegistryEntry::set() {
#ifdef DEBUG_law
printf( "Setting %s=%s\n", fullName().get(), setting.get() );
#endif
    nsresult result = NS_ERROR_FAILURE;

    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName.get(), &key );

    // See if key doesn't exist yet...
    if ( rc == ERROR_FILE_NOT_FOUND ) {
        rc = ::RegCreateKey( baseKey, keyName.get(), &key );
    }
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096] = { 0 };
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( strcmp( setting.get(), buffer ) != 0 ) {
            rc = ::RegSetValueEx( key, valueNameArg(), NULL, REG_SZ, (LPBYTE)setting.get(), setting.Length() );
#ifdef DEBUG_law
NS_WARN_IF_FALSE( rc == ERROR_SUCCESS, fullName().get() );
#endif
            if ( rc == ERROR_SUCCESS ) {
                result = NS_OK;
            }
        } else {
            // Already has desired setting.
            result = NS_OK;
        }
        ::RegCloseKey( key );
    } else {
#ifdef DEBUG_law
NS_WARN_IF_FALSE( rc == ERROR_SUCCESS, fullName().get() );
#endif
    }
    return result;
}

// Get current setting, set new one, then save the previous.
nsresult SavedRegistryEntry::set() {
    nsresult rv = NS_OK;
    nsCAutoString prev( currentSetting() );
    // See if value is changing.
    if ( setting != prev ) {
        // Set new.
        rv = RegistryEntry::set();
        if ( NS_SUCCEEDED( rv ) ) {
            // Save old.
            RegistryEntry( HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", fullName().get(), prev.get() ).set();
        }
    }
    return rv;
}

// Set this entry and its corresponding DDE entry.  The DDE entry
// must be turned off to stop Windows from trying to use DDE.
nsresult ProtocolRegistryEntry::set() {
    // Set this entry.
    nsresult rv = SavedRegistryEntry::set();

    // Save and set corresponding DDE entry(ies).
    DDERegistryEntry( protocol.get() ).set();

    return rv;
}

// Not being a "saved" entry, we can't restore, so just delete it.
nsresult RegistryEntry::reset() {
    HKEY key;
    LONG rc = ::RegOpenKey( baseKey, keyName.get(), &key );
    if ( rc == ERROR_SUCCESS ) {
        rc = ::RegDeleteValue( key, valueNameArg() );
    }
    return NS_OK;
}

// Resets registry entry to the saved value (if there is one).  We first
// ensure that we still "own" that entry (by comparing its value to what
// we would set it to).
nsresult SavedRegistryEntry::reset() {
    nsresult result = NS_OK;

    // Get current setting for this key/value.
    nsCAutoString current( currentSetting() );

    // Test if we "own" it.
    if ( current == setting ) {
        // Unset it, then.  First get saved value it had previously.
        RegistryEntry saved = RegistryEntry( HKEY_LOCAL_MACHINE, mozillaKeyName, fullName().get(), "" );
        saved.setting = saved.currentSetting();
        if ( !saved.setting.IsEmpty() ) {
            // Set to previous value.
            setting = saved.setting;
            result = RegistryEntry::set();
            // Remove saved entry.
            saved.reset();
        } else {
            // Just delete this key/value.
            result = RegistryEntry::reset();
        }
    }

    return result;
}

// Restore this entry and corresponding DDE entry.
nsresult ProtocolRegistryEntry::reset() {
    // Restore this entry.
    nsresult rv = SavedRegistryEntry::reset();

    // Do same for corresponding DDE entry.
    DDERegistryEntry( protocol.get() ).reset();

    return rv;
}

// Reset the main (ddeexec) value but also the Application and Topic.
// We reset the app/topic even though we no longer set them.  This
// handles cases where the user installed a prior version, and then
// upgraded.
nsresult DDERegistryEntry::reset() {
    nsresult rv = SavedRegistryEntry::reset();
    rv = app.reset();
    rv = topic.reset();
    return rv;
}

// Return current setting for this registry entry.
nsCString RegistryEntry::currentSetting() const {
    nsCString result;

    HKEY   key;
    LONG   rc = ::RegOpenKey( baseKey, keyName.get(), &key );
    if ( rc == ERROR_SUCCESS ) {
        char buffer[4096];
        DWORD len = sizeof buffer;
        rc = ::RegQueryValueEx( key, valueNameArg(), NULL, NULL, (LPBYTE)buffer, &len );
        if ( rc == ERROR_SUCCESS ) {
            result = buffer;
        }
        ::RegCloseKey( key );
    }

    return result;
}

// For each file extension, map it to this entry's file type.
// Set the file type so this application opens files of that type.
nsresult FileTypeRegistryEntry::set() {
    nsresult rv = NS_OK;

    // Set file extensions.
    for ( int i = 0; NS_SUCCEEDED( rv ) && ext[i]; i++ ) {
        nsCAutoString thisExt( "Software\\Classes\\" );
        thisExt += ext[i];
        rv = SavedRegistryEntry( HKEY_LOCAL_MACHINE, thisExt.get(), "", fileType.get() ).set();
    }

    // If OK, set file type opener.
    if ( NS_SUCCEEDED( rv ) ) {
        rv = ProtocolRegistryEntry::set();

        // If we just created this file type entry, set description and default icon.
        if ( NS_SUCCEEDED( rv ) ) {
            nsCAutoString descKey( "Software\\Classes\\" );
            descKey += protocol;
            RegistryEntry descEntry( HKEY_LOCAL_MACHINE, descKey.get(), NULL, desc.get() );
            if ( descEntry.currentSetting().IsEmpty() ) {
                descEntry.set();
            }
            nsCAutoString iconKey( "Software\\Classes\\" );
            iconKey += protocol;
            iconKey += "\\DefaultIcon";

            RegistryEntry iconEntry( HKEY_LOCAL_MACHINE, iconKey.get(), NULL,
                                     nsCAutoString( thisApplication() + NS_LITERAL_CSTRING(",0") ).get() );

            if ( iconEntry.currentSetting().IsEmpty() ) {
                iconEntry.set();
            }
        }
    }

    return rv;
}

// Basically, the inverse of set().
// First, reset the opener for this entry's file type.
// Then, reset the file type associated with each extension.
nsresult FileTypeRegistryEntry::reset() {
    nsresult rv = ProtocolRegistryEntry::reset();

    for ( int i = 0; ext[ i ]; i++ ) {
        nsCAutoString thisExt( "Software\\Classes\\" );
        thisExt += ext[i];
        (void)SavedRegistryEntry( HKEY_LOCAL_MACHINE, thisExt.get(), "", fileType.get() ).reset();
    }

    return rv;
}

// Do inherited set() and also set key for edit (with -edit option).
//
// Note: We make the rash assumption that we "own" this filetype (aka "protocol").
// If we ever start commandeering some other file type then this may have to be
// rethought.  The solution is to override reset() and undo this (and make the
// "edit" entry a SavedRegistryEntry).
nsresult EditableFileTypeRegistryEntry::set() {
    nsresult rv = FileTypeRegistryEntry::set();
    if ( NS_SUCCEEDED( rv ) ) {
        nsCAutoString editKey( "Software\\Classes\\" );
        editKey += protocol;
        editKey += "\\shell\\edit\\command";
        nsCAutoString editor( thisApplication() );
        editor += " -edit \"%1\"";
        rv = RegistryEntry( HKEY_LOCAL_MACHINE, editKey.get(), "", editor.get() ).set();
    }
    return rv;
}

// Convert current registry setting to boolean.
BoolRegistryEntry::operator void*() {
    return (void*)( currentSetting().Equals( "1" ) ? PR_TRUE : PR_FALSE );
}
