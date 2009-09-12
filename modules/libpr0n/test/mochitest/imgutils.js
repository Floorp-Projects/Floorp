// Helper file for shared image functionality
// 
// Note that this is use by tests elsewhere in the source tree. When in doubt,
// check mxr before removing or changing functionality.

// Helper function to determine if the frame is decoded for a given image id
function isFrameDecoded(id)
{
  return (getImageStatus(id) &
          Components.interfaces.imgIRequest.STATUS_FRAME_COMPLETE)
         ? true : false;
}

// Helper function to determine if the image is loaded for a given image id
function isImageLoaded(id)
{
  return (getImageStatus(id) &
          Components.interfaces.imgIRequest.STATUS_LOAD_COMPLETE)
         ? true : false;
}

// Helper function to get the status flags of an image
function getImageStatus(id)
{
  // Escalate
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  // Get the image
  var img = document.getElementById(id);

  // QI the image to nsImageLoadingContent
  img.QueryInterface(Components.interfaces.nsIImageLoadingContent);

  // Get the request
  var request = img.getRequest(Components.interfaces
                                         .nsIImageLoadingContent
                                         .CURRENT_REQUEST);

  // Return the status
  return request.imageStatus;
}

// Forces a synchronous decode of an image by drawing it to a canvas. Only
// really meaningful if the image is fully loaded first
function forceDecode(id)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  // Get the image
  var img = document.getElementById(id);

  // Make a new canvas
  var canvas = document.createElement("canvas");

  // Draw the image to the canvas. This forces a synchronous decode
  var ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0);
}


// Functions to facilitate getting/setting the discard timer pref
//
// Don't forget to reset the pref to the original value!
//
// Null indicates no pref set

const DISCARD_BRANCH_NAME = "image.cache.";
const DISCARD_PREF_NAME = "discard_timer_ms";

function setDiscardTimerPref(timeMS)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var branch = prefService.getBranch(DISCARD_BRANCH_NAME);
  if (timeMS != null)
    branch.setIntPref(DISCARD_PREF_NAME, timeMS);
  else if (branch.prefHasUserValue(DISCARD_PREF_NAME))
    branch.clearUserPref(DISCARD_PREF_NAME);
}

function getDiscardTimerPref()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var branch = prefService.getBranch(DISCARD_BRANCH_NAME);
  if (branch.prefHasUserValue(DISCARD_PREF_NAME))
    return branch.getIntPref("discard_timeout_ms");
  else
    return null;
}
