// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/ctypes.jsm");
Components.utils.import("resource://gre/modules/JNI.jsm");

add_task(function test_JNI() {
  var jenv = null;
  try {
    jenv = JNI.GetForThread();

    // Test a simple static method.
    var geckoAppShell = JNI.LoadClass(jenv, "org.mozilla.gecko.GeckoAppShell", {
      static_methods: [
        { name: "getPreferredIconSize", sig: "()I" },
        { name: "getContext", sig: "()Landroid/content/Context;" },
      ],
    });

    let iconSize = -1;
    iconSize = geckoAppShell.getPreferredIconSize();
    do_check_neq(iconSize, -1);

    // Test GeckoNetworkManager methods that are accessed by PaymentsUI.js.
    // The return values can vary, so we can't test for equivalence, but we
    // can ensure that the method calls return values of the correct type.
    let jGeckoNetworkManager = JNI.LoadClass(jenv, "org/mozilla/gecko/GeckoNetworkManager", {
      static_methods: [
        { name: "getMNC", sig: "()I" },
        { name: "getMCC", sig: "()I" },
      ],
    });
    do_check_eq(typeof jGeckoNetworkManager.getMNC(), "number");
    do_check_eq(typeof jGeckoNetworkManager.getMCC(), "number");

    // Test retrieving the context's class's name, which tests dynamic method
    // invocation as well as converting a Java string to JavaScript.
    JNI.LoadClass(jenv, "android.content.Context", {
      methods: [
        { name: "getClass", sig: "()Ljava/lang/Class;" },
      ],
    });
    JNI.LoadClass(jenv, "java.lang.Class", {
      methods: [
        { name: "getName", sig: "()Ljava/lang/String;" },
      ],
    });
    do_check_eq("org.mozilla.gecko.BrowserApp", JNI.ReadString(jenv, geckoAppShell.getContext().getClass().getName()));
  } finally {
    if (jenv) {
      JNI.UnloadClasses(jenv);
    }
  }
});

run_next_test();
