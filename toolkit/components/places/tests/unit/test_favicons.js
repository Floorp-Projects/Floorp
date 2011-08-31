/*
 * Tests for nsIFaviconService
 */

/*
 * dumpToFile()
 *
 * For test development, dumps the specified array to a file.
 * Call |dumpToFile(outData);| in a test to file to a file.
 */
function dumpToFile(aData) {
  const path = "/tmp";

  var outputFile = Cc["@mozilla.org/file/local;1"].
                   createInstance(Ci.nsILocalFile);
  outputFile.initWithPath(path);
  outputFile.append("testdump.png");

  var outputStream = Cc["@mozilla.org/network/file-output-stream;1"].
                     createInstance(Ci.nsIFileOutputStream);
  // WR_ONLY|CREAT|TRUNC
  outputStream.init(outputFile, 0x02 | 0x08 | 0x20, 0644, null);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].
            createInstance(Ci.nsIBinaryOutputStream);
  bos.setOutputStream(outputStream);

  bos.writeByteArray(aData, aData.length);

  outputStream.close();
}

/*
 * setAndGetFaviconData()
 *
 * Calls setFaviconData() with the specified image data,
 * and then retrieves it with getFaviconData(). Returns
 * and array of bytes and mimetype.
 */
function setAndGetFaviconData(aFilename, aData, aMimeType) {
  var iconURI = uri("http://places.test/" + aFilename);
  try {
    iconsvc.setFaviconData(iconURI,
                           aData, aData.length, aMimeType,
                           Number.MAX_VALUE);
  } catch (ex) {}
  var dataURL = iconsvc.getFaviconDataAsDataURL(iconURI);
  try {
    iconsvc.setFaviconDataFromDataURL(iconURI, dataURL, Number.MAX_VALUE);
  } catch (ex) {}
  var mimeTypeOutparam = {};

  var outData = iconsvc.getFaviconData(iconURI, mimeTypeOutparam);

  return [outData, mimeTypeOutparam.value];
}


// Get favicon service
try {
  var iconsvc = PlacesUtils.favicons;

  // Ugh, this is an ugly hack. The pixel values we get in Windows are sometimes
  // +/- 1 value compared to other platforms, so we need to compare against a
  // different set of reference images. nsIXULRuntime.OS doesn't seem to be
  // available in xpcshell, so we'll use this as a kludgy way to figure out if
  // we're running on Windows.
  var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
} catch(ex) {
  do_throw("Could not get favicon service\n");
}

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history services\n");
}

function checkArrays(a, b) {
  do_check_true(compareArrays(a, b));
}

function run_test() {
try {

/* ========== 1 ========== */
var testnum = 1;
var testdesc = "test storing a normal 16x16 icon";

// 16x16 png, 286 bytes.
var iconName = "favicon-normal16.png";
var inMimeType = "image/png";
var iconFile = do_get_file(iconName);

var inData = readFileData(iconFile);
do_check_eq(inData.length, 286);

var [outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Ensure input and output are identical
do_check_eq(inMimeType, outMimeType);
checkArrays(inData, outData);
                    

/* ========== 2 ========== */
testnum++;
testdesc = "test storing a normal 32x32 icon";

// 32x32 png, 344 bytes.
iconName = "favicon-normal32.png";
inMimeType = "image/png";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 344);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Ensure input and output are identical
do_check_eq(inMimeType, outMimeType);
checkArrays(inData, outData);


/* ========== 3 ========== */
testnum++;
testdesc = "test storing an oversize 16x16 icon ";

//  in: 16x16 ico, 1406 bytes.
// out: 16x16 png 
iconName = "favicon-big16.ico";
inMimeType = "image/x-icon";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 1406);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);

/* ========== 4 ========== */
testnum++;
testdesc = "test storing an oversize 4x4 icon ";

