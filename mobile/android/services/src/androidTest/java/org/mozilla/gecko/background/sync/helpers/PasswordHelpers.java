/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;

public class PasswordHelpers {

  public static PasswordRecord createPassword1() {
    PasswordRecord rec = new PasswordRecord();
    rec.encType = "some type";
    rec.formSubmitURL = "http://submit.html";
    rec.hostname = "http://hostname";
    rec.httpRealm = "httpRealm";
    rec.encryptedPassword ="12345";
    rec.passwordField = "box.pass.field";
    rec.timeCreated = 111111111L;
    rec.timeLastUsed = 123412352435L;
    rec.timePasswordChanged = 121111111L;
    rec.timesUsed = 5L;
    rec.encryptedUsername = "jvoll";
    rec.usernameField = "box.user.field";
    return rec;
  }

  public static PasswordRecord createPassword2() {
    PasswordRecord rec = new PasswordRecord();
    rec.encType = "some type";
    rec.formSubmitURL = "http://submit2.html";
    rec.hostname = "http://hostname2";
    rec.httpRealm = "httpRealm2";
    rec.encryptedPassword ="54321";
    rec.passwordField = "box.pass.field2";
    rec.timeCreated = 12111111111L;
    rec.timeLastUsed = 123412352213L;
    rec.timePasswordChanged = 123111111111L;
    rec.timesUsed = 2L;
    rec.encryptedUsername = "rnewman";
    rec.usernameField = "box.user.field2";
    return rec;
  }

  public static PasswordRecord createPassword3() {
    PasswordRecord rec = new PasswordRecord();
    rec.encType = "some type3";
    rec.formSubmitURL = "http://submit3.html";
    rec.hostname = "http://hostname3";
    rec.httpRealm = "httpRealm3";
    rec.encryptedPassword ="54321";
    rec.passwordField = "box.pass.field3";
    rec.timeCreated = 100000000000L;
    rec.timeLastUsed = 123412352213L;
    rec.timePasswordChanged = 110000000000L;
    rec.timesUsed = 2L;
    rec.encryptedUsername = "rnewman";
    rec.usernameField = "box.user.field3";
    return rec;
  }

  public static PasswordRecord createPassword4() {
    PasswordRecord rec = new PasswordRecord();
    rec.encType = "some type";
    rec.formSubmitURL = "http://submit4.html";
    rec.hostname = "http://hostname4";
    rec.httpRealm = "httpRealm4";
    rec.encryptedPassword ="54324";
    rec.passwordField = "box.pass.field4";
    rec.timeCreated = 101000000000L;
    rec.timeLastUsed = 123412354444L;
    rec.timePasswordChanged = 110000000000L;
    rec.timesUsed = 4L;
    rec.encryptedUsername = "rnewman4";
    rec.usernameField = "box.user.field4";
    return rec;
  }

  public static PasswordRecord createPassword5() {
    PasswordRecord rec = new PasswordRecord();
    rec.encType = "some type5";
    rec.formSubmitURL = "http://submit5.html";
    rec.hostname = "http://hostname5";
    rec.httpRealm = "httpRealm5";
    rec.encryptedPassword ="54325";
    rec.passwordField = "box.pass.field5";
    rec.timeCreated = 101000000000L;
    rec.timeLastUsed = 123412352555L;
    rec.timePasswordChanged = 111111111111L;
    rec.timesUsed = 5L;
    rec.encryptedUsername = "jvoll5";
    rec.usernameField = "box.user.field5";
    return rec;
  }
}
