/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API)
 *
 * This interface was an attempt to supersede npapi.h, npupp.h but it failed. It
 * would have eliminated the need for glue files: npunix.c, npwin.cpp and
 * npmac.cpp that get linked with the plugin.
 *
 * This is the master header file that includes most of the other headers
 * you'd have needed to write a plugin.
 */

/**
 * The following diagram depicts the relationships between various objects 
 * implementing the new plugin interfaces. QueryInterface can be used to switch
 * between interfaces in the same box:
 *
 *
 *         the plugin (only 1)                        
 *  +----------------------+                                             
 *  | nsIPlugin or         |<- - - - - -NSGetFactory()
 *  +----------------------+                                            
 *    |
 *    |                                                                  
 *    |              instances (many)
 *    |          +-------------------+
 *    |          | nsIPluginInstance |+
 *    |          +-------------------+|
 *    |            +------|-----------+
 *    |                   |
 *    | PLUGIN SIDE       |peer
 *~~~~|~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    | BROWSER SIDE      |
 *    |                   v
 *    |     +---------------------------------+
 *    |     | nsIPluginInstancePeer           |+
 *    |     | nsIWindowlessPluginInstancePeer ||
 *    |     | nsIPluginTagInfo                ||
 *    |     | nsIPluginTagInfo2               ||
 *    |     +---------------------------------+|                  
 *    |       +--------------------------------+                  
 *    |                                                
 *    |                                                
 *    v    the browser (only 1)                             
 *  +---------------------+                                        
 *  | nsIPluginManager    |                                        
 *  | nsIPluginManager2   |                                        
 *  | nsIFileUtilities    |                                        
 *  | nsIPref             |                                        
 *  | nsICacheManager ... |                            
 *  +---------------------+                                        
 */ 

#ifndef nsplugins_h___
#define nsplugins_h___
#include "nsIComponentManager.h"       // for NSGetFactory

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>Interfaces which must be implemented by a plugin</B>
 * These interfaces have NPP equivalents in pre-5.0 browsers (see npapi.h).
 */

/**                                                               
 * A plugin object is used to create new plugin instances. It manages the
 * global state of all plugins with the same implementation.      
 */                                                               
#include "nsIPlugin.h"

/**
 * A plugin instance represents a particular activation of a plugin on a page.
 */
#include "nsIPluginInstance.h"

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>Interfaces implemented by the browser:
 * These interfaces have NPN equivalents in pre-5.0 browsers (see npapi.h).
 */
////////////////////////////////////////////////////////////////////////////////

/**
 * The plugin manager which is the main point of interaction with the browser 
 * and provides general operations needed by a plugin.
 */
#include "nsIPluginManager.h"

/**
 * A plugin instance peer gets created by the browser and associated with each
 * plugin instance to represent tag information and other callbacks needed by
 * the plugin instance.
 */
#include "nsIPluginInstancePeer.h"

/**
 * The nsIPluginTagInfo interface provides information about the html tag
 * that was used to instantiate the plugin instance. 
 *
 * To obtain: QueryInterface on nsIPluginInstancePeer
 */
#include "nsIPluginTagInfo.h"

/**
 * The nsIWindowlessPluginInstancePeer provides additional operations for 
 * windowless plugins. 
 *
 * To obtain: QueryInterface on nsIPluginInstancePeer
 */
#include "nsIWindowlessPlugInstPeer.h"

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>Interfaces implemented by the browser (new for 5.0):
 */
////////////////////////////////////////////////////////////////////////////////

/**
 * The nsIPluginManager2 interface provides additional plugin manager features
 * only available in Communicator 5.0. 
 *
 * To obtain: QueryInterface on nsIPluginManager
 */
#include "nsIPluginManager2.h"

/**
 * The nsIFileUtilities interface provides operations to manage temporary
 * files and directories.
 *
 * To obtain: QueryInterface on nsIPluginManager
 */
#include "nsIFileUtilities.h"

/**
 * The nsIPluginTagInfo2 interface provides additional html tag information
 * only available in Communicator 5.0. 
 *
 * To obtain: QueryInterface on nsIPluginTagInfo
 */
#include "nsIPluginTagInfo2.h"

#include "nsIOutputStream.h"

////////////////////////////////////////////////////////////////////////////////
#endif // nsplugins_h___
