/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 */

const nsIFilePicker     = Components.interfaces.nsIFilePicker;
const nsIWindowMediator = Components.interfaces.nsIWindowMediator;

const FILEPICKER_CONTRACTID     = "@mozilla.org/filepicker;1";
const WINDOWMEDIATOR_CONTRACTID = "@mozilla.org/appshell/window-mediator;1";

function selectFile()
{
  var fp = Components.classes[FILEPICKER_CONTRACTID]
                     .createInstance(nsIFilePicker);

  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var title = prefutilitiesBundle.getString("choosehomepage");
  fp.init(window, title, nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText |
                   nsIFilePicker.filterXML | nsIFilePicker.filterHTML |
                   nsIFilePicker.filterImages);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    var folderField = document.getElementById("browserStartupHomepage");
    folderField.value = fp.fileURL.spec;
  }
}

function setHomePageToCurrentPage()
{
  var windowManager = Components.classes[WINDOWMEDIATOR_CONTRACTID]
                                .getService(nsIWindowMediator);

  var browserWindow = windowManager.getMostRecentWindow("navigator:browser");
  if (browserWindow) {
    var browser = browserWindow.document.getElementById("content");
    var url = browser.webNavigation.currentURI.spec;
    if (url) {
      var homePageField = document.getElementById("browserStartupHomepage");
      homePageField.value = url;
    }
  }
}
