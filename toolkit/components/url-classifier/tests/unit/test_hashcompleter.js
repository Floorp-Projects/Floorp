/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that the nsIUrlClassifierHashCompleter works as expected
// and simulates an HTTP server to provide completions.
//
// In order to test completions, each group of completions sent as one request
// to the HTTP server is called a completion set. There is currently not
// support for multiple requests being sent to the server at once, in this test.
// This tests makes a request for each element of |completionSets|, waits for
// a response and then moves to the next element.
// Each element of |completionSets| is an array of completions, and each
// completion is an object with the properties:
//   hash: complete hash for the completion. Automatically right-padded
//         to be COMPLETE_LENGTH.
//   expectCompletion: boolean indicating whether the server should respond
//                     with a full hash.
//   forceServerError: boolean indicating whether the server should respond
//                     with a 503.
//   table: name of the table that the hash corresponds to. Only needs to be set
//          if a completion is expected.
//   chunkId: positive integer corresponding to the chunk that the hash belongs
//            to. Only needs to be set if a completion is expected.
//   multipleCompletions: boolean indicating whether the server should respond
//                        with more than one full hash. If this is set to true
//                        then |expectCompletion| must also be set to true and
//                        |hash| must have the same prefix as all |completions|.
//   completions: an array of completions (objects with a hash, table and
//                chunkId property as described above). This property is only
//                used when |multipleCompletions| is set to true.

// Basic prefixes with 2/3 completions.
var basicCompletionSet = [
  {
    hash: "abcdefgh",
    expectCompletion: true,
    table: "test",
    chunkId: 1234,
  },
  {
    hash: "1234",
    expectCompletion: false,
  },
  {
    hash: "\u0000\u0000\u000012312",
    expectCompletion: true,
    table: "test",
    chunkId: 1234,
  }
];

// 3 prefixes with 0 completions to test HashCompleter handling a 204 status.
var falseCompletionSet = [
  {
    hash: "1234",
    expectCompletion: false,
  },
  {
    hash: "",
    expectCompletion: false,
  },
  {
    hash: "abc",
    expectCompletion: false,
  }
];

// The current implementation (as of Mar 2011) sometimes sends duplicate
// entries to HashCompleter and even expects responses for duplicated entries.
var dupedCompletionSet = [
  {
    hash: "1234",
    expectCompletion: true,
    table: "test",
    chunkId: 1,
  },
  {
    hash: "5678",
    expectCompletion: false,
    table: "test2",
    chunkId: 2,
  },
  {
    hash: "1234",
    expectCompletion: true,
    table: "test",
    chunkId: 1,
  },
  {
    hash: "5678",
    expectCompletion: false,
    table: "test2",
    chunkId: 2
  }
];

// It is possible for a hash completion request to return with multiple
// completions, the HashCompleter should return all of these.
var multipleResponsesCompletionSet = [
  {
    hash: "1234",
    expectCompletion: true,
    multipleCompletions: true,
    completions: [
      {
        hash: "123456",
        table: "test1",
        chunkId: 3,
      },
      {
        hash: "123478",
        table: "test2",
        chunkId: 4,
      }
    ],
  }
];

// The fifth completion set is added at runtime by getRandomCompletionSet.
// Each completion in the set only has one response and its purpose is to
// provide an easy way to test the HashCompleter handling an arbitrarily large
// completion set (determined by SIZE_OF_RANDOM_SET).
const SIZE_OF_RANDOM_SET = 16;
function getRandomCompletionSet(forceServerError) {
  let completionSet = [];
  let hashPrefixes = [];

  let seed = Math.floor(Math.random() * Math.pow(2, 32));
  dump("Using seed of " + seed + " for random completion set.\n");
  let rand = new LFSRgenerator(seed);

  for (let i = 0; i < SIZE_OF_RANDOM_SET; i++) {
    let completion = { expectCompletion: false, forceServerError: false, _finished: false };

    // Generate a random 256 bit hash. First we get a random number and then
    // convert it to a string.
    let hash;
    let prefix;
    do {
      hash = "";
      let length = 1 + rand.nextNum(5);
      for (let i = 0; i < length; i++)
        hash += String.fromCharCode(rand.nextNum(8));
      prefix = hash.substring(0,4);
    } while (hashPrefixes.indexOf(prefix) != -1);

    hashPrefixes.push(prefix);
    completion.hash = hash;

    if (!forceServerError) {
      completion.expectCompletion = rand.nextNum(1) == 1;
    } else {
      completion.forceServerError = true;
    }
    if (completion.expectCompletion) {
      // Generate a random alpha-numeric string of length at most 6 for the
      // table name.
      completion.table = (rand.nextNum(31)).toString(36);

      completion.chunkId = rand.nextNum(16);
    }
    completionSet.push(completion);
  }

  return completionSet;
}

var completionSets = [basicCompletionSet, falseCompletionSet,
                      dupedCompletionSet, multipleResponsesCompletionSet];
var currentCompletionSet = -1;
var finishedCompletions = 0;

const SERVER_PORT = 8080;
const SERVER_PATH = "/hash-completer";
var server;

// Completion hashes are automatically right-padded with null chars to have a
// length of COMPLETE_LENGTH.
// Taken from nsUrlClassifierDBService.h
const COMPLETE_LENGTH = 32;

