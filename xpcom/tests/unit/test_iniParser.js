var testnum = 0;
var factory;

function parserForFile(filename) {
    let parser = null;
    try {
        let file = do_get_file(filename);
        Assert.ok(!!file);
        parser = factory.createINIParser(file);
        Assert.ok(!!parser);
    } catch (e) {
        dump("INFO | caught error: " + e);
        // checkParserOutput will handle a null parser when it's expected.
    }
    return parser;
}

function checkParserOutput(parser, expected) {
    // If the expected output is null, we expect the parser to have
    // failed (and vice-versa).
    if (!parser || !expected) {
        Assert.equal(parser, null);
        Assert.equal(expected, null);
        return;
    }

    let output = getParserOutput(parser);
    for (let section in expected) {
        Assert.ok(section in output);
        for (let key in expected[section]) {
            Assert.ok(key in output[section]);
            Assert.equal(output[section][key], expected[section][key]);
            delete output[section][key];
        }
        for (let key in output[section])
            Assert.equal(key, "wasn't expecting this key!");
        delete output[section];
    }
    for (let section in output)
        Assert.equal(section, "wasn't expecting this section!");
}

function getParserOutput(parser) {
    let output = {};

    for (let section of parser.getSections()) {
        Assert.equal(false, section in output); // catch dupes
        output[section] = {};

        for (let key of parser.getKeys(section)) {
            Assert.equal(false, key in output[section]); // catch dupes
            let value = parser.getString(section, key);
            output[section][key] = value;
        }
    }
    return output;
}

