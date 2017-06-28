/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals JSZip */

/* eslint-disable no-nested-ternary */

// base: relative or absolute path (http[s] or file, untested with ftp)
// files: array of file names relative to base to include at the zip
// callbacks: object with optional functions:
//            onsuccess(result), onerror(exception), onprogress(percentage)
function createXpiDataUri(base, files, callbacks) {

  // Synchronous XHR for http[s]/file (untested ftp), throws on any error
  // Note that on firefox, file:// XHR can't access files outside base dir
  function readBinFile(url) {
    var r =  new XMLHttpRequest();
    r.open("GET", url, false);
    r.requestType = "arraybuffer";
    r.overrideMimeType("text/plain; charset=x-user-defined");
    try { r.send(); } catch (e) { throw "FileNotRetrieved: " + url + " - " + e; }
    // For 'file://' Firefox sets status=0 on success or throws otherwise
    // In Firefox 34-ish onwards, success status is 200.
    if (!(r.readyState == 4 && (r.status == 0 || r.status == 200)))
      throw ("FileNotRetrieved: " + url + " - " + r.status + " " + r.statusText);

    return r.response;
  }

  // Create base64 string for a binary array (btoa fails on arbitrary binary data)
  function base64EncArr(aBytes) {
    // From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
    "use strict;"
    function uint6ToB64(nUint6) {
      return nUint6 < 26 ? nUint6 + 65 : nUint6 < 52 ? nUint6 + 71 : nUint6 < 62 ?
          nUint6 - 4 : nUint6 === 62 ? 43 : nUint6 === 63 ? 47 : 65;
    }

    var nMod3 = 2, sB64Enc = "";
    for (var nLen = aBytes.length, nUint24 = 0, nIdx = 0; nIdx < nLen; nIdx++) {
      nMod3 = nIdx % 3;
      if (nIdx > 0 && (nIdx * 4 / 3) % 76 === 0) { sB64Enc += "\r\n"; }
      nUint24 |= aBytes[nIdx] << (16 >>> nMod3 & 24);
      if (nMod3 === 2 || aBytes.length - nIdx === 1) {
        sB64Enc += String.fromCharCode(uint6ToB64(nUint24 >>> 18 & 63), uint6ToB64(nUint24 >>> 12 & 63),
                                                  uint6ToB64(nUint24 >>> 6 & 63), uint6ToB64(nUint24 & 63));
        nUint24 = 0;
      }
    }

    return sB64Enc.substr(0, sB64Enc.length - 2 + nMod3) + (nMod3 === 2 ? "" : nMod3 === 1 ? "=" : "==");
  }

  // Create the zip/xpi
  try {
    function dummy() {}
    var onsuccess  = callbacks.onsuccess || dummy;
    var onerror    = callbacks.onerror || dummy;
    var onprogress = callbacks.onprogress || dummy;

    var zip = new JSZip();
    for (var i = 0; i < files.length; i++) {
      zip.file(files[i], readBinFile(base + files[i]),
               {binary: true, compression: "deflate"});
      onprogress(100 * (i + 1) / (files.length + 1));
    }
    zip = zip.generate({type: "uint8array"});
    onprogress(100);
    setTimeout(onsuccess, 0, "data:application/x-xpinstall;base64," + base64EncArr(zip));

  } catch (e) {
    setTimeout(onerror, 0, e);
  }
}
