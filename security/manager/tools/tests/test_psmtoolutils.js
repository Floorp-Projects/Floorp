// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests PSMToolUtils.jsm.

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
const { PSMToolUtils } =
  Cu.import("resource://testing-common/psm/PSMToolUtils.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

const ResponseType = {
  BadStatusCode: { contents: "", statusCode: 404 },
  Base64: { contents: "eyJmb28iOiJiYXIifQ==" }, // btoa('{"foo":"bar"}')
  NotBase64: { contents: "not*base 64." },
};

var gHttpServer = new HttpServer();
var gHttpServerPrePath;

add_task(function testDownloadFile_Setup() {
  gHttpServer = new HttpServer();
  gHttpServer.registerPrefixHandler("/", function(request, response) {
    let pathWithoutLeadingSlash = request.path.substring(1);
    let responseType = ResponseType[pathWithoutLeadingSlash];
    ok(responseType,
       `'${pathWithoutLeadingSlash}' should be a valid ResponseType`);

    response.setStatusLine(request.httpVersion, responseType.statusCode || 200,
                           null);
    response.setHeader("Content-Type", "text/plain");
    response.write(responseType.contents);
  });
  gHttpServer.start(-1);
  do_register_cleanup(() => {
    gHttpServer.stop(() => {});
  });
  gHttpServerPrePath = `http://localhost:${gHttpServer.identity.primaryPort}`;
});

add_task(function testDownloadFile_BadStatusCode() {
  let url = `${gHttpServerPrePath}/BadStatusCode`;
  throws(() => PSMToolUtils.downloadFile(url, true),
         /problem downloading.*status 404/,
         "Exception should be thrown for a non-200 status code");
});

add_task(function testDownloadFile_DecodeAsBase64_Base64() {
  let url = `${gHttpServerPrePath}/Base64`;
  let contents = PSMToolUtils.downloadFile(url, true);
  equal(contents, '{"foo":"bar"}', "Should get back decoded Base64 contents");
});

add_task(function testDownloadFile_DecodeAsBase64_NotBase64() {
  let url = `${gHttpServerPrePath}/NotBase64`;
  throws(() => PSMToolUtils.downloadFile(url, true),
         /could not decode data as base64/,
         "Exception should be thrown if we get non-Base64 content when we " +
         "were expecting otherwise");
});

add_task(function testDownloadFile_DontDecodeAsBase64_Base64() {
  let url = `${gHttpServerPrePath}/Base64`;
  let contents = PSMToolUtils.downloadFile(url, false);
  equal(contents, ResponseType.Base64.contents,
        "Should get back undecoded Base64 contents");
});

add_task(function testDownloadFile_DontDecodeAsBase64_NotBase64() {
  let url = `${gHttpServerPrePath}/NotBase64`;
  let contents = PSMToolUtils.downloadFile(url, false);
  equal(contents, ResponseType.NotBase64.contents,
        "Should get back expected contents");
});

add_task(function testDownloadFile_InvalidDomain() {
  let domain = "test-psmtoolutils.js.nosuchdomain.test";
  Services.prefs.setCharPref("network.dns.localDomains", domain);
  throws(() => PSMToolUtils.downloadFile(`http://${domain}/foo`, true),
         /problem downloading/,
         "Exception should be thrown if trying to download from an invalid " +
         "domain");
});

add_task(function testStripComments() {
  const tests = [
    {input: "", expected: ""},
    {input: "{}", expected: "{}"},
    {input: "     //\n", expected: ""},
    {input: "\t//\n//\n   //\n", expected: ""},
    {input: '{"foo":"bar"}', expected: '{"foo":"bar"}'},
    // From Bug 1197607.
    {input: '"report_uri": "http://clients3.google.com/cert_upload_json"',
     expected: '"report_uri": "http://clients3.google.com/cert_upload_json"'},
    {input: '//abc\n{"foo":"bar"}', expected: '{"foo":"bar"}'},
    {input: '{"foo":"bar",\n//baz\n"aaa": "bbb"}',
     expected: '{"foo":"bar",\n"aaa": "bbb"}'},
    {input: "// foo http://example.com/baz\n", expected: ""},
  ];
  for (let test of tests) {
    let result = PSMToolUtils.stripComments(test.input);
    equal(test.expected, result,
          `Expected and actual result should match for input '${test.input}'`);
  }
});
