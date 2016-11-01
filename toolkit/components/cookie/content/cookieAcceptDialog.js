// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsICookieAcceptDialog = Components.interfaces.nsICookieAcceptDialog;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsICookie = Components.interfaces.nsICookie;
const nsICookiePromptService = Components.interfaces.nsICookiePromptService;

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var params;
var cookieBundle;

var showDetails = "";
var hideDetails = "";
var detailsAccessKey = "";

function onload()
{
  doSetOKCancel(cookieAcceptNormal, cookieDeny, cookieAcceptSession);

  var dialog = document.documentElement;

  document.getElementById("Button2").collapsed = false;

  document.getElementById("ok").label = dialog.getAttribute("acceptLabel");
  document.getElementById("ok").accessKey = dialog.getAttribute("acceptKey");
  document.getElementById("Button2").label = dialog.getAttribute("extra1Label");
  document.getElementById("Button2").accessKey = dialog.getAttribute("extra1Key");
  document.getElementById("cancel").label = dialog.getAttribute("cancelLabel");
  document.getElementById("cancel").accessKey = dialog.getAttribute("cancelKey");

  // hook up button icons where implemented
  document.getElementById("ok").setAttribute("icon", "accept");
  document.getElementById("cancel").setAttribute("icon", "cancel");
  document.getElementById("disclosureButton").setAttribute("icon", "properties");

  cookieBundle = document.getElementById("cookieBundle");

  // cache strings
  if (!showDetails) {
    showDetails = cookieBundle.getString('showDetails');
  }
  if (!hideDetails) {
    hideDetails = cookieBundle.getString('hideDetails');
  }
  detailsAccessKey = cookieBundle.getString('detailsAccessKey');

  if (document.getElementById('infobox').hidden) {
    document.getElementById('disclosureButton').setAttribute("label", showDetails);
  } else {
    document.getElementById('disclosureButton').setAttribute("label", hideDetails);
  }
  document.getElementById('disclosureButton').setAttribute("accesskey", detailsAccessKey);

  if ("arguments" in window && window.arguments.length >= 1 && window.arguments[0]) {
    try {
      params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
      var objects = params.objects;
      var cookie = params.objects.queryElementAt(0, nsICookie);
      var cookiesFromHost = params.GetInt(nsICookieAcceptDialog.COOKIESFROMHOST);

      var messageFormat;
      if (params.GetInt(nsICookieAcceptDialog.CHANGINGCOOKIE))
        messageFormat = 'permissionToModifyCookie';
      else if (cookiesFromHost > 1)
        messageFormat = 'permissionToSetAnotherCookie';
      else if (cookiesFromHost == 1)
        messageFormat = 'permissionToSetSecondCookie';
      else
        messageFormat = 'permissionToSetACookie';

      var hostname = params.GetString(nsICookieAcceptDialog.HOSTNAME);

      var messageText;
      if (cookie)
        messageText = cookieBundle.getFormattedString(messageFormat, [hostname, cookiesFromHost]);
      else
        // No cookies means something went wrong. Bring up the dialog anyway
        // to not make the mess worse.
        messageText = cookieBundle.getFormattedString(messageFormat, ["", cookiesFromHost]);

      var messageParent = document.getElementById("dialogtextbox");
      var messageParagraphs = messageText.split("\n");

      // use value for the header, so it doesn't wrap.
      var headerNode = document.getElementById("dialog-header");
      headerNode.setAttribute("value", messageParagraphs[0]);

      // use childnodes here, the text can wrap
      for (var i = 1; i < messageParagraphs.length; i++) {
        var descriptionNode = document.createElement("description");
        text = document.createTextNode(messageParagraphs[i]);
        descriptionNode.appendChild(text);
        messageParent.appendChild(descriptionNode);
      }

      if (cookie) {
        document.getElementById('ifl_name').setAttribute("value", cookie.name);
        document.getElementById('ifl_value').setAttribute("value", cookie.value);
        document.getElementById('ifl_host').setAttribute("value", cookie.host);
        document.getElementById('ifl_path').setAttribute("value", cookie.path);
        document.getElementById('ifl_isSecure').setAttribute("value",
                                                                 cookie.isSecure ?
                                                                    cookieBundle.getString("forSecureOnly") : cookieBundle.getString("forAnyConnection")
                                                          );
        document.getElementById('ifl_expires').setAttribute("value", GetExpiresString(cookie.expires));
        document.getElementById('ifl_isDomain').setAttribute("value",
                                                                 cookie.isDomain ?
                                                                    cookieBundle.getString("domainColon") : cookieBundle.getString("hostColon")
                                                            );
      }
      // set default result to not accept the cookie
      params.SetInt(nsICookieAcceptDialog.ACCEPT_COOKIE, 0);
      // and to not persist
      params.SetInt(nsICookieAcceptDialog.REMEMBER_DECISION, 0);
    } catch (e) {
    }
  }

  // The Private Browsing service might not be available
  try {
    if (window.opener && PrivateBrowsingUtils.isWindowPrivate(window.opener)) {
      var persistCheckbox = document.getElementById("persistDomainAcceptance");
      persistCheckbox.removeAttribute("checked");
      persistCheckbox.setAttribute("disabled", "true");
    }
  } catch (ex) {}
}

function showhideinfo()
{
  var infobox=document.getElementById('infobox');

  if (infobox.hidden) {
    infobox.setAttribute("hidden", "false");
    document.getElementById('disclosureButton').setAttribute("label", hideDetails);
  } else {
    infobox.setAttribute("hidden", "true");
    document.getElementById('disclosureButton').setAttribute("label", showDetails);
  }
  sizeToContent();
}

function cookieAcceptNormal()
{
  // accept the cookie normally
  params.SetInt(nsICookieAcceptDialog.ACCEPT_COOKIE, nsICookiePromptService.ACCEPT_COOKIE);
  // And remember that when needed
  params.SetInt(nsICookieAcceptDialog.REMEMBER_DECISION, document.getElementById('persistDomainAcceptance').checked);
  window.close();
}

function cookieAcceptSession()
{
  // accept for the session only
  params.SetInt(nsICookieAcceptDialog.ACCEPT_COOKIE, nsICookiePromptService.ACCEPT_SESSION_COOKIE);
  // And remember that when needed
  params.SetInt(nsICookieAcceptDialog.REMEMBER_DECISION, document.getElementById('persistDomainAcceptance').checked);
  window.close();
}

function cookieDeny()
{
  // say that the cookie was rejected
  params.SetInt(nsICookieAcceptDialog.ACCEPT_COOKIE, nsICookiePromptService.DENY_COOKIE);
  // And remember that when needed
  params.SetInt(nsICookieAcceptDialog.REMEMBER_DECISION, document.getElementById('persistDomainAcceptance').checked);
  window.close();
}

function GetExpiresString(secondsUntilExpires) {
  if (secondsUntilExpires) {
    var date = new Date(1000*secondsUntilExpires);
    const locale = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                   .getService(Components.interfaces.nsIXULChromeRegistry)
                   .getSelectedLocale("global", true);
    const dtOptions = { year: 'numeric', month: 'long', day: 'numeric',
                        hour: 'numeric', minute: 'numeric', second: 'numeric' };
    return date.toLocaleString(locale, dtOptions);
  }
  return cookieBundle.getString("expireAtEndOfSession");
}
