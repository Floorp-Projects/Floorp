/* -*- Mode: Java; tab-width: 4; c-basic-offset: 4; -*-
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

/*

   Script for the registry viewer window

*/


function OnLoad()
{
    var registry = Components.classes['component://netscape/registry-viewer'].createInstance();
    registry = registry.QueryInterface(Components.interfaces.nsIRegistryDataSource);

    registry.openWellKnownRegistry(1); // application component registry

    var datasource = registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var tree = document.getElementById('tree');
    tree.database.AddDataSource(datasource);

    tree.setAttribute('ref', 'urn:mozilla-registry:key:/');
}  
