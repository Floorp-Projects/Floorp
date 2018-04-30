/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// First, parse and save the incoming arguments ("?storage=name&context=key")
// Note: window.location.search doesn't work with nsSimpleURIs used for about:* addresses.
var search = window.location.href.match(/^.*\?(.*)$/);
var searchParams = new URLSearchParams(search ? search[1] : "");
var storage = searchParams.get("storage") || "";
var cacheContext = searchParams.get("context");

// The context is in a format as used by the HTTP cache v2 back end
if (cacheContext)
  var [context, isAnon, isInBrowser, appId, isPrivate] = cacheContext.match(/(a,)?(b,)?(i\d+,)?(p,)?/);
if (appId)
  appId = appId.match(/i(\d+),/)[1];


function $(id) { return document.getElementById(id) || {}; }

// Initialize the context UI controls at the start according what we got in the "context=" argument
addEventListener("DOMContentLoaded", function() {
  $("anon").checked = !!isAnon;
  $("inbrowser").checked = !!isInBrowser;
  $("appid").value = appId || "";
  $("priv").checked = !!isPrivate;
}, false);

// When user presses the [Update] button, we build a new context key according the UI control
// values and navigate to a new about:cache?storage=<name>&context=<key> URL.
function navigate() {
  context = "";
  if ($("anon").checked)
    context += "a,";
  if ($("inbrowser").checked)
    context += "b,";
  if ($("appid").value)
    context += "i" + $("appid").value + ",";
  if ($("priv").checked)
    context += "p,";

  window.location.href = "about:cache?storage=" + storage + "&context=" + context;
}

let submitButton = document.getElementById("submit");
submitButton.addEventListener("click", navigate);
