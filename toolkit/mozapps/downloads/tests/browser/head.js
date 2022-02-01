/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function forcePromptForFiles(mime, extension) {
  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
    Ci.nsIHandlerService
  );
  const mimeSvc = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

  let txtHandlerInfo = mimeSvc.getFromTypeAndExtension(mime, extension);
  txtHandlerInfo.preferredAction = Ci.nsIHandlerInfo.alwaysAsk;
  txtHandlerInfo.alwaysAskBeforeHandling = true;
  handlerSvc.store(txtHandlerInfo);
  registerCleanupFunction(() => handlerSvc.remove(txtHandlerInfo));
}