//  in: 4x4 jpg, 4751 bytes.
// out: 16x16 png 
iconName = "favicon-big4.jpg";
inMimeType = "image/jpeg";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 4751);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);


/* ========== 5 ========== */
testnum++;
testdesc = "test storing an oversize 32x32 icon ";

//  in: 32x32 jpg, 3494 bytes.
// out: 16x16 png 
iconName = "favicon-big32.jpg";
inMimeType = "image/jpeg";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 3494);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
// Disabled on Windows due to problems with pixels varying slightly.
if (!isWindows)
  checkArrays(expectedData, outData);


/* ========== 6 ========== */
testnum++;
testdesc = "test storing an oversize 48x48 icon ";

//  in: 48x48 ico, 56646 bytes.
// (howstuffworks.com icon, contains 13 icons with sizes from 16x16 to
// 48x48 in varying depths)
// out: 16x16 png 
iconName = "favicon-big48.ico";
inMimeType = "image/x-icon";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 56646);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);

/* ========== 7 ========== */
testnum++;
testdesc = "test storing an oversize 64x64 icon ";

//  in: 64x64 png, 10698 bytes.
// out: 16x16 png 
iconName = "favicon-big64.png";
inMimeType = "image/png";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 10698);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);

/* ========== 8 ========== */
testnum++;
testdesc = "test scaling an oversize 160x3 icon ";

//  in: 160x3 jpg, 5095 bytes.
// out: 16x16 png 
iconName = "favicon-scale160x3.jpg";
inMimeType = "image/jpeg";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 5095);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);

/* ========== 9 ========== */
testnum++;
testdesc = "test scaling an oversize 3x160 icon ";

//  in: 3x160 jpg, 5059 bytes.
// out: 16x16 png 
iconName = "favicon-scale3x160.jpg";
inMimeType = "image/jpeg";
iconFile = do_get_file(iconName);

inData = readFileData(iconFile);
do_check_eq(inData.length, 5059);

[outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);

// Read in the expected output.
var expectedFile = do_get_file("expected-" + iconName + ".png");
var expectedData = readFileData(expectedFile);

// Compare thet expected data to the actual data.
do_check_eq("image/png", outMimeType);
checkArrays(expectedData, outData);


/* ========== 10 ========== */
testnum++;
testdesc = "test set and get favicon ";

// 32x32 png, 344 bytes.
var icon1Name = "favicon-normal32.png";
var icon1MimeType = "image/png";
var icon1File = do_get_file(icon1Name);
var icon1Data = readFileData(icon1File);
do_check_eq(icon1Data.length, 344);
var icon1URI = uri("file:///./" + icon1Name);

// 16x16 png, 286 bytes.
var icon2Name = "favicon-normal16.png";
var icon2MimeType = "image/png";
var icon2File = do_get_file(icon2Name);
var icon2Data = readFileData(icon2File);
do_check_eq(icon2Data.length, 286);
var icon2URI = uri("file:///./" + icon2Name);

var page1URI = uri("http://foo.bar/");
var page2URI = uri("http://bar.foo/");
var page3URI = uri("http://foo.bar.moz/");

// add visits to the db
histsvc.addVisit(page1URI, Date.now() * 1000, null,
                 histsvc.TRANSITION_TYPED, false, 0);
histsvc.addVisit(page2URI, Date.now() * 1000, null,
                 histsvc.TRANSITION_TYPED, false, 0);
histsvc.addVisit(page3URI, Date.now() * 1000, null,
                 histsvc.TRANSITION_TYPED, false, 0);

// set first page icon
try {
  iconsvc.setFaviconData(icon1URI, icon1Data, icon1Data.length,
                         icon1MimeType, Number.MAX_VALUE);
} catch (ex) {}
iconsvc.setFaviconUrlForPage(page1URI, icon1URI);
do_check_guid_for_uri(page1URI);
var savedIcon1URI = iconsvc.getFaviconForPage(page1URI);

