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
 *   Scott MacGregor <mscott@netscape.com>
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

var gCurrentHeight = 0;
var gFinalHeight = 50;
var gSlideIncrement = 1;
var gSlideTime = 10;
var gOpenTime = 3000; // total time the alert should stay up once we are done animating.

var gAlertListener = null;
var gAlertTextClickable = false;
var gAlertCookie = "";

function prefillAlertInfo()
{
  // unwrap all the args....
  // arguments[0] --> the image src url
  // arguments[1] --> the alert title
  // arguments[2] --> the alert text
  // arguments[3] --> is the text clickable? 
  // arguments[4] --> the alert cookie to be passed back to the listener
  // arguments[5] --> an optional callback listener (nsIObserver)

  document.getElementById('alertImage').setAttribute('src', window.arguments[0]);
  document.getElementById('alertTitleLabel').setAttribute('value', window.arguments[1]);
  document.getElementById('alertTextLabel').setAttribute('value', window.arguments[2]);
  gAlertTextClickable = window.arguments[3];
  gAlertCookie = window.arguments[4];

  if (gAlertTextClickable)
    document.getElementById('alertTextLabel').setAttribute('clickable', true);
  
  // the 5th argument is optional
  gAlertListener = window.arguments[5];
}

function onAlertLoad()
{
  // read out our initial settings from prefs.
  try 
  {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
    var prefBranch = prefService.getBranch(null);
    gSlideIncrement = prefBranch.getIntPref("alerts.slideIncrement");
    gSlideTime = prefBranch.getIntPref("alerts.slideIncrementTime");
    gOpenTime = prefBranch.getIntPref("alerts.totalOpenTime");
  } catch (ex) {}

  sizeToContent();
  gFinalHeight = window.outerHeight;
  window.outerHeight = 0;
  // be sure to offset the alert by 10 pixels from the far right edge of the screen
  window.moveTo( (screen.availLeft + screen.availWidth - window.outerWidth) - 10, screen.availTop + screen.availHeight - window.outerHeight);
  setTimeout(animateAlert, gSlideTime);
}

function animateAlert()
{
  if (gCurrentHeight < gFinalHeight)
  {
    gCurrentHeight += gSlideIncrement;

    window.screenY -= gSlideIncrement;
    window.outerHeight += gSlideIncrement;
    setTimeout(animateAlert, gSlideTime);
  }
  else
   setTimeout(closeAlert, gOpenTime);  
}

function closeAlert()
{
  if (gCurrentHeight)
  {
    gCurrentHeight -= gSlideIncrement;

    window.screenY += gSlideIncrement;
    window.outerHeight -= gSlideIncrement;
    setTimeout(closeAlert, gSlideTime);
  }
  else
  {
    if (gAlertListener)
      gAlertListener.observe(null, "alertfinished", gAlertCookie);
    window.close(); 
  }
}

function onAlertClick()
{
  if (gAlertListener && gAlertTextClickable)
    gAlertListener.observe(null, "alertclickcallback", gAlertCookie);
}
