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
 *  Scott MacGregor <mscott@netscape.com>
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

var gCurrentHeight = 0;
var gFinalHeight = 50;
var gWidth = 260;
var gSlideIncrement = 1;
var gSlideTime = 10;
var gOpenTime = 3000; // total time the alert should stay up once we are done animating.

var gAlertListener = null;
var gAlertTextClickable = false;
var gAlertCookie = "";

// fudge factor constants used to help guess a good width for the alert
var imageWidthFudgeFactor = 40; // an alert image should be 26 pixels wide plus a fudge factor
var alertTextFudgeMultiplier = 7;

function onAlertLoad()
{
  // unwrap all the args....
  // arguments[0] --> the image src url
  // arguments[1] --> the alert title
  // arguments[2] --> the alert text
  // arguments[3] --> is the text clickable? 
  // arguments[4] --> the alert cookie to be passed back to the listener
  // arguments[5] --> an optional callback listener (nsIAlertListener)

  document.getElementById('alertImage').setAttribute('src', window.arguments[0]);
  document.getElementById('alertTitleLabel').firstChild.nodeValue = window.arguments[1];
  document.getElementById('alertTextLabel').firstChild.nodeValue = window.arguments[2];
  gAlertTextClickable = window.arguments[3];
  gAlertCookie = window.arguments[4];
 
  // HACK: we need to make sure the alert is wide enough to cleanly hold the image
  // plus the length of the text passed in. Unfortunately we currently have no way to convert
  // .ems into pixels. So I'm "faking" this by multiplying the length of the alert text by a fudge
  // factor. I'm then adding to that the width of the image + a small fudge factor. 
  window.outerWidth = imageWidthFudgeFactor + window.arguments[2].length * alertTextFudgeMultiplier; 

  if (gAlertTextClickable)
    document.getElementById('alertTextLabel').setAttribute('clickable', true);
  
  // the 5th argument is optional
  if (window.arguments[5])
   gAlertListener = window.arguments[5].QueryInterface(Components.interfaces.nsIAlertListener);

  // read out our initial settings from prefs.
  try 
  {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"].getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
    var prefBranch = prefService.getBranch(null);
    gSlideIncrement = prefBranch.getIntPref("alerts.slideIncrement");
    gSlideTime = prefBranch.getIntPref("alerts.slideIncrementTime");
    gOpenTime = prefBranch.getIntPref("alerts.totalOpenTime");
    gFinalHeight = prefBranch.getIntPref("alerts.height");
  } catch (ex) {}

  // offset the alert by 10 pixels from the far right edge of the screen
  gWidth = window.outerWidth + 10;

  window.moveTo(screen.availWidth, screen.availHeight); // move it offscreen initially
  // force a resize on the window to make sure it is wide enough then after that, begin animating the alert.
  setTimeout('window.resizeTo(gWidth - 10, 1);', 1);   
  setTimeout(animateAlert, 10);
}

function animateAlert()
{
  if (gCurrentHeight < gFinalHeight)
  {
    gCurrentHeight += gSlideIncrement;
    window.outerHeight = gCurrentHeight;
    window.moveTo((screen.availWidth - gWidth), screen.availHeight - gCurrentHeight); 
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
    window.outerHeight = gCurrentHeight;
    window.moveTo((screen.availWidth - gWidth), screen.availHeight - gCurrentHeight); 
    setTimeout(closeAlert, gSlideTime);
  }
  else
  {
    if (gAlertListener)
      gAlertListener.onAlertFinished(gAlertCookie); 
    window.close(); 
  }
}

function onAlertClick()
{
  if (gAlertListener && gAlertTextClickable)
    gAlertListener.onAlertClickCallback(gAlertCookie);
}
