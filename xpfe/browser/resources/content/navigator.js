/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

  var appCore = null;
  var prefwindow = null;
  var appCoreName = "";
  var defaultStatus = "default status text";
  var debugSecurity = false; // Set this true to enable Security chrome testing.
  var explicitURL = false;

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

  function Shutdown() {
    // Close the app core.
    if ( appCore ) {
        appCore.close();
        // Remove app core from app core manager.
        XPAppCoresManager.Remove( appCore );
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
        // Remember that we want this url.
        explicitURL = true;
    }
  }

  function tryToSetContentWindow() {
    if ( window.content ) {
        dump("Setting content window\n");
        appCore.setContentWindow( window.content );
        // Have browser app core load appropriate initial page.

        if ( !explicitURL ) {
            var pref = Components.classes['component://netscape/preferences'];
	    dump("Components = " + Components + "\n");
	    dump("Components.classes = " + Components.classes + "\n");
	    dump("Components.classes[component://netscape/preferences] = " + pref + "\n");
    
            // if all else fails, use trusty "about:blank" as the start page
            var startpage = "about:blank";  
            if (pref) {
              pref = pref.getService();
            }
	    else {
		dump("failed to get component://netscape/preferences\n");
	    }
            if (pref) {
              pref = pref.QueryInterface(Components.interfaces.nsIPref);
            }
	    else {
		dump("failed to get pref service\n");
	    }
            if (pref) {
              // from mozilla/modules/libpref/src/init/all.js
              // 0 = blank 
              // 1 = home (browser.startup.homepage)
              // 2 = last 
		choice = 1;
		try {
              		choice = pref.GetIntPref("browser.startup.page");
		}
		catch (ex) {
			dump("failed to get the browser.startup.page pref\n");
		}
		dump("browser.startup.page = " + choice + "\n");
    	  switch (choice) {
    		case 0:
                		startpage = "about:blank";
          			break;
    		case 1:
				try {
                			startpage = pref.CopyCharPref("browser.startup.homepage");
				}
				catch (ex) {
					dump("failed to get the browser.startup.homepage pref\n");
					startpage = "about:blank";
				}
          			break;
    		case 2:
                		var history = Components.classes['component://netscape/browser/global-history'];
    			if (history) {
                   			history = history.getService();
    	    		}
    	    		if (history) {
                  			history = history.QueryInterface(Components.interfaces.nsIGlobalHistory);
    	    		}
    	    		if (history) {
    				startpage = history.GetLastPageVisted();
    	    		}
          			break;
       		default:
                		startpage = "about:blank";
    	  }
            }
            else {
		dump("failed to QI pref service\n");
	    }
	    dump("startpage = " + startpage + "\n");
            document.getElementById("args").setAttribute("value", startpage);
        }
        appCore.loadInitialPage();
    } else {
        // Try again.
        dump("Scheduling later attempt to set content window\n");
        window.setTimeout( "tryToSetContentWindow()", 100 );
    }
  }

  function Translate(src, dest, engine)
  {
	var service = "http://levis.alis.com:8081";
	service += "?AlisSourceLang=" + src;
	service += "&AlisTargetLang=" + dest;
	service += "&AlisMTEngine=" + engine;
	service += "&AlisTargetURI=" + window.content.location.href;
	window.content.location.href = service;
  }

  function RefreshUrlbar()
  {
   //Refresh the urlbar bar
    document.getElementById('urlbar').value = window.content.location.href;
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

     // Enable/Disable the Forward Menuitem
     if (canForward == "true")  {
        fm.setAttribute("disabled", "true");
     }
     else {
        fm.setAttribute("disabled", "");
     }    
  }

  function BrowserCanStop() {
    var stop = document.getElementById("canStop");
    if ( stop ) {
        var stopDisabled = stop.getAttribute("disabled");
        var stopButton   = document.getElementById( "stop-button" );
        if ( stopButton ) {
            if ( stopDisabled == "true") {
                stopButton.setAttribute( "disabled", "true" );
            } else {
                stopButton.setAttribute( "disabled", "" );
            }
        }
        //Enable/disable the stop menu item
        var stopMenu   = document.getElementById( "menuitem-stop" );
        if ( stopMenu ) {
            if ( stopDisabled == "true") {
                stopMenu.setAttribute( "disabled", "true" );
            } else {
                stopMenu.setAttribute( "disabled", "" );
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
     sb.setAttribute("disabled", "true");

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

     // Enable/Disable the Back Menuitem
     if (canBack == "true")  {
        bm.setAttribute("disabled", "true");
     }
     else {
        bm.setAttribute("disabled", "");
     }     
  }


  function BrowserSetReload() {
    var reload = document.getElementById("canReload");
    if ( reload ) {
        var reloadDisabled = reload.getAttribute("disabled");
        //Enable/disable the reload button
        var reloadButton   = document.getElementById( "reload-button" );
        
        if ( reloadButton ) {
            if ( reloadDisabled == "true") {
                reloadButton.setAttribute( "disabled", "true" );
            } else {
                reloadButton.setAttribute( "disabled", "" );
            }
        }
        //Enable/disable the reload menu
        var reloadMenu = document.getElementById("menuitem-reload");
        if ( reloadMenu ) {
            if ( reloadDisabled == "true") {
                
                reloadMenu.setAttribute( "disabled", "true" );
            } else {
                
                reloadMenu.setAttribute( "disabled", "" );
            }
        }
    }
  }

  function BrowserReallyReload(reloadType) {
     // Get a handle to the "canReload" broadcast id
     var reloadBElem = document.getElementById("canReload");
     if (!reloadBElem) {
        dump("Couldn't obtain handle to reload Broadcast element\n");
        return;
	   }

     var canreload = reloadBElem.getAttribute("disabled");
     var sb = document.getElementById("reload-button");
     
     if (!sb) {
    	 dump("Could not obtain handle to reload button\n");
	     return;
     }

     // If the reload button is currently disabled, just return
     if ((sb.getAttribute("disabled")) == "true") {
	     return;
     }
	
     //reload button has just been pressed. Disable it. 
     sb.setAttribute("disabled", "true");

     // Get a handle to the reload menu item.
     var sm = document.getElementById("menuitem-reload");
     if (!sm) {
       dump("Couldn't obtain menu item reload\n");
     } else {
       // Disable the reload menu-item.
       
       sm.setAttribute("disabled", "true");
     }
  
     //Call in to BrowserAppcore to reload the current loading
     if (appCore != null) {
        dump("Going to reload\n");
        appCore.reload(reloadType);
     } else {
        dump("BrowserAppCore has not been created!\n");
     }
  }

  function BrowserHome()
  {
   window.content.home();
   RefreshUrlbar();
  }

  function OpenBookmarkURL(node)
  {
    if (node.getAttribute('container') == "true") {
      return false;
    }

	var url = node.getAttribute('id');
	try
	{
		// first try asking RDF's graph for the URL
		var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf)
		{
			var Bookmarks = rdf.GetDataSource("rdf:bookmarks");
			if (Bookmarks)
			{
				var src = rdf.GetResource(url, true);
				var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
				var target = Bookmarks.GetTarget(src, prop, true);
				if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
				if (target)	target = target.Value;
				if (target)	url = target;
				
			}
		}
	}
	catch(ex)
	{
	}

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:") {
      return false;
    }

    window.content.location.href = url;
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
        core.ShowWindowWithArgs( "chrome://editor/content", window, "resource:/res/html/empty_doc.html" );
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
        //core.ShowWindowWithArgs( "resource:/res/samples/openLocation.xul", window, appCoreName );
        var name = appCoreName.replace( /\./, /\_/ );
        // Note: Use width/height one less than actual so resizing occurs.
        //       This bypasses bug whereby dialog contents don't appear
        //       till the dialog is resized.
        window.openDialog( "chrome://navigator/content/openLocation.xul", name+"_openLocation", "chrome,width=419,height=189", appCoreName );
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
        core.ShowWindowWithArgs( "chrome://navigator/content/navigator.xul", window, url );
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
    var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
    bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
    bmks.AddBookmark(url, title);
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
      toolkitCore.ShowWindow("resource:/res/samples/bookmarks.xul",window);
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
      window.content.location.reload();
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function BrowserClose()
  {
    dump("BrowserClose\n");
  // Currently window.close doesn't work unless the window was opened from JS
  //	 window.close();
  
   core = XPAppCoresManager.Find("toolkitCore");
    if ( !core ) {
        core = new ToolkitCore();
        if ( core ) {
            core.Init("toolkitCore");
        }
    }
    if ( core ) {
        core.CloseWindow( window );
    } else {
        dump("Error can't create toolkitCore\n");
    }
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
    try {
      var bmks = Components.classes["component://netscape/browser/bookmarks-service"].getService();
      bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);

      var shortcutURL = bmks.FindShortcut(document.getElementById('urlbar').value);

      dump("FindShortcut: in='" + document.getElementById('urlbar').value + "'  out='" + shortcutURL + "'\n");

      if ((shortcutURL != null) && (shortcutURL != "")) {
        document.getElementById('urlbar').value = shortcutURL;
      }

	// if the URL is bookmarked, update its "Last Visited" date
      bmks.UpdateBookmarkLastVisitedDate(document.getElementById('urlbar').value);

	}
    catch (ex) {
      // stifle any exceptions so we're sure to load the URL.
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
    if (appCore != null) {
      dump("Wallet Safe Fillin\n");
      appCore.walletPreview(window, window.content);
    } else {
      dump("BrowserAppCore has not been created!\n");
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
      appCore.walletQuickFillin(window.content);
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
    if (appCore != null) {
      dump("Signon Viewer\n");
      appCore.signonViewer(window);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function CookieViewer()
  {
    if (appCore != null) {
      dump("Cookie Viewer\n");
      appCore.cookieViewer(window);
    } else {
      dump("BrowserAppCore has not been created!\n");
    }
  }

  function OpenMessenger()
  {
	window.open("chrome://messenger/content/", "_new", "chrome");
  }

  function OpenAddressbook()
  {
	window.open("chrome://addressbook/content/", "_new", "chrome");
  }

  function MsgNewMessage()
  {
    var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
    if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
    if (toolkitCore) {
      // We need to use ShowWindowWithArgs because message compose depend on callback
      toolkitCore.ShowWindowWithArgs("chrome://messengercompose/content/", window, "");
    }
  }
  
  function DoPreferences()
  {
    if (!prefwindow)
    {
    	prefwindow = Components.classes['component://netscape/prefwindow'].createInstance(Components.interfaces.nsIPrefWindow);
	}
    prefwindow.showWindow("navigator.js", window, "chrome://pref/content/pref-appearance.xul");
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
      var url = window.content.location;
      dump("Opening view of source for" + url + "\n");
      toolkitCore.ShowWindowWithArgs("chrome://navigator/content/viewSource.xul", window, url);
    }
  }


        var bindCount = 0;
        function onStatus() {
            var status = document.getElementById("Browser:Status");
            if ( status ) {
                var text = status.getAttribute("value");
                if ( text == "" ) {
                    text = defaultStatus;
                }
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
            // Set debugSecurity (at top of file) to turn this code on.
            if ( !debugSecurity ) {
                return;
            }

            var security = document.getElementById("Browser:Security");
            if ( security.getAttribute("secure") == "false" ) {
                security.setAttribute("secure","true");
            }
        }
        function securityOff() {
            var security = document.getElementById("Browser:Security");
            if ( security.getAttribute("secure") == "true" ) {
                security.setAttribute("secure","false");
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
                var wasBusy = meter.getAttribute("mode") == "undetermined" ? "true" : "false";
                if ( busy == "true" ) {
                    if ( wasBusy == "false" ) {
                        // Remember when loading commenced.
    				    startTime = (new Date()).getTime();
                        // Turn progress meter on.
                        meter.setAttribute("mode","undetermined");
                    }
                    // Update status bar.
                } else if ( busy == "false" && wasBusy == "true" ) {
                    // Record page loading time.
                    var status = document.getElementById("Browser:Status");
                    if ( status ) {
						var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
						var msg = "Document: Done (" + elapsed + " secs)";
						dump( msg + "\n" );
                        status.setAttribute("value",msg);
                        defaultStatus = msg;
                    }
                    // Turn progress meter off.
                    meter.setAttribute("mode","normal");
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



