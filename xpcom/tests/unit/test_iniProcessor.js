const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

let testnum = 0;
let factory;

function parserForFile(filename) {
    let parser = null;
    try {
        let file = do_get_file(filename);
        do_check_true(!!file);
        parser = factory.createINIParser(file);
        do_check_true(!!parser);
    } catch(e) {
	dump("INFO | caught error: " + e);
        // checkParserOutput will handle a null parser when it's expected.
    }
    return parser;
    
}

function checkParserOutput(parser, expected) {
    // If the expected output is null, we expect the parser to have
    // failed (and vice-versa).
    if (!parser || !expected) {
        do_check_eq(parser, null);
        do_check_eq(expected, null);
        return;
    }

    let output = getParserOutput(parser);
    for (let section in expected) {
        do_check_true(section in output);
        for (let key in expected[section]) {
            do_check_true(key in output[section]);
            do_check_eq(output[section][key], expected[section][key]);
            delete output[section][key];
        }
        for (let key in output[section])
            do_check_eq(key, "wasn't expecting this key!");
        delete output[section];
    }
    for (let section in output)
        do_check_eq(section, "wasn't expecting this section!");
}

function getParserOutput(parser) {
    let output = {};

    let sections = parser.getSections();
    do_check_true(!!sections);
    while (sections.hasMore()) {
        let section = sections.getNext();
        do_check_false(section in output); // catch dupes
        output[section] = {};

        let keys = parser.getKeys(section);
        do_check_true(!!keys);
        while (keys.hasMore()) {
            let key = keys.getNext();
            do_check_false(key in output[section]); // catch dupes
            let value = parser.getString(section, key);
            output[section][key] = value;
        }
    }
    return output;
}

function run_test() {
try {

let testdata = [
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

    let os = Cc["@mozilla.org/xre/app-info;1"]
             .getService(Ci.nsIXULRuntime).OS;
    if("WINNT" === os) {
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
factory = Cc["@mozilla.org/xpcom/ini-processor-factory;1"].
          getService(Ci.nsIINIParserFactory);
do_check_true(!!factory);

// Test reading from a variety of files. While we're at it, write out each one
// and read it back to ensure that nothing changed.
while (testnum < testdata.length) {
    dump("\nINFO | test #" + ++testnum);
    let filename = testdata[testnum -1].filename;
    dump(", filename " + filename + "\n");
    let parser = parserForFile(filename);
    checkParserOutput(parser, testdata[testnum - 1].reference);
    if (!parser)
        continue;
    do_check_true(parser instanceof Ci.nsIINIParserWriter);
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
let newfile = do_get_file("data/");
newfile.append("nonexistent-file.ini");
if (newfile.exists())
    newfile.remove(false);
do_check_false(newfile.exists());

let parser = factory.createINIParser(newfile);
do_check_true(!!parser);
do_check_true(parser instanceof Ci.nsIINIParserWriter);
checkParserOutput(parser, {});
parser.writeFile();
do_check_true(newfile.exists());

// test adding a new section and new key
parser.setString("section", "key", "value");
parser.writeFile();
do_check_true(newfile.exists());
checkParserOutput(parser, {section: {key: "value"} });
// read it in again, check for same data.
parser = parserForFile("data/nonexistent-file.ini");
checkParserOutput(parser, {section: {key: "value"} });
// cleanup after the test
newfile.remove(false);

dump("INFO | test #" + ++testnum + "\n");

// test modifying a existing key's value (in an existing section)
parser = parserForFile("data/iniparser09.ini");
checkParserOutput(parser, {section1: {name1: "value1"} });

do_check_true(parser instanceof Ci.nsIINIParserWriter);
parser.setString("section1", "name1", "value2");
checkParserOutput(parser, {section1: {name1: "value2"} });

dump("INFO | test #" + ++testnum + "\n");

// test trying to set illegal characters
let caughtError;
caughtError = false;
checkParserOutput(parser, {section1: {name1: "value2"} });

// Bad characters in section name
try { parser.SetString("bad\0", "ok", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("bad\r", "ok", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("bad\n", "ok", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("bad[", "ok", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("bad]", "ok", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);

// Bad characters in key name
caughtError = false;
try { parser.SetString("ok", "bad\0", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "bad\r", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "bad\n", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "bad=", "ok"); } catch (e) { caughtError = true; }
do_check_true(caughtError);

// Bad characters in value
caughtError = false;
try { parser.SetString("ok", "ok", "bad\0"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "ok", "bad\r"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "ok", "bad\n"); } catch (e) { caughtError = true; }
do_check_true(caughtError);
caughtError = false;
try { parser.SetString("ok", "ok", "bad="); } catch (e) { caughtError = true; }
do_check_true(caughtError);

} catch(e) {
    throw "FAILED in test #" + testnum + " -- " + e;
}
}
