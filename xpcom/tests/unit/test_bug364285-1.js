Cu.import("resource://gre/modules/Services.jsm");

var nameArray = [
 "ascii",                                           // ASCII
 "fran\u00E7ais",                                   // Latin-1
 "\u0420\u0443\u0441\u0441\u043A\u0438\u0439",      // Cyrillic
 "\u65E5\u672C\u8A9E",                              // Japanese
 "\u4E2D\u6587",                                    // Chinese
 "\uD55C\uAD6D\uC5B4",                              // Korean
 "\uD801\uDC0F\uD801\uDC2D\uD801\uDC3B\uD801\uDC2B" // Deseret
];

function getTempDir() {
    return Services.dirsvc.get("TmpD", Ci.nsIFile);
}

function create_file(fileName) {
    var outFile = getTempDir();
    outFile.append(fileName);
    outFile.createUnique(outFile.NORMAL_FILE_TYPE, 0o600);

    var stream = Cc["@mozilla.org/network/file-output-stream;1"]
        .createInstance(Ci.nsIFileOutputStream);
    stream.init(outFile, 0x02 | 0x08 | 0x20, 0o600, 0);
    stream.write("foo", 3);
    stream.close();

    Assert.equal(outFile.leafName.substr(0, fileName.length), fileName);

    return outFile;
}

function test_create(fileName) {
    var file1 = create_file(fileName);
    var file2 = create_file(fileName);
    file1.remove(false);
    file2.remove(false);
}

function run_test() {
    for (var i = 0; i < nameArray.length; ++i) {
        test_create(nameArray[i]);
    }
}
