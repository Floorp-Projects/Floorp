
var appCore = null;


function onLoadViewSource() 
{

    try {
        createBrowserInstance();
        if (appCore == null) {
            // Give up.
            dump("Giving up...\n");
            window.close();
        }

        // Initialize browser instance..
        appCore.setWebShellWindow(window);
        if ( window._content ) {
            dump("Setting content window\n");
            appCore.setContentWindow( window._content );
        }
    }

    catch(ex) {
        dump("Failed to create and initialiaze the AppCore...\n");
    }

    var docShellElement = document.getElementById("content-frame");
    var docShell = docShellElement.docShell;
    docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;
    var webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);

    try {
        if ( window.arguments && window.arguments[1] ) {
            if (window.arguments[1].indexOf('charset=') != -1) {
                var arrayArgComponents = window.arguments[1].split('=');
                if (arrayArgComponents) {
                    if (appCore != null) {
                      dump("*** SetDocumentCharset(" + arrayArgComponents[1] + ")\n");
                      appCore.SetDocumentCharset(arrayArgComponents[1]);
                    } 
                }
            }
        }
    }

    catch(ex) {
        dump("*** Failed to SetDocumentCharset...\n");
    }

    webNav.loadURI(window.arguments[0]);

}


function createBrowserInstance()
{
  appCore = Components
    .classes[ "@mozilla.org/appshell/component/browser/instance;1" ]
    .createInstance( Components.interfaces.nsIBrowserInstance );
  if ( !appCore ) {
    dump("Error creating browser instance\n");
  }
}

function BrowserClose()
{
  window.close();
}

function BrowserFind()
{
  if (appCore) {
    appCore.find();      
  } else {
    dump("BrowserAppCore has not been created!\n");
  }
}


function BrowserFindAgain()
{
  if (appCore) {
    appCore.findNext();      
  } else {
    dump("BrowserAppCore has not been created!\n");
  }
}

  function BrowserSelectAll() {
    if (appCore != null) {
        appCore.selectAll();
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

