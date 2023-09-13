// calculate the fullhash and send it to gethash server
function addCompletionToServer(list, url, mochitestUrl) {
  return new Promise(function (resolve, reject) {
    var listParam = "list=" + list;
    var fullhashParam = "fullhash=" + hash(url);

    var xhr = new XMLHttpRequest();
    xhr.open("PUT", mochitestUrl + "?" + listParam + "&" + fullhashParam, true);
    xhr.setRequestHeader("Content-Type", "text/plain");
    xhr.onreadystatechange = function () {
      if (this.readyState == this.DONE) {
        resolve();
      }
    };
    xhr.send();
  });
}

function hash(str) {
  var hasher = SpecialPowers.Cc["@mozilla.org/security/hash;1"].createInstance(
    SpecialPowers.Ci.nsICryptoHash
  );

  var data = new TextEncoder().encode(str);
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return hasher.finish(true);
}

var pushPrefs = (...p) => SpecialPowers.pushPrefEnv({ set: p });

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      setTimeout(aCallback, 0);
    }
  }, "browser-delayed-startup-finished");
}
