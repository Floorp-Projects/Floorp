  function onLoadViewSource() {
    if ( !useOldAppCore ) {
        StartupViewSource();
    }
  }

  function StartupViewSource() {
    if ( useOldAppCore ) {
        // Generate unique name (var appCoreName declared in navigator.js).
        appCoreName = "ViewSource." + ( new Date() ).getTime().toString();
    
        // Create and initialize the browser app core.
        appCore = new BrowserAppCore();
        appCore.Init( appCoreName );
        appCore.setContentWindow(window.frames[0]);
        appCore.setWebShellWindow(window);
        appCore.setToolbarWindow(window);
    
        // Get url whose source to view.
        var url = document.getElementById("args").getAttribute("value");
    
        // Load the source (the app core will magically know what to do).
        appCore.loadUrl(url);
    } else {
        // Create and initialize the browser instance.
        createBrowserInstance();
    
        if ( appCore ) {
            appCore.isViewSource = true;
            appCore.setContentWindow(window.frames[0]);
            appCore.setWebShellWindow(window);
            appCore.setToolbarWindow(window);
        }
        
        // Get url whose source to view.
        var url = window.arguments[0];
    
        // Load the source (the app core will magically know what to do).
        appCore.loadUrl( url );
    }
  }
