  var appCore = null;
  var appCoreName = "";
  function Startup()
  {
    dump("Doing Startup...\n");
    dump("Creating browser app core\n");
    appCore = new BrowserAppCore();
    if (appCore != null) {
      dump("BrowserAppCore has been created.\n");
	  appCoreName = "BrowserAppCore." + ( new Date() ).getTime().toString();
	  appCore.Init( appCoreName );
	  appCore.setWebShellWindow(window);
	  appCore.setToolbarWindow(window);
	  tryToSetContentWindow();
    }
  }

  function onLoadWithArgs() {
    // See if Startup has been run.
    if ( appCore ) {
        // See if load in progress (loading default page).
        if ( document.getElementById("Browser:Throbber").getAttribute("busy") == "true" ) {
            dump( "Stopping load of default initial page\n" );
            appCore.stop();
        }
        dump( "Loading page specified on ShowWindowWithArgs\n" );
        appCore.loadInitialPage();
    } else {
        // onLoad handler timing is not correct yet.
        dump( "onLoadWithArgs not needed yet\n" );
    }
  }

  function tryToSetContentWindow() {
    if ( window.frames[0].frames[1] ) {
        dump("Setting content window\n");
        appCore.setContentWindow( window.frames[0].frames[1] );
        // Have browser app core load appropriate initial page.
        appCore.loadInitialPage();
    } else {
        // Try again.
        dump("Scheduling later attempt to set content window\n");
        window.setTimeout( "tryToSetContentWindow()", 100 );
    }
  }

  function Translate(src, dest)
  {
	var service = "http://levis.alis.com:8080";
	service += "?AlisSourceLang=" + src;
	service += "&AlisTargetLang=" + dest;
	service += "&AlisMTEngine=SSI";
	service += "&AlisTargetURI=" + window.frames[0].frames[1].location.href;
	window.frames[0].frames[1].location.href = service;
  }

  function RefreshUrlbar()
  {
   //Refresh the urlbar bar
    document.getElementById('urlbar').value = window.frames[0].frames[1].location.href;
  }

  function BrowserBack()
  {
     // Get a handle to the back-button
     var bb = document.getElementById("canGoBack");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (bb.getAttribute("disabled")) == "true" ) 
        return;

    if (appCore != null) {
      dump("Going Back\n");
      appCore.back();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserForward()
  {
     // Get a handle to the back-button
     var fb = document.getElementById("canGoForward");
     // If the button is disabled, don't bother calling in to Appcore
     if ( (fb.getAttribute("disabled")) == "true" ) 
        return;

    if (appCore != null) {
      dump("Going Forward\n");
      appCore.forward();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserSetForward()
  {
     var forwardBElem = document.getElementById("canGoForward");
     if (!forwardBElem) {
	dump("Couldn't obtain handle to forward Broarcast element\n");
	return;
	}

     var canForward = forwardBElem.getAttribute("disabled");
     var fb = document.getElementById("forward-button");
     
     if (!fb) {
	dump("Could not obtain handle to forward button\n");
	return;
     }
	
     // Enable/Disable the Forward button      
     if (canForward == "true")  {
        fb.setAttribute("disabled", "true");
     }
     else {
        fb.setAttribute("disabled", "");
     }
        
     // Enable/Disable the Forward menu
     var fm = document.getElementById("menuitem-forward");
     if (!fm) {
       dump("Couldn't obtain menu item Forward\n");
       return;
     }
     else
	dump("Got a handle to forward menu item\n");

     // Enable/Disable the Forward Menuitem
     if (canForward == "true")  {
	dump("Setting forward menu item disabled\n");
        fm.setAttribute("disabled", "true");
     }
     else {
	dump("Setting forward menu item enabled\n");
        fm.setAttribute("disabled", "");
     }
    
  }

  function BrowserCanStop() {
    var stop = document.getElementById("canStop");
    if ( stop ) {
        var stopDisabled = stop.getAttribute("disabled");
        var stopButton   = document.getElementById( "stop-button" );
        if ( stopButton ) {
            if ( stopDisabled ) {
                stopButton.setAttribute( "disabled", "" );
            } else {
                stopButton.removeAttribute( "disabled" );
            }
        }
    }
  }

  function BrowserStop() {
     // Get a handle to the "canStop" broadcast id
     var stopBElem = document.getElementById("canStop");
     if (!stopBElem) {
        dump("Couldn't obtain handle to stop Broadcast element\n");
        return;
	 }

     var canStop = stopBElem.getAttribute("disabled");
     var sb = document.getElementById("stop-button");
     
     if (!sb) {
    	dump("Could not obtain handle to stop button\n");
	    return;
     }

     // If the stop button is currently disabled, just return
     if ((sb.getAttribute("disabled")) == "true") {
	    return;
     }
	
     //Stop button has just been pressed. Disable it. 
     sb.setAttribute("disabled", "");

     // Get a handle to the stop menu item.
     var sm = document.getElementById("menuitem-stop");
     if (!sm) {
       dump("Couldn't obtain menu item Stop\n");
     } else {
       // Disable the stop menu-item.
       sm.setAttribute("disabled", "true");
     }
  
     //Call in to BrowserAppcore to stop the current loading
     if (appCore != null) {
        dump("Going to Stop\n");
        appCore.stop();
     } else {
        dump("BrowserAppCore has not been created!\n");
     }
  }

  function BrowserSetBack()
  {
     var backBElem = document.getElementById("canGoBack");
     if (!backBElem) {
	dump("Couldn't obtain handle to back Broadcast element\n");
	return;
	}

     var canBack = backBElem.getAttribute("disabled");
     var bb = document.getElementById("back-button");
     
     if (!bb) {
	dump("Could not obtain handle to back button\n");
	return;
     }
	
     // Enable/Disable the Back button      
     if (canBack == "true")  {
        bb.setAttribute("disabled", "true");
     }
     else {
        bb.setAttribute("disabled", "");
     }
        
     // Enable/Disable the Back menu
     var bm = document.getElementById("menuitem-back");
     if (!bm) {
       dump("Couldn't obtain menu item Back\n");
       return;
     }
     else
	dump("Obtained MenuItem Back\n");

     // Enable/Disable the Back Menuitem
     if (canBack == "true")  {
	dump("Setting Back menuitem to disabled\n");
        bm.setAttribute("disabled", "true");
     }
     else {
	dump("Setting Back menuitem to enabled\n");
        bm.setAttribute("disabled", "");
     }
    
     
  }

  function BrowserHome()
  {
   window.frames[0].frames[1].home();
   RefreshUrlbar();
  }

  function OpenBookmarkURL(node)
  {
    if (node.getAttribute('container') == "true") {
      return false;
    }
    url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:") {
      return false;
    }

    window.frames[0].frames[1].location.href = url;
    RefreshUrlbar();
  }

  function BrowserNewWindow()
  {
    if (appCore != null) {
	    dump("Opening New Window\n");
      appCore.newWindow();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserNewEditorWindow()
  {
    core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.ShowWindowWithArgs( "chrome://editor/content", window, "chrome://editor/content/EditorInitPage.html" );
    } else {
        dump("Error; can't create toolkitCore\n");
    }
  }
  
  function BrowserNewTextEditorWindow()
  {
    core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.ShowWindowWithArgs( "chrome://editor/content/TextEditorAppShell.xul", window, "chrome://editor/content/EditorInitPagePlain.html" );
    } else {
        dump("Error; can't create toolkitCore\n");
    }
  }
  
  function BrowserEditPage(url)
  {
    core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.ShowWindowWithArgs( "chrome://editor/content", window, url);
    } else {
        dump("Error; can't create toolkitCore\n");
    }
  }
  
  function BrowserOpenWindow()
  {
    core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.ShowWindowWithArgs( "resource:/res/samples/openLocation.xul", window, appCoreName );
    } else {
        dump("Error; can't create toolkitCore\n");
    }
  }

  function OpenFile(url) {
    // This is invoked from the browser app core.
    core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.ShowWindowWithArgs( "resource:/res/samples/navigator.xul", window, url );
    } else {
        dump("Error; can't create toolkitCore\n");
    }
  }

  function BrowserCopy()
  {
    if (appCore != null) {
	    dump("Copying\n");
      appCore.copy();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }


  function BrowserAddBookmark(url,title)
  {
    // get RDF Core service
    var rdfCore = XPAppCoresManager.Find("RDFCore");
    if (!rdfCore) {
      rdfCore = new RDFCore();
      if (!rdfCore) {
        return(false);
      }
      rdfCore.Init("RDFCore");
      XPAppCoresManager.Add(rdfCore);
    }
    // Add it
    rdfCore.addBookmark(url,title);
  }

  function BrowserEditBookmarks()
  {
    var toolkitCore = XPAppCoresManager.Find("toolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("toolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("resource://res/rdf/bookmarks.xul",window);
    }
  }

  function OpenHistoryView()
  {
    var toolkitCore = XPAppCoresManager.Find("toolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("toolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("resource://res/rdf/history.xul",window);
    }
  }

  function BrowserPrintPreview()
  {
    // Borrowing this method to show how to 
    // dynamically change icons
    dump("BrowserPrintPreview\n");
    if (appCore != null) {
	    dump("Changing Icons\n");
      appCore.printPreview();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserPrint()
  {
    // Borrowing this method to show how to 
    // dynamically change icons
    if (appCore != null) {
      appCore.print();
    }
  }

  function BrowserSetDefaultCharacterSet(aCharset)
  {
    if (appCore != null) {
      appCore.SetDocumentCharset(aCharset);
      window.frames[0].frames[1].location.reload();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserClose()
  {
    dump("BrowserClose\n");
  }

  function BrowserExit()
  {
    if (appCore != null) {
	    dump("Exiting\n");
      appCore.exit();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserFind() {
    if (appCore != null) {
        appCore.find();      
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserFindAgain() {
    if (appCore != null) {
        appCore.findNext();      
    } else {
        dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserLoadURL()
  {
	if (appCore == null)
	{
		dump("BrowserAppCore has not been initialized\n");
		return;
	}

	// rjc: added support for URL shortcuts (3/30/1999)

	// get RDF Core service
	var rdfCoreFound = false;
	var rdfCore = XPAppCoresManager.Find("RDFCore");
	if (rdfCore)
	{
		rdfCoreFound = true;
	}
	else
	{
		rdfCore = new RDFCore();
		if (rdfCore)
		{
			rdfCore.Init("RDFCore");
// We do not need this add() call. It fails and doesn't execute the loadURL
// call that is following. The job of add has already been done by the Init()
// call.  
		//	XPAppCoresManager.Add(rdfCore);
			rdfCoreFound = true;
		}
	}
	if (rdfCoreFound == true)
	{
		var shortcutURL = rdfCore.findBookmarkShortcut(document.getElementById('urlbar').value);

		dump("FindBookmarkShortcut: in='" + document.getElementById('urlbar').value + "'  out='" + shortcutURL + "'\n");

		if ((shortcutURL != null) && (shortcutURL != ""))
		{
			document.getElementById('urlbar').value = shortcutURL;
		}
	}

	appCore.loadUrl(document.getElementById('urlbar').value);
      
  }

  function WalletEditor()
  {
    if (appCore != null) {
      dump("Wallet Editor\n");
      appCore.walletEditor(window);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function WalletSafeFillin()
  {
    var walletCore = XPAppCoresManager.Find("WalletCore");
    if (!walletCore) {
      walletCore = new WalletCore();
      if (walletCore) {
        walletCore.Init("WalletCore");
      }
    }
    if (walletCore) {
      walletCore.ShowWindow(window, window.frames[0].frames[1]);
    }
  }

  function WalletChangePassword()
  {
    if (appCore != null) {
      dump("Wallet Change Password\n");
      appCore.walletChangePassword();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }


  function WalletQuickFillin()
  {
    if (appCore != null) {
      dump("Wallet Quick Fillin\n");
      appCore.walletQuickFillin(window.frames[0].frames[1]);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function WalletSamples()
  {
    if (appCore != null) {
      dump("Wallet Samples\n");
      appCore.walletSamples();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function SignonViewer()
  {
    var signonCore = XPAppCoresManager.Find("SignonCore");
    if (!signonCore) {
      signonCore = new SignonCore();
      if (signonCore) {
        signonCore.Init("SignonCore");
      }
    }
    if (signonCore) {
      signonCore.ShowWindow(window);
    }
  }

  function CookieViewer()
  {
    var cookieCore = XPAppCoresManager.Find("CookieCore");
    if (!cookieCore) {
      cookieCore = new CookieCore();
      if (cookieCore) {
      cookieCore.Init("CookieCore");
      }
    }
    if (cookieCore) {
      cookieCore.ShowWindow(window);
    }
  }

  function OpenMessenger()
  {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("chrome://messenger/content/",
                             window);
    }
  }

  function OpenAddressbook()
  {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindow("chrome://addressbook/content/",
                             window);
    }
  }

  function MsgNewMessage()
  {
	// Generate a unique number, do we have a better way?
	var date = new Date();
	var composeAppCoreName = "ComposeAppCore:" + date.getTime() + Math.random();
	var composeAppCore = XPAppCoresManager.Find(composeAppCoreName);
	if (! composeAppCore)
	{
		composeAppCore = new ComposeAppCore();
		if (composeAppCore)
		{
			composeAppCore.Init(composeAppCoreName);
			//argument:
			//	name=<name of the appcore>
			//	editorType=[default | html | text]			; default means use the prefs value send_html
			var args = "name=" + composeAppCoreName + ",editorType=default";
			composeAppCore.NewMessage("chrome://messengercompose/content/", args, null, null, 0);
			dump("Created a compose appcore from Navigator.xul, " + args);
		}
	}
  }

  function DoPreferences()
  {
    var prefsCore = XPAppCoresManager.Find("PrefsCore");
    if (!prefsCore) {
      prefsCore = new PrefsCore();
      if (prefsCore) {
        prefsCore.Init("PrefsCore");
      }
    }
    if (prefsCore) {
      prefsCore.ShowWindow(window);
    }
  }

  function BrowserViewSource()
  {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
    if (toolkitCore) {
      var url = window.frames[0].frames[1].location;
      dump("Opening view of source for" + url + "\n");
      toolkitCore.ShowWindowWithArgs("resource:/res/samples/viewSource.xul", window, url);
    }
  }

  function OpenEditor()
  {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
    if (toolkitCore) {
      toolkitCore.ShowWindowWithArgs("chrome://editor/content/EditorAppShell.xul",window,"chrome://editor/content/EditorInitPage.html");
    }
  }
        var bindCount = 0;
        function onStatus() {
            var status = document.getElementById("Browser:Status");
            if ( status ) {
                var text = status.getAttribute("value");
                var statusText = document.getElementById("statusText");
                if ( statusText ) {
                    statusText.setAttribute( "value", text );
                }
            } else {
                dump("Can't find status broadcaster!\n");
            }
        }

        function onSecurity() {
            var security = document.getElementById("Browser:Security");
            var indicator = document.getElementById("security-box");
            var icon = document.getElementById("security-button");
 
            if ( security.getAttribute("secure") == "true" ) {
                indicator.setAttribute("class","secure");
                icon.setAttribute("class","secure");
            } else {
                indicator.setAttribute("class","insecure");
                icon.setAttribute("class","insecure");
            }
        }

        function securityOn() {
            var security = document.getElementById("Browser:Security");
            if ( security.getAttribute("secure") == "false" ) {
                security.setAttribute("secure","true");
                // Temporary till onchange handlers work.
                onSecurity();
            }
        }
        function securityOff() {
            var security = document.getElementById("Browser:Security");
            if ( security.getAttribute("secure") == "true" ) {
                security.setAttribute("secure","false");
                // Temporary till onchange handlers work.
                onSecurity();
            }
        }
        function doTests() {
            // Turn security on.
            securityOn();
        }
		var startTime = 0;
        function onProgress() {
            var throbber = document.getElementById("Browser:Throbber");
            var meter    = document.getElementById("Browser:LoadingProgress");
            if ( throbber && meter ) {
                var busy = throbber.getAttribute("busy");
                if ( busy == "true" ) {
                    mode = "undetermined";
					if ( !startTime ) {
						startTime = (new Date()).getTime();
					}
                } else {
                    mode = "normal";
                }
                meter.setAttribute("mode",mode);
                if ( mode == "normal" ) {
                    var status = document.getElementById("Browser:Status");
                    if ( status ) {
						var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
						var msg = "Document: Done (" + elapsed + " secs)";
						dump( msg + "\n" );
                        status.setAttribute("value",msg);
                    }
					startTime = 0;
                }
            }
        }
        function dumpProgress() {
            var broadcaster = document.getElementById("Browser:LoadingProgress");
            var meter       = document.getElementById("meter");
            dump( "bindCount=" + bindCount + "\n" );
            dump( "broadcaster mode=" + broadcaster.getAttribute("mode") + "\n" );
            dump( "broadcaster value=" + broadcaster.getAttribute("value") + "\n" );
            dump( "meter mode=" + meter.getAttribute("mode") + "\n" );
            dump( "meter value=" + meter.getAttribute("value") + "\n" );
        }


