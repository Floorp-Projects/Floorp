/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var gBrowser = null;
var appCore = null;

function onLoadViewSource() 
{
  viewSource(window.arguments[0]);
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function viewSource(url)
{
  if (!url)
    return false; // throw Components.results.NS_ERROR_FAILURE;

  try {
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);

    // Initialize browser instance..
    appCore.setWebShellWindow(window);
  } catch(ex) {
    // Give up.
    window.close();
    return false;
  }

  try {
    if ("arguments" in window && window.arguments.length >= 2) {
      if (window.arguments[1].indexOf('charset=') != -1) {
        var arrayArgComponents = window.arguments[1].split('=');
        if (arrayArgComponents) {
          appCore.setDefaultCharacterSet(arrayArgComponents[1]); //XXXjag see bug 67442
        } 
      }
    }
  } catch(ex) {
  }

  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  var viewSrcUrl = "view-source:" + url;
  getBrowser().webNavigation.loadURI(viewSrcUrl, loadFlags);
  window._content.focus();
  return true;
}

function BrowserClose()
{
  window.close();
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}

function BrowserFind()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;

  findInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserFindAgain()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;

  findAgainInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserCanFindAgain()
{
  return canFindAgainInPage();
}
