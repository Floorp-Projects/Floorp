/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var passed = 0;
var ran = 0;
if(tests.length < 1){
  dump("FAILED! tests.length was " + tests.length  + "\n");
}

function logResult(didPass, testcase, extra) {
  var start = didPass ? "PASS | " : "FAIL | ";
  print(start + testcase.path + " | Test was: \"" +
        testcase.desc + "\" | " + testcase.expect + " |");
  if (extra) {
    print(extra);
  }
}


function isIID(a,iid){
  var rv = false;
  try{
    a.QueryInterface(iid);
    rv = true;
  }catch(e){}
  return rv;
}
function TestListener(){}

TestListener.prototype = {
  handleResult: function(result){
    var feed = result.doc;    
    // QI to something
    (isIID(feed, Components.interfaces.nsIFeed));
    try {
      if(!eval(testcase.expect)){
        logResult(false, testcase);
  
      }else{
        logResult(true, testcase);
        passed += 1;
      }
    }
    catch(e) {
      logResult(false, testcase, "ex: " + e.message);
    }

    ran += 1;
    result = null;
  }
}

var startDate = new Date();

for(var i=0; i<tests.length; i++){
  var testcase = tests[i];
  var uri;
  if (testcase.base == null)
    uri = ioService.newURI('http://example.org/'+testcase.path, null,null);
  else
    uri = testcase.base

  var parser = Components.classes["@mozilla.org/feed-processor;1"]
                         .createInstance(Components.interfaces.nsIFeedProcessor);
  var stream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Components.interfaces.nsIFileInputStream);
  var listener = new TestListener();
  try{
    stream.init(testcase.file, 0x01, 0444, 0);
    parser.listener = listener;
    parser.parseFromStream(stream, uri);
  }catch(e){
    dump("FAILED! Error parsing file " + testcase.file.leafName  + 
         "Error: " + e.message + "\n");
  }finally{
    stream.close();
  }

}
var endDate = new Date();

print("Start: " + startDate);
print("End  : " + endDate);
print("tests:  " + tests.length);
print("ran:    " + ran);
print("passed: " + passed);
