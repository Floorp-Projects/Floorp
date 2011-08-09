/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */

var phases = { "phase1": "profile1",
               "phase2": "profile2"};

/*
 * History data
 */

// the history data to add to the browser
var history1 = [
  { uri: "http://www.google.com/",
    title: "Google",
    visits: [
      { type: 1,
        date: 0
      },
      { type: 2,
        date: -1
      }
    ]
  },
  { uri: "http://www.cnn.com/",
    title: "CNN",
    visits: [
      { type: 1,
        date: -1
      },
      { type: 2,
        date: -36
      }
    ]
  }
];

// Another history data to add to the browser
var history2 = [
  { uri: "http://www.mozilla.com/",
    title: "Mozilla",
    visits: [
      { type: 1,
        date: 0
      },
      { type: 2,
        date: -36
      }
    ]
  },
  { uri: "http://www.google.com/language_tools?hl=en",
    title: "Language Tools",
    visits: [
      { type: 1, 
        date: 0
      },
      { type: 2, 
        date: -40
      }
    ]
  }
];

/*
 * Test phases
 */
Phase('phase1', [
  [History.add, history1],
  [Sync],
  [History.add, history2],
  [Sync, SYNC_WIPE_SERVER]
]);

Phase('phase2', [
  [Sync],
  [History.verify, history2]
]);

