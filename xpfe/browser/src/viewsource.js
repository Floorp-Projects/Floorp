  var coreName;


  function StartupViewSource() {
    // Generate unique name.
    coreName = "ViewSource." + ( new Date() ).getTime().toString();
    
    // Create and initialize the browser app core.
    appCore = new BrowserAppCore();
    appCore.Init( coreName );
    appCore.setContentWindow(window.frames[0]);
    appCore.setWebShellWindow(window);
    appCore.setToolbarWindow(window);
    
    // Get url whose source to view.
    var url = document.getElementById("args").getAttribute("value");

    // Load the source (the app core will magically know what to do).
    XPAppCoresManager.Find( coreName ).loadUrl(url);
  }
