
var appCore = null;

function onLoadViewSource() 
{
  viewSource(window.arguments[0]);
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

  var docShellElement = document.getElementById("content");
  docShellElement.docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;

  try {
    if ("arguments" in window && window.arguments.length >= 2) {
      if (window.arguments[1].indexOf('charset=') != -1) {
        var arrayArgComponents = window.arguments[1].split('=');
        if (arrayArgComponents) {
          docShellElement.markupDocumentViewer.defaultCharacterSet = arrayArgComponents[1];
        } 
      }
    }
  } catch(ex) {
  }

  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  docShellElement.webNavigation.loadURI(url, loadFlags);
  return true;
}

function BrowserClose()
{
  window.close();
}

function getMarkupDocumentViewer()
{
  return document.getElementById("content").markupDocumentViewer;
}

function BrowserFind()
{
  if (appCore)
    appCore.find();
}


function BrowserFindAgain()
{
  if (appCore)
    appCore.findNext();      
}

