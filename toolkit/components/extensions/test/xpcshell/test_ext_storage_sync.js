/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {keyToId, idToKey} = Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");

add_task(function* test_key_to_id() {
  equal(keyToId("foo"), "key-foo");
  equal(keyToId("my-new-key"), "key-my_2D_new_2D_key");
  equal(keyToId(""), "key-");
  equal(keyToId("™"), "key-_2122_");
  equal(keyToId("\b"), "key-_8_");
  equal(keyToId("abc\ndef"), "key-abc_A_def");
  equal(keyToId("Kinto's fancy_string"), "key-Kinto_27_s_20_fancy_5F_string");

  const KEYS = ["foo", "my-new-key", "", "Kinto's fancy_string", "™", "\b"];
  for (let key of KEYS) {
    equal(idToKey(keyToId(key)), key);
  }

  equal(idToKey("hi"), null);
  equal(idToKey("-key-hi"), null);
  equal(idToKey("key--abcd"), null);
  equal(idToKey("key-%"), null);
  equal(idToKey("key-_HI"), null);
  equal(idToKey("key-_HI_"), null);
  equal(idToKey("key-"), "");
  equal(idToKey("key-1"), "1");
  equal(idToKey("key-_2D_"), "-");
});
