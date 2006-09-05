/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Update Service Unit Test.
 *
 * The Initial Developer of the Original Code is Dave Liebreich.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dave Liebreich <davel@mozilla.org> (Original Author)
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
 * ***** END LICENSE BLOCK ***** */

/* Much of the following harness-like code is copied from jsunit(jsUnitCore.js)
 * http://jsunit.net is distributed under GPL 2.0, GLPL 2.1, and MPL 1.1
 * If and when jsunit is incorporated into the mozilla project as a test harness, much of the
 * code in this file can be eliminated as duplicate.
 */

// make jsUnitCore.js stuff happy about running not in a browser content window
var xbDEBUG = false;
var top = new Object();
var window = new Object();
window.document = new Object();
var navigator = new Object();
navigator.userAgent = new Object();

navigator.userAgent.indexOf = function () {
  return -1;
}

// cut-n-paste stuff from jsUnitCore.js
function parseErrorStack(excp)
{
    var stack = [];
    var name;

    if (!excp || !excp.stack)
    {
        return stack;
    }

    var stacklist = excp.stack.split('\n');

    for (var i = 0; i < stacklist.length - 1; i++)
    {
        var framedata = stacklist[i];

        name = framedata.match(/^(\w*)/)[1];
        if (!name) {
            name = 'anonymous';
        }

        stack[stack.length] = name;
    }
    // remove top level anonymous functions to match IE

    while (stack.length && stack[stack.length - 1] == 'anonymous')
    {
        stack.length = stack.length - 1;
    }
    return stack;
}

function getStackTrace() {
    var result = '';

    if (typeof(arguments.caller) != 'undefined') { // IE, not ECMA
        for (var a = arguments.caller; a != null; a = a.caller) {
            result += '> ' + getFunctionName(a.callee) + '\n';
            if (a.caller == a) {
                result += '*';
                break;
            }
        }
    }
    else { // Mozilla, not ECMA
        // fake an exception so we can get Mozilla's error stack
        var testExcp;
        try
        {
            foo.bar;
        }
        catch(testExcp)
        {
            var stack = parseErrorStack(testExcp);
            for (var i = 1; i < stack.length; i++)
            {
                result += '> ' + stack[i] + '\n';
            }
        }
    }

    return result;
}

function JsUnitException(comment, message) {
    this.isJsUnitException = true;
    this.comment = comment;
    this.jsUnitMessage = message;
    this.stackTrace = getStackTrace();
}

function _displayStringForValue(aVar) {
    var result = '<' + aVar + '>';
    if (!(aVar === null || aVar === top.JSUNIT_UNDEFINED_VALUE)) {
        result += ' (' + _trueTypeOf(aVar) + ')';
    }
    return result;
}

function commentArg(expectedNumberOfNonCommentArgs, args) {
    if (argumentsIncludeComments(expectedNumberOfNonCommentArgs, args))
        return args[0];

    return null;
}

function _assert(comment, booleanValue, failureMessage) {
    if (!booleanValue)
        throw new JsUnitException(comment, failureMessage);
}

function argumentsIncludeComments(expectedNumberOfNonCommentArgs, args) {
    return args.length == expectedNumberOfNonCommentArgs + 1;
}

function nonCommentArg(desiredNonCommentArgIndex, expectedNumberOfNonCommentArgs, args) {
    return argumentsIncludeComments(expectedNumberOfNonCommentArgs, args) ?
           args[desiredNonCommentArgIndex] :
           args[desiredNonCommentArgIndex - 1];
}

function _validateArguments(expectedNumberOfNonCommentArgs, args) {
    if (!( args.length == expectedNumberOfNonCommentArgs ||
           (args.length == expectedNumberOfNonCommentArgs + 1 && typeof(args[0]) == 'string') ))
        error('Incorrect arguments passed to assert function');
}
function assertNull() {
    _validateArguments(1, arguments);
    var aVar = nonCommentArg(1, 1, arguments);
    _assert(commentArg(1, arguments), aVar === null, 'Expected ' + _displayStringForValue(null) + ' but was ' + _displayStringForValue(aVar));
}

// start of original code 
global_object = this;
mytestManager = new Object();

