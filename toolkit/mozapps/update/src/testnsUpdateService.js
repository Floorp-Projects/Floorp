
function assertThrows(assertion, command) {
  try {
    eval(command);
    return false;
  }
  catch (e) {
    if (e && e == assertion) {
      return true;
    } else {
      return false;
    }
  }
}

function testUpdatePatchThrowsExceptionForZeroSizePatch(updateString) {
  
  var parser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
    .createInstance(Components.interfaces.nsIDOMParser);
  var doc = parser.parseFromString(updateString, "text/xml");
  
  var updateCount = doc.documentElement.childNodes.length;
  
  var result = "fail";
  
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
      
      if (assertThrows(Components.results.NS_ERROR_ILLEGAL_VALUE, "var patch = new UpdatePatch(patchElement);")) {
        result = "pass";
      }
    }
  }
  print(result + "\n");
}

testUpdatePatchThrowsExceptionForZeroSizePatch('<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="0"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="652"/></update></updates>');
testUpdatePatchThrowsExceptionForZeroSizePatch('<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="8780423"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="0"/></update></updates>');
// next test should fail - need assertNotThrow()
testUpdatePatchThrowsExceptionForZeroSizePatch('<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="8780423"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="652"/></update></updates>');
testUpdatePatchThrowsExceptionForZeroSizePatch('<?xml version="1.0"?><updates><update type="minor" version="1.7" extensionVersion="1.7" buildID="2005102512"><patch type="complete" URL="DUMMY" hashFunction="MD5" hashValue="9a76b75088671550245428af2194e083" size="0"/><patch type="partial" URL="DUMMY" hashFunction="MD5" hashValue="71000cd10efc7371402d38774edf3b2c" size="0"/></update></updates>');
