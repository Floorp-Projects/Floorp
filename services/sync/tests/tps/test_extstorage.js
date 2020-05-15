/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["addons"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = {
  phase1: "profile1",
  phase2: "profile2",
  phase3: "profile1",
  phase4: "profile2",
  phase5: "profile1",
};

/*
 * Test phases
 */

Phase("phase1", [
  [
    ExtStorage.set,
    "ext-1",
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": {
        sk_1: "v1",
        sk_2: "v2",
      },
    },
  ],
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": {
        sk_1: "v1",
        sk_2: "v2",
      },
    },
  ],
  [Sync],
]);

Phase("phase2", [
  [Sync],
  [
    ExtStorage.set,
    "ext-1",
    {
      "key-2": "value from profile 2",
    },
  ],
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": "value from profile 2",
    },
  ],
  [Sync],
]);

Phase("phase3", [
  [Sync],
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": "value from profile 2",
    },
  ],
  [
    ExtStorage.set,
    "ext-1",
    {
      "key-2": "value from profile 1",
    },
  ],
  // exit without syncing.
]);

Phase("phase4", [
  [Sync],
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": "value from profile 2",
    },
  ],
  [
    ExtStorage.set,
    "ext-1",
    {
      "key-2": "second value from profile 2",
    },
  ],
  [Sync],
]);

Phase("phase5", [
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": "value from profile 1",
    },
  ],
  [Sync],
  [
    ExtStorage.verify,
    "ext-1",
    null,
    {
      "key-1": {
        sub_key_1: "value 1",
        sub_key_2: "value 2",
      },
      "key-2": "second value from profile 2",
    },
  ],
]);
