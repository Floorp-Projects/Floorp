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
function LoadDetectorsMenu()
  {
    dump("run LoadDetectorsMenu");
    var Registry = Components.classes['component://netscape/registry-viewer'].createInstance();
    Registry = Registry.QueryInterface(Components.interfaces.nsIRegistryDataSource);

    Registry.openWellKnownRegistry(Registry.ApplicationComponentRegistry);

    Registry = Registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var menu = document.getElementById('CharsetDetectorsMenu');
    menu.database.AddDataSource(Registry);

    menu.setAttribute('ref', 'urn:mozilla-registry:key:/software/netscape/intl/charsetdetector');
  }
function SelectDetectors( event )
  {
   dump ( event.target );
   pref = Components.classes['component://netscape/preferences'];

   // if all else fails, use trusty "about:blank" as the start page
   if (pref) {
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
   }


   if (pref) {
       pref.SetChartPref("intl.charset.detector", "japsm");
   }
  }

/* this is really hacky, but waterson say it will work */
setTimeout("LoadDetectorsMenu()", 30000);
