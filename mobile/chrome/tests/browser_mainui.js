    const Cc = Components.classes;
    const Ci = Components.interfaces;

    function test(){
       var mainWindow = window;
       is(mainWindow.location.href, "chrome://browser/content/browser.xul", "Did not get main window");

       mainWindow.focus();

       var browser = mainWindow.getBrowser();
       isnot(browser, null, "Should have a browser");
       is(browser.currentURI.spec, "about:blank", "Should be displaying the blank page");
    }
