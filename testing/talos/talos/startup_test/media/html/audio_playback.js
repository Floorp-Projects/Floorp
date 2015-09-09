/*
 vim:se?t ts=2 sw=2 sts=2 et cindent:
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// audioPlayback Test plays an input file from a given
// <audio> and records the same to compute PESQ scores
var audioPlayback = function() {
  var test = this;
  var cleanupTimeout = 5000;
  var audio = document.createElement('audio');

  // start audio recorder
  initiateAudioRecording(test);
  if (test.failed) {
    test.finished = true;
    runNextTest();
    return;
  }

  audio.addEventListener('ended', function(evt) {
    // stop the recorder
    cleanupAudioRecording(test);
    if (!test.failed) {
      // Compute SNR and Delay between the reference
      // and the degreated files.
      getSNRAndDelay(test);
      // SNR_DELAY=5.4916,0
      // We update results as 2 separate results
      var res = JSON.parse(test.http_response);
      if(res["SNR-DELAY"]) {
        // fix the test.name
        var testName = test.name;
        var snr_delay = res["SNR-DELAY"].split(",");
        test.name = testName+"_snr_in_db";
        test.results = snr_delay[0];
        updateResults(test);
        test.name = testName+"_delay_in_ms";
        test.results = snr_delay[1];
        updateResults(test);
        // restore test.name
        test.name = testName;
      }
    }
    test.finished = true;
    runNextTest();
  });

  audio.addEventListener('error', function(evt) {
    // cleanup any started processes and run the next
    // test
    cleanupAudioRecording(test);
    test.finished = true;
    runNextTest();
  });

  audio.volume = 0.9;
  audio.src = test.src;
  audio.play();
};
