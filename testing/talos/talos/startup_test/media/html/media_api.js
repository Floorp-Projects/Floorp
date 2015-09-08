/*
 vim:se?t ts=2 sw=2 sts=2 et cindent:
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// This file hosts the logic for performing Media Operations
// on our test server and handling responses.
// Media Operations are in the form of GET requests/

// Global call back for all the HTTP Requests.
// Since we use synchronous Ajax requests, we should
// be good with this design.
var httpRequestCb = function(test, req_status, response) {
  // update the test of its http request status
  if (req_status != 200) {
    test.failed = true;
  }
  test.http_response = response;
  log(test.http_response);
}

// Perform the GET Request, invoke appropriate call backs
var sendGetRequest = function(test, async) {
  var request = new XMLHttpRequest();
  request.open('GET', test.url, async);
  request.onreadystatechange = function() {
    if (request.readyState == 4) {
      httpRequestCb(test,request.status, request.response);
    }
  };
  request.send();
};

// Stop our server and exit the browser
var cleanupTests = function() {
  var url = baseUrl + '/server/config/stop'
  test.url = url;
  sendGetRequest(test, true);
};


// Perform audio/recorder/start command
var initiateAudioRecording = function(test) {
  var url = baseUrl + '/audio/recorder/start/?timeout=' + test.timeout;
  test.url = url;
  test.failed = false;
  sendGetRequest(test, false);
};


// Perform audio/recorder/stop command
var cleanupAudioRecording = function(test) {
  var url = baseUrl + '/audio/recorder/stop';
  test.url = url;
  test.failed = false;
  sendGetRequest(test, false);
};

// Perform audio/snr/compute command
var getSNRAndDelay = function(test) {
  var url = baseUrl + '/audio/snr/compute';
  test.url = url;
  test.failed = false;
  sendGetRequest(test, false);
};
