const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const RELATIVE_DIR = "modules/libpr0n/test/browser/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;
const TESTROOT2 = "http://example.org/browser/" + RELATIVE_DIR;

var chrome_root = getRootDirectory(gTestPath);
const CHROMEROOT = chrome_root;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function getImageLoading(doc, id) {
  var htmlImg = doc.getElementById(id);
  return htmlImg.QueryInterface(Ci.nsIImageLoadingContent);
}

// Tries to get the Moz debug image, imgIContainerDebug. Only works
// in a debug build. If we succeed, we call func().
function actOnMozImage(doc, id, func) {
  var imgContainer = getImageLoading(doc, id).getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST).image;
  var mozImage;
  try {
    mozImage = imgContainer.QueryInterface(Ci.imgIContainerDebug);
  }
  catch (e) {
    return false;
  }
  func(mozImage);
  return true;
}

function awaitImageContainer(doc, id, func) {
  getImageLoading(doc, id).addObserver({
    QueryInterface: XPCOMUtils.generateQI([Ci.imgIDecoderObserver]),
    onStartContainer: function(aRequest, aContainer) {
      getImageLoading(doc, id).removeObserver(this);
      func(aContainer);
    },
  });
}