function run_test() {
try {
var testdata = [
    { filename: "data/iniparser01.ini", reference: {} },
    { filename: "data/iniparser02.ini", reference: {} },
    { filename: "data/iniparser03.ini", reference: {} },
    { filename: "data/iniparser04.ini", reference: {} },
    { filename: "data/iniparser05.ini", reference: {} },
    { filename: "data/iniparser06.ini", reference: {} },
    { filename: "data/iniparser07.ini", reference: {} },
    { filename: "data/iniparser08.ini", reference: { section1: { name1: "" }} },
    { filename: "data/iniparser09.ini", reference: { section1: { name1: "value1" } } },
    { filename: "data/iniparser10.ini", reference: { section1: { name1: "value1" } } },
    { filename: "data/iniparser11.ini", reference: { section1: { name1: "value1" } } },
    { filename: "data/iniparser12.ini", reference: { section1: { name1: "value1" } } },
    { filename: "data/iniparser13.ini", reference: { section1: { name1: "value1" } } },
    { filename: "data/iniparser14.ini", reference:
                    { section1: { name1: "value1", name2: "value2" },
                      section2: { name1: "value1", name2: "foopy"  }} },
    { filename: "data/iniparser15.ini", reference:
                    { section1: { name1: "newValue1" },
                      section2: { name1: "foopy"     }} },
    { filename: "data/iniparser16.ini", reference:
                    { "☺♫": { "♫": "☻", "♪": "♥"  },
                       "☼": { "♣": "♠", "♦": "♥"  }} },
    { filename: "data/iniparser17.ini", reference: { section: { key: "" } } },
    ];

    testdata.push( { filename: "data/iniparser01-utf8BOM.ini",
                     reference: testdata[0].reference } );
    testdata.push( { filename: "data/iniparser02-utf8BOM.ini",
                     reference: testdata[1].reference } );
    testdata.push( { filename: "data/iniparser03-utf8BOM.ini",
                     reference: testdata[2].reference } );
    testdata.push( { filename: "data/iniparser04-utf8BOM.ini",
                     reference: testdata[3].reference } );
    testdata.push( { filename: "data/iniparser05-utf8BOM.ini",
                     reference: testdata[4].reference } );
    testdata.push( { filename: "data/iniparser06-utf8BOM.ini",
                     reference: testdata[5].reference } );
    testdata.push( { filename: "data/iniparser07-utf8BOM.ini",
                     reference: testdata[6].reference } );
    testdata.push( { filename: "data/iniparser08-utf8BOM.ini",
                     reference: testdata[7].reference } );
    testdata.push( { filename: "data/iniparser09-utf8BOM.ini",
                     reference: testdata[8].reference } );
    testdata.push( { filename: "data/iniparser10-utf8BOM.ini",
                     reference: testdata[9].reference } );
    testdata.push( { filename: "data/iniparser11-utf8BOM.ini",
                     reference: testdata[10].reference } );
    testdata.push( { filename: "data/iniparser12-utf8BOM.ini",
                     reference: testdata[11].reference } );
    testdata.push( { filename: "data/iniparser13-utf8BOM.ini",
                     reference: testdata[12].reference } );
    testdata.push( { filename: "data/iniparser14-utf8BOM.ini",
                     reference: testdata[13].reference } );
    testdata.push( { filename: "data/iniparser15-utf8BOM.ini",
                     reference: testdata[14].reference } );
    testdata.push( { filename: "data/iniparser16-utf8BOM.ini",
                     reference: testdata[15].reference } );

    // Intentional test for appInfo that can't be preloaded.
    // eslint-disable-next-line mozilla/use-services
    let os = Cc["@mozilla.org/xre/app-info;1"]
             .getService(Ci.nsIXULRuntime).OS;
    if ("WINNT" === os) {
        testdata.push( { filename: "data/iniparser01-utf16leBOM.ini",
                         reference: testdata[0].reference } );
        testdata.push( { filename: "data/iniparser02-utf16leBOM.ini",
                         reference: testdata[1].reference } );
        testdata.push( { filename: "data/iniparser03-utf16leBOM.ini",
                         reference: testdata[2].reference } );
        testdata.push( { filename: "data/iniparser04-utf16leBOM.ini",
                         reference: testdata[3].reference } );
        testdata.push( { filename: "data/iniparser05-utf16leBOM.ini",
                         reference: testdata[4].reference } );
        testdata.push( { filename: "data/iniparser06-utf16leBOM.ini",
                         reference: testdata[5].reference } );
        testdata.push( { filename: "data/iniparser07-utf16leBOM.ini",
                         reference: testdata[6].reference } );
        testdata.push( { filename: "data/iniparser08-utf16leBOM.ini",
                         reference: testdata[7].reference } );
        testdata.push( { filename: "data/iniparser09-utf16leBOM.ini",
                         reference: testdata[8].reference } );
        testdata.push( { filename: "data/iniparser10-utf16leBOM.ini",
                         reference: testdata[9].reference } );
        testdata.push( { filename: "data/iniparser11-utf16leBOM.ini",
                         reference: testdata[10].reference } );
        testdata.push( { filename: "data/iniparser12-utf16leBOM.ini",
                         reference: testdata[11].reference } );
        testdata.push( { filename: "data/iniparser13-utf16leBOM.ini",
                         reference: testdata[12].reference } );
        testdata.push( { filename: "data/iniparser14-utf16leBOM.ini",
                         reference: testdata[13].reference } );
        testdata.push( { filename: "data/iniparser15-utf16leBOM.ini",
                         reference: testdata[14].reference } );
        testdata.push( { filename: "data/iniparser16-utf16leBOM.ini",
                         reference: testdata[15].reference } );
    }

/* ========== 0 ========== */
factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
          getService(Ci.nsIINIParserFactory);
Assert.ok(!!factory);

// Test reading from a variety of files. While we're at it, write out each one
// and read it back to ensure that nothing changed.
while (testnum < testdata.length) {
    dump("\nINFO | test #" + ++testnum);
    let filename = testdata[testnum - 1].filename;
    dump(", filename " + filename + "\n");
    let parser = parserForFile(filename);
    checkParserOutput(parser, testdata[testnum - 1].reference);
    if (!parser)
        continue;
    Assert.ok(parser instanceof Ci.nsIINIParserWriter);
    // write contents out to a new file
    let newfilename = filename + ".new";
    let newfile = do_get_file(filename);
    newfile.leafName += ".new";
    parser.writeFile(newfile);
    // read new file and make sure the contents are the same.
    parser = parserForFile(newfilename);
    checkParserOutput(parser, testdata[testnum - 1].reference);
    // cleanup after the test
    newfile.remove(false);
}

dump("INFO | test #" + ++testnum + "\n");

// test writing to a new file.
var newfile = do_get_file("data/");
newfile.append("nonexistent-file.ini");
if (newfile.exists())
    newfile.remove(false);
Assert.ok(!newfile.exists());

try {
    var parser = factory.createINIParser(newfile);
    Assert.ok(false, "Should have thrown an exception");
} catch (e) {
    Assert.equal(e.result, Cr.NS_ERROR_FILE_NOT_FOUND, "Caught a file not found exception");
}
parser = factory.createINIParser();
Assert.ok(!!parser);
Assert.ok(parser instanceof Ci.nsIINIParserWriter);
checkParserOutput(parser, {});
parser.writeFile(newfile);
Assert.ok(newfile.exists());

// test adding a new section and new key
parser.setString("section", "key", "value");
parser.setString("section", "key2", "");
parser.writeFile(newfile);
Assert.ok(newfile.exists());
checkParserOutput(parser, {section: {key: "value", key2: ""} });
// read it in again, check for same data.
parser = parserForFile("data/nonexistent-file.ini");
checkParserOutput(parser, {section: {key: "value", key2: ""} });
// cleanup after the test
newfile.remove(false);

dump("INFO | test #" + ++testnum + "\n");

// test modifying a existing key's value (in an existing section)
parser = parserForFile("data/iniparser09.ini");
checkParserOutput(parser, {section1: {name1: "value1"} });

Assert.ok(parser instanceof Ci.nsIINIParserWriter);
parser.setString("section1", "name1", "value2");
checkParserOutput(parser, {section1: {name1: "value2"} });

dump("INFO | test #" + ++testnum + "\n");

// test trying to set illegal characters
var caughtError;
caughtError = null;
checkParserOutput(parser, {section1: {name1: "value2"} });

// Bad characters in section name
try { parser.setString("bad\0", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("bad\r", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("bad\n", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("bad[", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("bad]", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("", "ok", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);

// Bad characters in key name
caughtError = null;
try { parser.setString("ok", "bad\0", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "bad\r", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "bad\n", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "bad=", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "", "ok"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);

// Bad characters in value
caughtError = null;
try { parser.setString("ok", "ok", "bad\0"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "ok", "bad\r"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "ok", "bad\n"); } catch (e) { caughtError = e; }
Assert.ok(caughtError);
Assert.equal(caughtError.result, Cr.NS_ERROR_INVALID_ARG);
caughtError = null;
try { parser.setString("ok", "ok", "good="); } catch (e) { caughtError = e; }
Assert.ok(!caughtError);
caughtError = null;
} catch (e) {
    throw new Error(`FAILED in test #${testnum} -- ${e}`);
}
}
