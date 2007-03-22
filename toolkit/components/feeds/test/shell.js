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
 * The Original Code is Feed Parser Unit Tests.
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