var completer = Cc["@mozilla.org/url-classifier/hashcompleter;1"].
                  getService(Ci.nsIUrlClassifierHashCompleter);

var gethashUrl;

// Expected highest completion set for which the server sends a response.
var expectedMaxServerCompletionSet = 0;
var maxServerCompletionSet = 0;

function run_test() {
  // Generate a random completion set that return successful responses.
  completionSets.push(getRandomCompletionSet(false));
  // We backoff after receiving an error, so requests shouldn't reach the
  // server after that.
  expectedMaxServerCompletionSet = completionSets.length;
  // Generate some completion sets that return 503s.
  for (let j = 0; j < 10; ++j) {
    completionSets.push(getRandomCompletionSet(true));
  }

  // Fix up the completions before running the test.
  for (let completionSet of completionSets) {
    for (let completion of completionSet) {
      // Pad the right of each |hash| so that the length is COMPLETE_LENGTH.
      if (completion.multipleCompletions) {
        for (let responseCompletion of completion.completions) {
          let numChars = COMPLETE_LENGTH - responseCompletion.hash.length;
          responseCompletion.hash += (new Array(numChars + 1)).join("\u0000");
        }
      }
      else {
        let numChars = COMPLETE_LENGTH - completion.hash.length;
        completion.hash += (new Array(numChars + 1)).join("\u0000");
      }
    }
  }
  do_test_pending();

  server = new HttpServer();
  server.registerPathHandler(SERVER_PATH, hashCompleterServer);

  server.start(-1);
  const SERVER_PORT = server.identity.primaryPort;

  gethashUrl = "http://localhost:" + SERVER_PORT + SERVER_PATH;

  runNextCompletion();
}

function runNextCompletion() {
  // The server relies on currentCompletionSet to send the correct response, so
  // don't increment it until we start the new set of callbacks.
  currentCompletionSet++;
  if (currentCompletionSet >= completionSets.length) {
    finish();
    return;
  }

  dump("Now on completion set index " + currentCompletionSet + ", length " +
       completionSets[currentCompletionSet].length + "\n");
  // Number of finished completions for this set.
  finishedCompletions = 0;
  for (let completion of completionSets[currentCompletionSet]) {
    completer.complete(completion.hash.substring(0,4), gethashUrl,
                       (new callback(completion)));
  }
}

function hashCompleterServer(aRequest, aResponse) {
  let stream = aRequest.bodyInputStream;
  let wrapperStream = Cc["@mozilla.org/binaryinputstream;1"].
                        createInstance(Ci.nsIBinaryInputStream);
  wrapperStream.setInputStream(stream);

  let len = stream.available();
  let data = wrapperStream.readBytes(len);

  // To avoid a response with duplicate hash completions, we keep track of all
  // completed hash prefixes so far.
  let completedHashes = [];
  let responseText = "";

  function responseForCompletion(x) {
    return x.table + ":" + x.chunkId + ":" + x.hash.length + "\n" + x.hash;
  }
  // As per the spec, a server should response with a 204 if there are no
  // full-length hashes that match the prefixes.
  let httpStatus = 204;
  for (let completion of completionSets[currentCompletionSet]) {
    if (completion.expectCompletion &&
        (completedHashes.indexOf(completion.hash) == -1)) {
      completedHashes.push(completion.hash);

      if (completion.multipleCompletions)
        responseText += completion.completions.map(responseForCompletion).join("");
      else
        responseText += responseForCompletion(completion);
    }
    if (completion.forceServerError) {
      httpStatus = 503;
    }
  }

  dump("Server sending response for " + currentCompletionSet + "\n");
  maxServerCompletionSet = currentCompletionSet;
  if (responseText && httpStatus != 503) {
    aResponse.write(responseText);
  } else {
    aResponse.setStatusLine(null, httpStatus, null);
  }
}


function callback(completion) {
  this._completion = completion;
}

callback.prototype = {
  completion: function completion(hash, table, chunkId, trusted) {
    do_check_true(this._completion.expectCompletion);
    if (this._completion.multipleCompletions) {
      for (let completion of this._completion.completions) {
        if (completion.hash == hash) {
          do_check_eq(JSON.stringify(hash), JSON.stringify(completion.hash));
          do_check_eq(table, completion.table);
          do_check_eq(chunkId, completion.chunkId);

          completion._completed = true;

          if (this._completion.completions.every(x => x._completed))
            this._completed = true;

          break;
        }
      }
    }
    else {
      // Hashes are not actually strings and can contain arbitrary data.
      do_check_eq(JSON.stringify(hash), JSON.stringify(this._completion.hash));
      do_check_eq(table, this._completion.table);
      do_check_eq(chunkId, this._completion.chunkId);

      this._completed = true;
    }
  },

  completionFinished: function completionFinished(status) {
    finishedCompletions++;
    do_check_eq(!!this._completion.expectCompletion, !!this._completed);
    this._completion._finished = true;

    // currentCompletionSet can mutate before all of the callbacks are complete.
    if (currentCompletionSet < completionSets.length &&
        finishedCompletions == completionSets[currentCompletionSet].length) {
      runNextCompletion();
    }
  },
};

function finish() {
  do_check_eq(expectedMaxServerCompletionSet, maxServerCompletionSet);
  server.stop(function() {
    do_test_finished();
  });
}
