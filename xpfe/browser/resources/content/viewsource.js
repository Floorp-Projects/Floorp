  function onLoadViewSource() {
    // Create and initialize the browser instance.
    createBrowserInstance();

    if ( appCore ) {
        appCore.isViewSource = true;
        appCore.setContentWindow(window.frames[0]);
        appCore.setWebShellWindow(window);
        appCore.setToolbarWindow(window);

        // Get url whose source to view.
        var url = window.arguments[0];
    
        // Load the source (the app core will magically know what to do).
        appCore.loadUrl( url );
    } else {
        // Give up.
        alert( "Error creating browser instance\n" );
    }
    
  }
