/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function loadHelperScript(path) {
  let scriptFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  scriptFile.initWithPath(getState("__LOCATION__"));
  scriptFile = scriptFile.parent;
  scriptFile.append(path);
  let scriptSpec = Services.io.newFileURI(scriptFile).spec;
  Services.scriptloader.loadSubScript(scriptSpec, this);
}
/* import-globals-from ./server_helper.js */
loadHelperScript("server_helper.js");

let gResponses = new Map(
  Object.entries({
    ABCDEFG123: [
      {
        name: "VIVO Electric 60 x 24 inch Stand Up Desk | Black Table Top, Black Frame, Height Adjustable Standing Workstation with Memory Preset Controller (DESK-KIT-1B6B)",
        url: "https://example.com/Some-Product/dp/ABCDEFG123",
        image_url: "https://example.com/api/image.jpg",
        price: "249.99",
        currency: "USD",
        grade: "A",
        adjusted_rating: 4.6,
        analysis_url:
          "https://www.fakespot.com/product/vivo-electric-60-x-24-inch-stand-up-desk-black-table-top-black-frame-height-adjustable-standing-workstation-with-memory-preset-controller-desk-kit-1b6b",
        sponsored: true,
        aid: "ELcC6OziGKu2jjQsCf5xMYHTpyPKFtjl/4mKPeygbSuQMyIqF/gkY3bTTznoMmNv0OsPV5uql0/NdzNsoguccIS0BujM3DwBADvkGIKLLF2WX0u3G+B2tvRpZmbTmC1NQW0ivS/KX7dTrRjae3Z84fs0i0PySM68buCo5JY848wvzdlyTfCrcT0B3/Ov0tJjZdy9FupF9skLuFtx0/lgElSRsnoGow/H--uLo2Tq7E++RNxgsl--YqlLhv5n8iGFYQwBD61VBg==",
      },
    ],
    HIJKLMN456: [],
    OPQRSTU789: [],
  })
);

function handleRequest(request, response) {
  var body = getPostBody(request.bodyInputStream);
  let requestData = JSON.parse(body);
  let recommendation = gResponses.get(requestData.product_id);

  response.write(JSON.stringify(recommendation));
}