mytestManager.executeTestFunction = function(functionName) {
  this._testFunctionName = functionName;
  var excep = null;
  assertNull(excep)
  var timeBefore = new Date();
  try {
    global_object[this._testFunctionName]();
  }
  catch (e1) {
    excep = e1
  }
  var timeTaken = (new Date() - timeBefore) / 1000;
  print (timeTaken);
  
  if (excep != null) {
    var problemMessage = this._testFunctionName + ' ';
    
    if (typeof(excep.isJsUnitException) == 'undefined' || !excep.isJsUnitException) {
      problemMessage += 'had an error';
      this.errorCount++;
    }
    else {
      problemMessage += 'failed';
      this.failureCount++;
    }
    print(problemMessage);
    print(this._problemDetailMessageFor(excep));
  }
}

mytestManager._problemDetailMessageFor = function (excep) {
    var result = null;
    if (typeof(excep.isJsUnitException) != 'undefined' && excep.isJsUnitException) {
        result = '';
        if (excep.comment != null)
            result += ('"' + excep.comment + '"\n');

        result += excep.jsUnitMessage;

        if (excep.stackTrace)
            result += '\n\nStack trace follows:\n' + excep.stackTrace;
    }
    else {
        result = 'Error message is:\n"';
        result +=
        (typeof(excep.description) == 'undefined') ?
        excep :
        excep.description;
        result += '"';
        if (typeof(excep.stack) != 'undefined') // Mozilla only
            result += '\n\nStack trace follows:\n' + excep.stack;
    }
    return result;
}

mytestManager.runAllTests = function () {
  for (var test in global_object) {
    if ((typeof(global_object[test]) == "function") && test.match('^test')) {
      mytestManager.executeTestFunction(test);
    }
  }
}

function assertThrows(assertion, command) {

  try {
    eval(command);
    _assert(command, false, 'Expected assertion, but got none');
  }
  catch (e) {
    if (typeof(e.isJsUnitException) != 'undefined' && e.isJsUnitException) {
      throw e;
    }
    else {
      _assert(command, (e && e == assertion), 'Expected assertion ' + assertion + ', but was ' + e);
    }
  }
 
}

function assertDoesNotThrow(assertion, command) {

  try {
    eval(command);
  }
  catch (e) {
    if (typeof(e.isJsUnitException) != 'undefined' && e.isJsUnitException) {
      throw e;
    }
    else {
      _assert(command, (e && e != assertion), 'Expected anything but assertion ' + assertion + ', and caught ' + e);
    }
  }
 
}
function doUpdatePatchFromXML(updateString) {
  
  var parser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
    .createInstance(Components.interfaces.nsIDOMParser);
  var doc = parser.parseFromString(updateString, "text/xml");
  
  var updateCount = doc.documentElement.childNodes.length;
  
  for (var i = 0; i < updateCount; ++i) {
    var update = doc.documentElement.childNodes.item(i);
    if (update.nodeType != Node.ELEMENT_NODE ||
        update.localName != "update")
      continue;
    
    update.QueryInterface(Components.interfaces.nsIDOMElement);
    
    for (var i = 0; i < update.childNodes.length; ++i) {
      // patchElement needs to be global so the eval in assertThrows will work
      patchElement = update.childNodes.item(i);
      if (patchElement.nodeType != Node.ELEMENT_NODE ||
          patchElement.localName != "patch")
        continue;
      
      patchElement.QueryInterface(Components.interfaces.nsIDOMElement);
      
      var patch = new UpdatePatch(patchElement);
    }
  }
}

updateXMLThrows = new Object();
updateXMLThrows.zeroComplete = '<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="0"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="652"/></update></updates>';
updateXMLThrows.zeroPartial  = '<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="8780423"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="0"/></update></updates>';
updateXMLThrows.zeroBoth     = '<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="0"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="0"/></update></updates>';

updateXML = new Object();
updateXML.nonZeroBoth  = '<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="8780423"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="652"/></update></updates>';

for (var i in updateXMLThrows) {
  eval("function testUpdatePatchThrowsExceptionFor" + i
    + "() { assertThrows(Components.results.NS_ERROR_ILLEGAL_VALUE, "
    + "\"doUpdatePatchFromXML(updateXMLThrows['"+i+"'])\");}");
}

for (var i in updateXML) {
  eval("function testUpdatePatchThrowsExceptionFor" + i
    + "() { assertDoesNotThrow(Components.results.NS_ERROR_ILLEGAL_VALUE, "
    + "\"doUpdatePatchFromXML(updateXML['"+i+"'])\");}");
}

mytestManager.runAllTests();
