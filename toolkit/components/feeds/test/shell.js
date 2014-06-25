/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// shell.js
// This file sets up the unit test environment by building an array of files
// to be tested. It assumes it lives in a folder adjacent to the a folder
// called 'xml', where the testcases live.
//
// The directory layout looks something like this:
//
// tests/ *shell.js*
//       |                  
//       - test.js
//       |
//       - xml/ -- rss1/... 
//              |
//              -- rss2/...
//              |
//              -- atom/testcase.xml

function trimString(s)
{
  return(s.replace(/^\s+/,'').replace(/\s+$/,''));
}

var tests = new Array();
const ioService = Components.classes['@mozilla.org/network/io-service;1'].getService(Components.interfaces.nsIIOService);

// find the directory containing our test XML

if (0 < arguments.length) {

  // dir is specified on the command line
  
  var topDir = Components.classes["@mozilla.org/file/local;1"]
    .createInstance(Components.interfaces.nsILocalFile);
  
  topDir.initWithPath(arguments[0]);
}
else {
  const nsIDirectoryServiceProvider
    = Components.interfaces.nsIDirectoryServiceProvider;
  const nsIDirectoryServiceProvider_CONTRACTID
    = "@mozilla.org/file/directory_service;1";

  var dirServiceProvider 
    = Components.classes[nsIDirectoryServiceProvider_CONTRACTID]
    .getService(nsIDirectoryServiceProvider);

  var persistent = new Object();

  var topDir = dirServiceProvider.getFile("CurWorkD", persistent);
}

var entries = topDir.directoryEntries;
var xmlDir;
while(entries.hasMoreElements()){
  xmlDir = entries.getNext();
  xmlDir.QueryInterface(Components.interfaces.nsILocalFile);
  if(xmlDir.leafName == "xml") 
    break;
  else
    xmlDir = null;
}

// if we got our xmldir, find our testcases
var testDir;
if(xmlDir){
  entries = xmlDir.directoryEntries;
  while(entries.hasMoreElements()){
    testDir = entries.getNext();
    testDir.QueryInterface(Components.interfaces.nsILocalFile);
    if(testDir.exists() && testDir.isDirectory()){
      var testfiles = testDir.directoryEntries;
      while(testfiles.hasMoreElements()){ // inside an individual testdir, like "rss2"
        var test = testfiles.getNext();
        test.QueryInterface(Components.interfaces.nsILocalFile);
        if(test.exists() && test.isFile()){
          
          // Got a feed file, now we need to add it to our tests
          // and parse out the Description and Expect headers.
          // That lets us write one test.js file and keep the
          // tests with the XML. Convenient method learned from
          // Mark Pilgrim.

          var istream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                                  .createInstance(Components.interfaces.nsIFileInputStream);
          
          try{
            // open an input stream from file
            istream.init(test, 0x01, 0444, 0);
            istream.QueryInterface(Components.interfaces.nsILineInputStream);
            var line = {}, hasmore, testcase = {}, expectIndex, descIndex;

            do {
              hasmore = istream.readLine(line);
              expectIndex = line.value.indexOf('Expect:');
              descIndex = line.value.indexOf('Description:');
              baseIndex = line.value.indexOf('Base:');
              if(descIndex > -1)
                testcase.desc = trimString(line.value.substring(line.value.indexOf(':')+1));
              if(expectIndex > -1)
                testcase.expect = trimString(line.value.substring(line.value.indexOf(':')+1));
              if(baseIndex > -1)
                testcase.base = ioService.newURI(trimString(line.value.substring(line.value.indexOf(':')+1)),null,null);
              if(testcase.expect && testcase.desc){
                // got our fields, let's add it to our array and break
                testcase.path = xmlDir.leafName + '/' + testDir.leafName + '/' + test.leafName;
                testcase.file = test;
                tests.push(testcase);
                break; // out of do-while
              }
              
            } while(hasmore);

          }catch(e){
            dump("FAILED! Error reading test case in file " + test.leafName  + " " + e + "\n"); 
          }finally{
            istream.close();
          }
        }
      }
    }
  }
}

load(topDir.path+'/test.js');
