
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
    createBrowserInstance();
    if (appCore == null) {
    // Give up.
    window.close();
    return false;
    }

    // Initialize browser instance..
    appCore.setWebShellWindow(window);
  }

  catch(ex) {
  }

  var docShellElement = document.getElementById("content-frame");
  var docShell = docShellElement.docShell;
  docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;
  var webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);

  try {
      if (window.arguments && window.arguments[1]) {
          if (window.arguments[1].indexOf('charset=') != -1) {
              var arrayArgComponents = window.arguments[1].split('=');
              if (arrayArgComponents) {
                appCore.SetDocumentCharset(arrayArgComponents[1]);
              } 
          }
      }
  }

  catch(ex) {
  }

  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  webNav.loadURI(url, loadFlags);
  return true;
}

function createBrowserInstance()
{
  appCore = Components
    .classes[ "@mozilla.org/appshell/component/browser/instance;1" ]
    .createInstance( Components.interfaces.nsIBrowserInstance );
}

function BrowserClose()
{
  window.close();
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