// Test getFaviconForPage().
do_test_pending();
iconsvc.getFaviconURLForPage(page1URI, {
    onFaviconDataAvailable: function(aURI, aDataLen, aData, aMimeType) {
      do_check_true(aURI.equals(savedIcon1URI));
      do_check_eq(aDataLen, 0);
      do_check_eq(aData.length, 0);
      do_check_eq(aMimeType, "");
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFaviconDataCallback])
});

iconsvc.getFaviconDataForPage(page1URI, {
    onFaviconDataAvailable: function(aURI, aDataLen, aData, aMimeType) {
      do_check_true(aURI.equals(savedIcon1URI));
      do_check_eq(icon1MimeType, out1MimeType.value);
      checkArrays(icon1Data, aData);
      do_check_eq(aDataLen, aData.length);
      do_test_finished();
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFaviconDataCallback])
});

// set second page icon
try {
  iconsvc.setFaviconData(icon2URI, icon2Data, icon2Data.length,
                         icon2MimeType, Number.MAX_VALUE);
} catch (ex) {}
iconsvc.setFaviconUrlForPage(page2URI, icon2URI);
do_check_guid_for_uri(page2URI);
var savedIcon2URI = iconsvc.getFaviconForPage(page2URI);

// set third page icon as the same as first page one
try {
  iconsvc.setFaviconData(icon1URI, icon1Data, icon1Data.length,
                         icon1MimeType, Number.MAX_VALUE);
} catch (ex) {}
iconsvc.setFaviconUrlForPage(page3URI, icon1URI);
do_check_guid_for_uri(page3URI);
var savedIcon3URI = iconsvc.getFaviconForPage(page3URI);

// check first page icon
var out1MimeType = {};
var out1Data = iconsvc.getFaviconData(savedIcon1URI, out1MimeType);
do_check_eq(icon1MimeType, out1MimeType.value);
checkArrays(icon1Data, out1Data);

// check second page icon
var out2MimeType = {};
var out2Data = iconsvc.getFaviconData(savedIcon2URI, out2MimeType);
do_check_eq(icon2MimeType, out2MimeType.value);
checkArrays(icon2Data, out2Data);

// check third page icon
var out3MimeType = {};
var out3Data = iconsvc.getFaviconData(savedIcon3URI, out3MimeType);
do_check_eq(icon1MimeType, out3MimeType.value);
checkArrays(icon1Data, out3Data);


/* ========== 11 ========== */
testnum++;
testdesc = "test favicon links ";

var pageURI = uri("http://foo.bar/");
var faviconURI = uri("file:///./favicon-normal32.png");
do_check_eq(iconsvc.getFaviconImageForPage(pageURI).spec,
            iconsvc.getFaviconLinkForIcon(faviconURI).spec);


/* ========== 12 ========== */
testnum++;
testdesc = "test failed favicon cache ";

// 32x32 png, 344 bytes.
iconName = "favicon-normal32.png";
faviconURI = uri("file:///./" + iconName);

iconsvc.addFailedFavicon(faviconURI);
do_check_true(iconsvc.isFailedFavicon(faviconURI));
iconsvc.removeFailedFavicon(faviconURI);
do_check_false(iconsvc.isFailedFavicon(faviconURI));


/* ========== 13 ========== */
testnum++;
testdesc = "test getFaviconData on the default favicon ";

outMimeType = {};
outData = iconsvc.getFaviconData(iconsvc.defaultFavicon, outMimeType);
do_check_eq(outMimeType.value, "image/png");

// Read in the icon and compare it to what the API returned above.
var istream = NetUtil.newChannel(iconsvc.defaultFavicon).open();
var bistream = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
bistream.setInputStream(istream);
expectedData = [];
var avail;
while (avail = bistream.available()) {
  expectedData = expectedData.concat(bistream.readByteArray(avail));
}
bistream.close();
checkArrays(outData, expectedData);


/* ========== end ========== */

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
