// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/ctypes.jsm");
Components.utils.import("resource://gre/modules/JNI.jsm");

add_task(function test_JNI() {
  let iconSize = -1;

  let jni = null;
  try {
    jni = new JNI();
    let cls = jni.findClass("org/mozilla/gecko/GeckoAppShell");
    let method = jni.getStaticMethodID(cls, "getPreferredIconSize", "()I");
    iconSize = jni.callStaticIntMethod(cls, method);
  } finally {
    if (jni != null) {
      jni.close();
    }
  }

  do_check_neq(iconSize, -1);
});

run_next_test();
