"use strict";

add_task(_ => {
  try {
    Cc["@mozilla.org/network/effective-tld-service;1"]
      .createInstance(Ci.nsISupports);
  } catch (e) {
    is(e.result, Cr.NS_ERROR_XPC_CI_RETURNED_FAILURE,
       "Component creation as an instance fails with expected code");
  }
});
