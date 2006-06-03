/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bill Law     <law@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
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

#include <windows.h>

// Where Mozilla stores its own registry values.
const char * const mozillaKeyName = "Software\\Mozilla\\Desktop";

static const char shortcutSuffix[] = " -url \"%1\"";
static const char chromeSuffix[] = " -chrome \"%1\"";
static const char iconSuffix[] = ",0";

// Returns the (fully-qualified) name of this executable.
static nsCString thisApplication() {
    static nsCAutoString result;

    if ( result.IsEmpty() ) {
        char buffer[MAX_PATH] = { 0 };
    	DWORD len = ::GetModuleFileName( NULL, buffer, sizeof buffer );
        len = ::GetShortPathName( buffer, buffer, sizeof buffer );
    
        result = buffer;
        ToUpperCase(result);
    }

    return result;
}

// Returns the "short" name of this application (in upper case).  This is for
// use as a StartMenuInternet value.
static nsCString shortAppName() {
    static nsCAutoString result;
    
    if ( result.IsEmpty() ) { 
        // Find last backslash in thisApplication().
        nsCAutoString thisApp( thisApplication() );
        PRInt32 n = thisApp.RFind( "\\" );
        if ( n != kNotFound ) {
            // Use what comes after the last backslash.
            result = (const char*)thisApp.get() + n + 1;
        } else {
            // Use entire string.
            result = thisApp;
        }
    }

    return result;
}

// RegistryEntry
//
// Generic registry entry (no saving of previous values).  Each is comprised of:
//      o A base HKEY
//      o A subkey name.
//      o An optional value name (empty for the "default" value).
//      o The registry setting we'd like this entry to have when set.
struct RegistryEntry {
    HKEY        baseKey;   // e.g., HKEY_CURRENT_USER
    PRBool      isNull;    // i.e., should use ::RegDeleteValue
    nsCString   keyName;   // Key name.
    nsCString   valueName; // Value name (can be empty, which implies NULL).
    nsCString   setting;   // What we set it to (UTF-8). This had better be 
                           // nsString to avoid extra string copies, but
                           // this code is not performance-critical. 

    RegistryEntry( HKEY baseKey, const char* keyName, const char* valueName, const char* setting )
        : baseKey( baseKey ), isNull( setting == 0 ), keyName( keyName ), valueName( valueName ), setting( setting ? setting : "" ) {
    }

    PRBool     isAlreadySet() const;
    nsresult   set();
    nsresult   reset();
    nsCString  currentSetting( PRBool *currentUndefined = 0 ) const;

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
    operator PRBool();
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

// ProtocolIconRegistryEntry
//
// For setting icon entries for a given Internet Shortcut protocol.
// The key name is calculated as
// HKEY_LOCAL_MACHINE\Software\Classes\protocol\DefaultIcon.
// The setting is this executable (with appropriate suffix).
struct ProtocolIconRegistryEntry : public SavedRegistryEntry {
    nsCString protocol;
    ProtocolIconRegistryEntry( const char* aprotocol )
        : SavedRegistryEntry( HKEY_LOCAL_MACHINE, "", "", thisApplication().get() ),
          protocol( aprotocol ) {
        keyName = NS_LITERAL_CSTRING("Software\\Classes\\") + protocol + NS_LITERAL_CSTRING("\\DefaultIcon");

        // Append appropriate suffix to setting.
        setting += iconSuffix;
    }
};

// DDERegistryEntry
//
// Like a protocol registry entry, but for the shell\open\ddeexec subkey.
//
// We need to remove this subkey entirely to ensure we work properly with
// various programs on various platforms (see Bugzilla bugs 59078, 58770, etc.).
//
// We don't try to save everything, though.  We do save the known useful info
// under the ddeexec subkey:
//     ddeexec\@
//     ddeexec\NoActivateHandler
//     ddeexec\Application\@
//     ddeexec\Topic\@
//
// set/reset save/restore these values and remove/restore the ddeexec subkey
struct DDERegistryEntry : public SavedRegistryEntry {
    DDERegistryEntry( const char *protocol )
        : SavedRegistryEntry( HKEY_LOCAL_MACHINE, "", "", 0 ),
          activate( HKEY_LOCAL_MACHINE, "", "NoActivateHandler", 0 ),
          app( HKEY_LOCAL_MACHINE, "", "", 0 ),
          topic( HKEY_LOCAL_MACHINE, "", "", 0 ) {
        // Derive keyName from protocol.
        keyName = "Software\\Classes\\";
        keyName += protocol;
        keyName += "\\shell\\open\\ddeexec";
        // Set subkey names.
        activate.keyName = keyName;
        app.keyName = keyName;
        app.keyName += "\\Application";
        topic.keyName = keyName;
        topic.keyName += "\\Topic";
    }
    nsresult set();
    nsresult reset();
    SavedRegistryEntry activate, app, topic;
};

// FileTypeRegistryEntry
//
// For setting entries relating to a file extension (or extensions).
// This object itself is for the "file type" associated with the extension.
// Set/reset manage the mapping from extension to the file type, as well.
// The description is taken from defDescKey if available. Otherwise desc 
// is used.
struct FileTypeRegistryEntry : public ProtocolRegistryEntry {
    nsCString fileType;
    const char **ext;
    nsCString desc;
    nsCString defDescKey;
    nsCString iconFile;
    FileTypeRegistryEntry ( const char **ext, const char *fileType, 
        const char *desc, const char *defDescKey, const char *iconFile )
        : ProtocolRegistryEntry( fileType ),
          fileType( fileType ),
          ext( ext ),
          desc( desc ),
          defDescKey(defDescKey),
          iconFile(iconFile) {
    }
    nsresult set();
    nsresult reset();
};

// EditableFileTypeRegistryEntry
//
// Extends FileTypeRegistryEntry by setting an additional handler for an "edit" command.
struct EditableFileTypeRegistryEntry : public FileTypeRegistryEntry {
    EditableFileTypeRegistryEntry( const char **ext, const char *fileType, 
        const char *desc, const char *defDescKey, const char *iconFile )
        : FileTypeRegistryEntry( ext, fileType, desc, defDescKey, iconFile ) {
    }
    nsresult set();
};
