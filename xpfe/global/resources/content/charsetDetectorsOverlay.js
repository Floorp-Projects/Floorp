/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
   pref = Components.classes['@mozilla.org/preferences-service;1'];
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
        pref = pref.QueryInterface(Components.interfaces.nsIPrefBranch);
   }


   if (pref) {
       debug("get pref 2\n");
       pref.setCharPref("intl.charset.detector", prefvalue);
       window._content.location.reload();
   }
  }

/* this is really hacky, but waterson say it will work */
setTimeout("LoadDetectorsMenu()", 10000);
