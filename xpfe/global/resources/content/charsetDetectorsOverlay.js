/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
function debug(str)
{
  dump( str );
}
function LoadDetectorsMenu()
  {
    debug("run LoadDetectorsMenu()\n");
    var Registry = Components.classes['@mozilla.org/registry-viewer;1'].createInstance();
    Registry = Registry.QueryInterface(Components.interfaces.nsIRegistryDataSource);

    Registry.openWellKnownRegistry(Registry.ApplicationComponentRegistry);

    Registry = Registry.QueryInterface(Components.interfaces.nsIRDFDataSource);

    var menu = document.getElementById('CharsetDetectorsMenu');
    if (menu) {
      menu.database.AddDataSource(Registry);
      menu.setAttribute('ref', 'urn:mozilla-registry:key:/software/netscape/intl/charsetdetector');
    }
  }
function SelectDetectors( event )
  {
   uri =  event.target.getAttribute("id");
   debug(uri + "\n");
   pref = Components.classes['@mozilla.org/preferences;1'];
   prefvalue = uri.substring(
                     'urn:mozilla-registry:key:/software/netscape/intl/charsetdetector/'.length
                     ,uri.length);
   if("off" == prefvalue) { // "off" is special value to turn off the detectors
      prefvalue = "";
   }
   debug(prefvalue + "\n");

   // if all else fails, use trusty "about:blank" as the start page
   if (pref) {
        debug("get pref\n");
        pref = pref.getService();
        pref = pref.QueryInterface(Components.interfaces.nsIPref);
   }


   if (pref) {
       debug("get pref 2\n");
       pref.SetCharPref("intl.charset.detector", prefvalue);
       window._content.location.reload();
   }
  }

/* this is really hacky, but waterson say it will work */
setTimeout("LoadDetectorsMenu()", 10000);
