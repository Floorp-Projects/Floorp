/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is The Original Code is Mozilla Automated Testing Code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bob Clary <bob@bclary.com>
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

function AppInfo()
{
  // See http://developer.mozilla.org/en/docs/Using_nsIXULAppInfo
  var appInfo;

  this.vendor          = 'unknown';
  this.name            = 'unknown';
  this.ID              = 'unknown';
  this.version         = 'unknown';
  this.appBuildID      = 'unknown';
  this.platformVersion = 'unknown';
  this.platformBuildID = 'unknown';

  try
  {
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

    if('@mozilla.org/xre/app-info;1' in Components.classes) 
    {
      // running under Mozilla 1.8 or later
      appInfo = Components
                  .classes['@mozilla.org/xre/app-info;1']
                  .getService(Components.interfaces.nsIXULAppInfo);

      this.vendor          = appInfo.vendor;
      this.name            = appInfo.name;
      this.ID              = appInfo.ID;
      this.version         = appInfo.version;
      this.appBuildID      = appInfo.appBuildID;
      this.platformVersion = appInfo.platformVersion;
      this.platformBuildID = appInfo.platformBuildID;
    }
  }
  catch(e)
  {
  }

  if (this.vendor == 'unknown')
  {
    var ua = navigator.userAgent;
    var cap = ua.match(/rv:([\d.ab+]+).*Gecko\/(\d{8,8}) ([\S]+)\/([\d.]+)/);

    if (cap && cap.length == 5)
    {
      this.vendor          = navigator.vendor ? navigator.vendor : 'Mozilla';
      this.name            = cap[3];
      this.version         = cap[4];
      this.appBuildID      = cap[2] + '00';
      this.platformVersion = cap[1];
      this.platformBuildID =  this.appBuildID;
    }
  }
}
