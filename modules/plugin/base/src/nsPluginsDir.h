/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsPluginsDir_h___
#define nsPluginsDir_h___

#include "nsError.h"
#include "nsFileSpec.h"

/**
 * Provides a cross-platform way to obtain the location of the plugins
 * directory. Once constructed, can be used with nsDirectoryIterator to
 * scan the plugins directory. An nsPluginFileSpec can be constructed from the
 * nsFileSpec returned by the iterator.
 */

/* Where to seek for plugins dir */
#define PLUGINS_DIR_LOCATION_AUTO       0
#define PLUGINS_DIR_LOCATION_MOZ_LOCAL  1
#define PLUGINS_DIR_LOCATION_4DOTX      2
#define PLUGINS_DIR_LOCATION_MAC_SYSTEM_PLUGINS_FOLDER 3
#define PLUGINS_DIR_LOCATION_JAVA_JRE   4

class nsPluginsDir : public nsFileSpec {
public:
	/**
	 * Locates the plugins directory in a platform-dependent manner.
	 */
	nsPluginsDir(PRUint16 location = PLUGINS_DIR_LOCATION_AUTO);
  nsPluginsDir(nsFileSpec plugDir) :
     nsFileSpec(plugDir) {  }
	virtual ~nsPluginsDir();

	/**
	 * Determines whether or not the given file is actually a plugin file.
	 */
	PRBool IsPluginFile(const nsFileSpec& fileSpec);
};

struct PRLibrary;

struct nsPluginInfo {
	PRUint32 fPluginInfoSize;	// indicates how large the structure is currently.
	char* fName;				// name of the plugin
	char* fDescription;			// etc.
	PRUint32 fVariantCount;
	char** fMimeTypeArray;
	char** fMimeDescriptionArray;
	char** fExtensionArray;
	char* fFileName;
	char* fFullPath;
};

/**
 * Provides cross-platform access to a plugin file. Deals with reading
 * properties from the plugin file, and loading the plugin's shared
 * library. Insulates core nsIPluginHost implementations from these
 * details.
 */
class nsPluginFile : public nsFileSpec {
  PRLibrary* pLibrary;
public:
	/**
	 * If spec corresponds to a valid plugin file, constructs a reference
	 * to a plugin file on disk. Plugins are typically located using the
	 * nsPluginsDir class.
	 */
	nsPluginFile(const nsFileSpec& spec);
	virtual ~nsPluginFile();

	/**
	 * Loads the plugin into memory using NSPR's shared-library loading
	 * mechanism. Handles platform differences in loading shared libraries.
	 */
	nsresult LoadPlugin(PRLibrary* &outLibrary);

	/**
	 * Obtains all of the information currently available for this plugin.
	 */
	nsresult GetPluginInfo(nsPluginInfo &outPluginInfo);

  /**
	 * Should be called after GetPluginInfo to free all allocated stuff
	 */
	nsresult FreePluginInfo(nsPluginInfo &PluginInfo);

	// Open the resource fork for the plugin
	short OpenPluginResource(void);
};

#endif /* nsPluginsDir_h___ */
