/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

class ObliviousHttpTestRequest {
  constructor(method, uri, headers, content) {
    this.method = method;
    this.uri = uri;
    this.headers = headers;
    this.content = content;
  }
}

class ObliviousHttpTestResponse {
  constructor(status, headers, content) {
    this.status = status;
    this.headers = headers;
    this.content = content;
  }
}

class ObliviousHttpTestCase {
  constructor(request, response) {
    this.request = request;
    this.response = response;
  }
}

add_task(async function test_oblivious_http() {
  let testcases = [
    new ObliviousHttpTestCase(
      new ObliviousHttpTestRequest(
        "GET",
        NetUtil.newURI("https://example.com"),
        { "X-Some-Header": "header value" },
        ""
      ),
      new ObliviousHttpTestResponse(200, {}, "Hello, World!")
    ),
    new ObliviousHttpTestCase(
      new ObliviousHttpTestRequest(
        "POST",
        NetUtil.newURI("http://example.test"),
        { "X-Some-Header": "header value", "X-Some-Other-Header": "25" },
        "Posting some content..."
      ),
      new ObliviousHttpTestResponse(
        418,
        { "X-Teapot": "teapot" },
        "I'm a teapot"
      )
    ),
    new ObliviousHttpTestCase(
      new ObliviousHttpTestRequest(
        "GET",
        NetUtil.newURI("http://example.test/404"),
        { "X-Some-Header": "header value", "X-Some-Other-Header": "25" },
        ""
      ),
      undefined // 404 relay
    ),
  ];

  for (let testcase of testcases) {
    await run_one_testcase(testcase);
  }
});

async function run_one_testcase(testcase) {
  let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
    Ci.nsIObliviousHttp
  );
  let ohttpServer = ohttp.server();

  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/", function (request, response) {
    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
      Ci.nsIScriptableInputStream
    );
    inputStream.init(request.bodyInputStream);
    let requestBody = inputStream.readBytes(inputStream.available());
    let ohttpResponse = ohttpServer.decapsulate(stringToBytes(requestBody));
    let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
      Ci.nsIBinaryHttp
    );
    let decodedRequest = bhttp.decodeRequest(ohttpResponse.request);
    equal(decodedRequest.method, testcase.request.method);
    equal(decodedRequest.scheme, testcase.request.uri.scheme);
    equal(decodedRequest.authority, testcase.request.uri.hostPort);
    equal(decodedRequest.path, testcase.request.uri.pathQueryRef);
    for (
      let i = 0;
      i < decodedRequest.headerNames.length &&
      i < decodedRequest.headerValues.length;
      i++
    ) {
      equal(
        decodedRequest.headerValues[i],
        testcase.request.headers[decodedRequest.headerNames[i]]
      );
    }
    equal(bytesToString(decodedRequest.content), testcase.request.content);

    let responseHeaderNames = ["content-type"];
    let responseHeaderValues = ["text/plain"];
    for (let headerName of Object.keys(testcase.response.headers)) {
      responseHeaderNames.push(headerName);
      responseHeaderValues.push(testcase.response.headers[headerName]);
    }
    let binaryResponse = new BinaryHttpResponse(
      testcase.response.status,
      responseHeaderNames,
      responseHeaderValues,
      stringToBytes(testcase.response.content)
    );
    let responseBytes = bhttp.encodeResponse(binaryResponse);
    let encResponse = ohttpResponse.encapsulate(responseBytes);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "message/ohttp-res", false);
    response.write(bytesToString(encResponse));
  });
  httpServer.start(-1);

  let ohttpService = Cc[
    "@mozilla.org/network/oblivious-http-service;1"
  ].getService(Ci.nsIObliviousHttpService);
  let relayURI = NetUtil.newURI(
    `http://localhost:${httpServer.identity.primaryPort}/`
  );
  if (!testcase.response) {
    relayURI = NetUtil.newURI(
      `http://localhost:${httpServer.identity.primaryPort}/404`
    );
  }
  let obliviousHttpChannel = ohttpService
    .newChannel(relayURI, testcase.request.uri, ohttpServer.encodedConfig)
    .QueryInterface(Ci.nsIHttpChannel);
  for (let headerName of Object.keys(testcase.request.headers)) {
    obliviousHttpChannel.setRequestHeader(
      headerName,
      testcase.request.headers[headerName],
      false
    );
  }
  if (testcase.request.method == "POST") {
    let uploadChannel = obliviousHttpChannel.QueryInterface(
      Ci.nsIUploadChannel2
    );
    ok(uploadChannel);
    let bodyStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsIStringInputStream
    );
    bodyStream.setData(
      testcase.request.content,
      testcase.request.content.length
    );
    uploadChannel.explicitSetUploadStream(
      bodyStream,
      null,
      -1,
      testcase.request.method,
      false
    );
  }
  let response = await new Promise((resolve, reject) => {
    NetUtil.asyncFetch(obliviousHttpChannel, function (inputStream, result) {
      let scriptableInputStream = Cc[
        "@mozilla.org/scriptableinputstream;1"
      ].createInstance(Ci.nsIScriptableInputStream);
      scriptableInputStream.init(inputStream);
      try {
        // If decoding failed just return undefined.
        inputStream.available();
      } catch (e) {
        resolve(undefined);
        return;
      }
      let responseBody = scriptableInputStream.readBytes(
        inputStream.available()
      );
      resolve(responseBody);
    });
  });
  if (testcase.response) {
    equal(response, testcase.response.content);
    for (let headerName of Object.keys(testcase.response.headers)) {
      equal(
        obliviousHttpChannel.getResponseHeader(headerName),
        testcase.response.headers[headerName]
      );
    }
  } else {
    let relayChannel = obliviousHttpChannel.QueryInterface(
      Ci.nsIObliviousHttpChannel
    ).relayChannel;
    equal(relayChannel.responseStatus, 404);
  }
  await new Promise((resolve, reject) => {
    httpServer.stop(resolve);
  });
}
