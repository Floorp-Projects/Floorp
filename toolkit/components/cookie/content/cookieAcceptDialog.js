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
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsICookieAcceptDialog = Components.interfaces.nsICookieAcceptDialog;

var params; 
var cookieBundle;
var gDateService = null;

var showDetails = "";
var hideDetails = "";

function onload()
{
  var dialog = document.documentElement;
  dialog.getButton("accept").label = dialog.getAttribute("acceptLabel");
  dialog.getButton("cancel").label = dialog.getAttribute("cancelLabel");

  if (!gDateService) {
    const nsScriptableDateFormat_CONTRACTID = "@mozilla.org/intl/scriptabledateformat;1";
    const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
    gDateService = Components.classes[nsScriptableDateFormat_CONTRACTID]
                             .getService(nsIScriptableDateFormat);
  }

  cookieBundle = document.getElementById("cookieBundle");

  //cache strings
  if (!showDetails) {
    showDetails = cookieBundle.getString('showDetails');
  }
  if (!hideDetails) {
    hideDetails = cookieBundle.getString('hideDetails');
  }

  if (document.getElementById('infobox').hidden) {
    document.getElementById('disclosureButton').setAttribute("label",showDetails);
  } else {
    document.getElementById('disclosureButton').setAttribute("label",hideDetails);
  }

  if ("arguments" in window && window.arguments.length >= 1 && window.arguments[0]) {
    try {
      params = window.arguments[0].QueryInterface(nsIDialogParamBlock);

      var messageText = params.GetString(nsICookieAcceptDialog.MESSAGETEXT);
      var messageParent = document.getElementById("info.box");
      var messageParagraphs = messageText.split("\n");

      for (var i = 0; i < messageParagraphs.length; i++) {
        var descriptionNode = document.createElement("description");
        var text = document.createTextNode(messageParagraphs[i]);
        descriptionNode.appendChild(text);
        messageParent.appendChild(descriptionNode);
      }

      document.getElementById('persistDomainAcceptance').checked = params.GetInt(nsICookieAcceptDialog.REMEMBER_DECISION) > 0;

      document.getElementById('ifl_name').setAttribute("value",params.GetString(nsICookieAcceptDialog.COOKIE_NAME));
      document.getElementById('ifl_value').setAttribute("value",params.GetString(nsICookieAcceptDialog.COOKIE_VALUE));
      document.getElementById('ifl_host').setAttribute("value",params.GetString(nsICookieAcceptDialog.COOKIE_HOST));
      document.getElementById('ifl_path').setAttribute("value",params.GetString(nsICookieAcceptDialog.COOKIE_PATH));
      document.getElementById('ifl_isSecure').setAttribute("value",
                                                           params.GetInt(nsICookieAcceptDialog.COOKIE_IS_SECURE) ?
                                                                  cookieBundle.getString("yes") : cookieBundle.getString("no")
                                                          );
      document.getElementById('ifl_expires').setAttribute("value",GetExpiresString(params.GetInt(nsICookieAcceptDialog.COOKIE_EXPIRES)));
      document.getElementById('ifl_isDomain').setAttribute("value",
                                                           params.GetInt(nsICookieAcceptDialog.COOKIE_IS_DOMAIN) ?
                                                                  cookieBundle.getString("domainColon") : cookieBundle.getString("hostColon")
                                                          );

      // set default result to cancelled
      params.SetInt(eAcceptCookie, 0); 

    } catch (e) {
    }
  }
}

function showhideinfo()
{
  var infobox=document.getElementById('infobox');

  if (infobox.hidden) {
    infobox.setAttribute("hidden","false");
    document.getElementById('disclosureButton').setAttribute("label",hideDetails);
  } else {
    infobox.setAttribute("hidden","true");
    document.getElementById('disclosureButton').setAttribute("label",showDetails);
  }
  sizeToContent();
}

function onChangePersitence()
{
  params.SetInt(nsICookieAcceptDialog.REMEMBER_DECISION, document.getElementById('persistDomainAcceptance').checked);
}

function cookieAccept()
{
  params.SetInt(nsICookieAcceptDialog.ACCEPT_COOKIE, 1); // say that ok was pressed
}

function GetExpiresString(secondsUntilExpires) {
  if (secondsUntilExpires) {
    var expireDate = new Date(1000*secondsUntilExpires);
    return gDateService.FormatDateTime("", gDateService.dateFormatLong,
                                       gDateService.timeFormatSeconds, 
                                       expireDate.getFullYear(),
                                       expireDate.getMonth()+1, 
                                       expireDate.getDate(), 
                                       expireDate.getHours(),
                                       expireDate.getMinutes(), 
                                       expireDate.getSeconds());
  }
  return cookieBundle.getString("atEndOfSession");
}

