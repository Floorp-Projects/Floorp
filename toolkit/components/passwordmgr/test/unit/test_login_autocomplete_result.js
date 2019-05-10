const {LoginAutoCompleteResult} = ChromeUtils.import("resource://gre/modules/LoginAutoCompleteResult.jsm");
var nsLoginInfo = Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                         Ci.nsILoginInfo, "init");

const PREF_INSECURE_FIELD_WARNING_ENABLED = "security.insecure_field_warning.contextual.enabled";
const PREF_INSECURE_AUTOFILLFORMS_ENABLED = "signon.autofillForms.http";

let matchingLogins = [];
matchingLogins.push(new nsLoginInfo("http://mochi.test:8888", "http://autocomplete:8888", null,
                                    "", "emptypass1", "uname", "pword"));

matchingLogins.push(new nsLoginInfo("http://mochi.test:8888", "http://autocomplete:8888", null,
                                    "tempuser1", "temppass1", "uname", "pword"));

matchingLogins.push(new nsLoginInfo("http://mochi.test:8888", "http://autocomplete:8888", null,
                                    "testuser2", "testpass2", "uname", "pword"));

matchingLogins.push(new nsLoginInfo("http://mochi.test:8888", "http://autocomplete:8888", null,
                                    "testuser3", "testpass3", "uname", "pword"));

matchingLogins.push(new nsLoginInfo("http://mochi.test:8888", "http://autocomplete:8888", null,
                                    "zzzuser4", "zzzpass4", "uname", "pword"));

let meta = matchingLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
let dateAndTimeFormatter = new Services.intl.DateTimeFormat(undefined,
                                                            { dateStyle: "medium" });
let time = dateAndTimeFormatter.format(new Date(meta.timePasswordChanged));
const LABEL_NO_USERNAME = "No username (" + time + ")";

let expectedResults = [
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: true,
    isSecure: true,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: true,
    isSecure: false,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: "This connection is not secure. Logins entered here could be compromised. Learn More",
      style: "insecureWarning",
    }, {
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: true,
    isSecure: true,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: true,
    isSecure: false,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "",
      label: "This connection is not secure. Logins entered here could be compromised. Learn More",
      style: "insecureWarning",
    }, {
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: true,
    isSecure: true,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: true,
    isSecure: false,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: true,
    isSecure: true,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: true,
    isSecure: false,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: false,
    isSecure: true,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: false,
    isSecure: false,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: "This connection is not secure. Logins entered here could be compromised. Learn More",
      style: "insecureWarning",
    }, {
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: false,
    isSecure: true,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: true,
    insecureAutoFillFormsEnabled: false,
    isSecure: false,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "",
      label: "This connection is not secure. Logins entered here could be compromised. Learn More",
      style: "insecureWarning",
    }, {
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: false,
    isSecure: true,
    isPasswordField: false,
    matchingLogins,
    items: [{
      value: "",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "tempuser1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testuser2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testuser3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzuser4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: false,
    isSecure: false,
    isPasswordField: false,
    matchingLogins,
    items: [],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: false,
    isSecure: true,
    isPasswordField: true,
    matchingLogins,
    items: [{
      value: "emptypass1",
      label: LABEL_NO_USERNAME,
      style: "loginWithOrigin",
    }, {
      value: "temppass1",
      label: "tempuser1",
      style: "loginWithOrigin",
    }, {
      value: "testpass2",
      label: "testuser2",
      style: "loginWithOrigin",
    }, {
      value: "testpass3",
      label: "testuser3",
      style: "loginWithOrigin",
    }, {
      value: "zzzpass4",
      label: "zzzuser4",
      style: "loginWithOrigin",
    }],
  },
  {
    insecureFieldWarningEnabled: false,
    insecureAutoFillFormsEnabled: false,
    isSecure: false,
    isPasswordField: true,
    matchingLogins,
    items: [],
  },
];

add_task(async function test_all_patterns() {
  LoginHelper.createLogger("LoginAutoCompleteResult");
  Services.prefs.setBoolPref("signon.showAutoCompleteOrigins", true);

  expectedResults.forEach(pattern => {
    Services.prefs.setBoolPref(PREF_INSECURE_FIELD_WARNING_ENABLED,
                               pattern.insecureFieldWarningEnabled);
    Services.prefs.setBoolPref(PREF_INSECURE_AUTOFILLFORMS_ENABLED,
                               pattern.insecureAutoFillFormsEnabled);
    let actual = new LoginAutoCompleteResult("", pattern.matchingLogins, {
      isSecure: pattern.isSecure,
      isPasswordField: pattern.isPasswordField,
    });
    pattern.items.forEach((item, index) => {
      equal(actual.getValueAt(index), item.value);
      equal(actual.getLabelAt(index), item.label);
      equal(actual.getStyleAt(index), item.style);
    });

    if (pattern.items.length != 0) {
      Assert.throws(() => actual.getValueAt(pattern.items.length),
                    /Index out of range\./);

      Assert.throws(() => actual.getLabelAt(pattern.items.length),
                    /Index out of range\./);

      Assert.throws(() => actual.removeValueAt(pattern.items.length, true),
                    /Index out of range\./);
    }
  });
});
