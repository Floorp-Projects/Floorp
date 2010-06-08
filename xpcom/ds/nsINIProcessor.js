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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
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


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function INIProcessorFactory() {
}

INIProcessorFactory.prototype = {
    classDescription: "INIProcessorFactory",
    contractID: "@mozilla.org/xpcom/ini-processor-factory;1",
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
function NSGetModule (compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
