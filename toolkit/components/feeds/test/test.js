/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Robert Sayre.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****/

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
