"use strict";
/* eslint-disable no-unused-vars */

// Loaded into the same scope as head_xpc.js
/* import-globals-from head_xpc.js */

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { NormandyApi } = ChromeUtils.import(
  "resource://normandy/lib/NormandyApi.jsm"
);
const { NormandyTestUtils } = ChromeUtils.import(
  "resource://testing-common/NormandyTestUtils.jsm"
);

const CryptoHash = Components.Constructor(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);
const FileInputStream = Components.Constructor(
  "@mozilla.org/network/file-input-stream;1",
  "nsIFileInputStream",
  "init"
);

class MockResponse {
  constructor(content) {
    this.content = content;
  }

  async text() {
    return this.content;
  }

  async json() {
    return JSON.parse(this.content);
  }
}

function withServer(server) {
  return function(testFunction) {
    return NormandyTestUtils.decorate(
      NormandyTestUtils.withMockPreferences(),
      async function inner({ mockPreferences, ...args }) {
        const serverUrl = `http://localhost:${server.identity.primaryPort}`;
        mockPreferences.set("app.normandy.api_url", `${serverUrl}/api/v1`);
        mockPreferences.set(
          "security.content.signature.root_hash",
          // Hash of the key that signs the normandy dev certificates
          "4C:35:B1:C3:E3:12:D9:55:E7:78:ED:D0:A7:E7:8A:38:83:04:EF:01:BF:FA:03:29:B2:46:9F:3C:C5:EC:36:04"
        );
        NormandyApi.clearIndexCache();

        try {
          await testFunction({ ...args, serverUrl, mockPreferences, server });
        } finally {
          await new Promise(resolve => server.stop(resolve));
        }
      }
    );
  };
}

function makeScriptServer(scriptPath) {
  const server = new HttpServer();
  server.registerContentType("sjs", "sjs");
  server.registerFile("/", do_get_file(scriptPath));
  server.start(-1);
  return server;
}

function withScriptServer(scriptPath) {
  return withServer(makeScriptServer(scriptPath));
}

function makeMockApiServer(directory) {
  const server = new HttpServer();
  server.registerDirectory("/", directory);

  server.setIndexHandler(async function(request, response) {
    response.processAsync();
    const dir = request.getProperty("directory");
    const index = dir.clone();
    index.append("index.json");

    if (!index.exists()) {
      response.setStatusLine("1.1", 404, "Not Found");
      response.write(`Cannot find path ${index.path}`);
      response.finish();
      return;
    }

    try {
      const contents = await OS.File.read(index.path, { encoding: "utf-8" });
      response.write(contents);
    } catch (e) {
      response.setStatusLine("1.1", 500, "Server error");
      response.write(e.toString());
    } finally {
      response.finish();
    }
  });

  server.start(-1);
  return server;
}

function withMockApiServer(apiName = "mock_api") {
  return withServer(makeMockApiServer(do_get_file(apiName)));
}

const CryptoUtils = {
  _getHashStringForCrypto(aCrypto) {
    // return the two-digit hexadecimal code for a byte
    let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

    // convert the binary hash data to a hex string.
    let binary = aCrypto.finish(false);
    let hash = Array.from(binary, c => toHexString(c.charCodeAt(0)));
    return hash.join("").toLowerCase();
  },

  /**
   * Get the computed hash for a given file
   * @param {nsIFile} file The file to be hashed
   * @param {string} [algorithm] The hashing algorithm to use
   */
  getFileHash(file, algorithm = "sha256") {
    const crypto = CryptoHash(algorithm);
    const fis = new FileInputStream(file, -1, -1, false);
    crypto.updateFromStream(fis, file.fileSize);
    const hash = this._getHashStringForCrypto(crypto);
    fis.close();
    return hash;
  },
};
