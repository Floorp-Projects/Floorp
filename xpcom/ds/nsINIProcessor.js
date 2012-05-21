/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function INIProcessorFactory() {
}

INIProcessorFactory.prototype = {
    classID: Components.ID("{6ec5f479-8e13-4403-b6ca-fe4c2dca14fd}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIINIParserFactory]),

    createINIParser : function (aINIFile) {
        return new INIProcessor(aINIFile);
    }

}; // end of INIProcessorFactory implementation

const MODE_WRONLY = 0x02;
const MODE_CREATE = 0x08;
const MODE_TRUNCATE = 0x20;

// nsIINIParser implementation
function INIProcessor(aFile) {
    this._iniFile = aFile;
    this._iniData = {};
    this._readFile();
}

INIProcessor.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIINIParser, Ci.nsIINIParserWriter]),

    __utfConverter : null, // UCS2 <--> UTF8 string conversion
    get _utfConverter() {
        if (!this.__utfConverter) {
            this.__utfConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                                  createInstance(Ci.nsIScriptableUnicodeConverter);
            this.__utfConverter.charset = "UTF-8";
        }
        return this.__utfConverter;
    },

    _utfConverterReset : function() {
        this.__utfConverter = null;
    },

    _iniFile : null,
    _iniData : null,

    /*
     * Reads the INI file and stores the data internally.
     */
    _readFile : function() {
        // If file doesn't exist, there's nothing to do.
        if (!this._iniFile.exists() || 0 == this._iniFile.fileSize)
            return;

        let iniParser = Cc["@mozilla.org/xpcom/ini-parser-factory;1"]
            .getService(Ci.nsIINIParserFactory).createINIParser(this._iniFile);
        for (let section in XPCOMUtils.IterStringEnumerator(iniParser.getSections())) {
            this._iniData[section] = {};
            for (let key in XPCOMUtils.IterStringEnumerator(iniParser.getKeys(section))) {
                this._iniData[section][key] = iniParser.getString(section, key);
            }
        }
    },

    // nsIINIParser

    getSections : function() {
        let sections = [];
        for (let section in this._iniData)
            sections.push(section);
        return new stringEnumerator(sections);
    },

    getKeys : function(aSection) {
        let keys = [];
        if (aSection in this._iniData)
            for (let key in this._iniData[aSection])
                keys.push(key);
        return new stringEnumerator(keys);
    },

    getString : function(aSection, aKey) {
        if (!(aSection in this._iniData))
            throw Cr.NS_ERROR_FAILURE;
        if (!(aKey in this._iniData[aSection]))
            throw Cr.NS_ERROR_FAILURE;
        return this._iniData[aSection][aKey];
    },


    // nsIINIParserWriter

    setString : function(aSection, aKey, aValue) {
        const isSectionIllegal = /[\0\r\n\[\]]/;
        const isKeyValIllegal  = /[\0\r\n=]/;

        if (isSectionIllegal.test(aSection))
            throw Components.Exception("bad character in section name",
                                       Cr.ERROR_ILLEGAL_VALUE);
        if (isKeyValIllegal.test(aKey) || isKeyValIllegal.test(aValue))
            throw Components.Exception("bad character in key/value",
                                       Cr.ERROR_ILLEGAL_VALUE);

        if (!(aSection in this._iniData))
            this._iniData[aSection] = {};

        this._iniData[aSection][aKey] = aValue;
    },

    writeFile : function(aFile) {

        let converter = this._utfConverter;
        function writeLine(data) {
            data = converter.ConvertFromUnicode(data);
            data += converter.Finish();
            data += "\n";
            outputStream.write(data, data.length);
        }

        if (!aFile)
            aFile = this._iniFile;

        let safeStream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                         createInstance(Ci.nsIFileOutputStream);
        safeStream.init(aFile, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE,
                        0600, null);

        var outputStream = Cc["@mozilla.org/network/buffered-output-stream;1"].
                           createInstance(Ci.nsIBufferedOutputStream);
        outputStream.init(safeStream, 8192);
        outputStream.QueryInterface(Ci.nsISafeOutputStream); // for .finish()

        for (let section in this._iniData) {
            writeLine("[" + section + "]");
            for (let key in this._iniData[section]) {
                writeLine(key + "=" + this._iniData[section][key]);
            }
        }

        outputStream.finish();
    }
};

function stringEnumerator(stringArray) {
    this._strings = stringArray;
}
stringEnumerator.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIUTF8StringEnumerator]),

    _strings : null,
    _enumIndex: 0,

    hasMore : function() {
        return (this._enumIndex < this._strings.length);
    },

    getNext : function() {
        return this._strings[this._enumIndex++];
    }
};

let component = [INIProcessorFactory];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
