const { LoginAutoCompleteResult } = ChromeUtils.import(
  "resource://gre/modules/LoginAutoCompleteResult.jsm"
);
let nsLoginInfo = Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const PREF_INSECURE_FIELD_WARNING_ENABLED =
  "security.insecure_field_warning.contextual.enabled";

let matchingLogins = [];
matchingLogins.push(
  new nsLoginInfo(
    "http://mochi.test:8888",
    "http://autocomplete:8888",
    null,
    "",
    "emptypass1",
    "uname",
    "pword"
  )
);

matchingLogins.push(
  new nsLoginInfo(
    "http://mochi.test:8888",
    "http://autocomplete:8888",
    null,
    "tempuser1",
    "temppass1",
    "uname",
    "pword"
  )
);

matchingLogins.push(
  new nsLoginInfo(
    "http://mochi.test:8888",
    "http://autocomplete:8888",
    null,
    "testuser2",
    "testpass2",
    "uname",
    "pword"
  )
);
// subdomain:
matchingLogins.push(
  new nsLoginInfo(
    "http://sub.mochi.test:8888",
    "http://autocomplete:8888",
    null,
    "testuser3",
    "testpass3",
    "uname",
    "pword"
  )
);

matchingLogins.push(
  new nsLoginInfo(
    "http://mochi.test:8888",
    "http://autocomplete:8888",
    null,
    "zzzuser4",
    "zzzpass4",
    "uname",
    "pword"
  )
);

add_task(async function setup() {
  // Get a profile so we have storage access and insert the logins to get unique GUIDs.
  do_get_profile();
  matchingLogins = await Services.logins.addLogins(matchingLogins);
});

add_task(async function test_all_patterns() {
  let meta = matchingLogins[0].QueryInterface(Ci.nsILoginMetaInfo);
  let dateAndTimeFormatter = new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
  });
  let time = dateAndTimeFormatter.format(new Date(meta.timePasswordChanged));
  const LABEL_NO_USERNAME = "No username (" + time + ")";

  let expectedResults = [
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: false,
      isPasswordField: false,
      matchingLogins: [],
      items: [
        {
          value: "",
          label:
            "This connection is not secure. Logins entered here could be compromised. Learn More",
          style: "insecureWarning",
          comment: "",
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: false,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label:
            "This connection is not secure. Logins entered here could be compromised. Learn More",
          style: "insecureWarning",
          comment: "",
        },
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: false,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "",
          label:
            "This connection is not secure. Logins entered here could be compromised. Learn More",
          style: "insecureWarning",
          comment: "",
        },
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: true,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: true,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: false,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label:
            "This connection is not secure. Logins entered here could be compromised. Learn More",
          style: "insecureWarning",
          comment: "",
        },
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: false,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "",
          label:
            "This connection is not secure. Logins entered here could be compromised. Learn More",
          style: "insecureWarning",
          comment: "",
        },
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: true,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: false,
      matchingLogins: [],
      items: [
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: false,
      matchingLogins: [],
      searchString: "foo",
      items: [
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: true,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: false,
      isSecure: false,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins: [],
      items: [
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins: [],
      searchString: "foo",
      items: [],
    },
    {
      generatedPassword: "9ljgfd4shyktb45",
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins: [],
      items: [
        {
          value: "9ljgfd4shyktb45",
          label: "Use a Securely Generated Password",
          style: "generatedPassword",
          comment: "9ljgfd4shyktb45",
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      generatedPassword: "9ljgfd4shyktb45",
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins: [],
      searchString: "9ljgfd4shyktb45",
      items: [],
    },
    {
      description: "secure username field on sub.mochi.test",
      formHostPort: "sub.mochi.test:8888",
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: false,
      matchingLogins,
      items: [
        {
          value: "testuser3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "tempuser1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testuser2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzuser4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
    {
      description: "secure password field on sub.mochi.test",
      formHostPort: "sub.mochi.test:8888",
      insecureFieldWarningEnabled: true,
      isSecure: true,
      isPasswordField: true,
      matchingLogins,
      items: [
        {
          value: "testpass3",
          label: "testuser3",
          style: "loginWithOrigin",
          comment: { comment: "sub.mochi.test:8888" },
        },
        {
          value: "emptypass1",
          label: LABEL_NO_USERNAME,
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "temppass1",
          label: "tempuser1",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "testpass2",
          label: "testuser2",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "zzzpass4",
          label: "zzzuser4",
          style: "loginWithOrigin",
          comment: { comment: "mochi.test:8888" },
        },
        {
          value: "",
          label: "View Saved Logins",
          style: "loginsFooter",
          comment: "mochi.test",
        },
      ],
    },
  ];

  LoginHelper.createLogger("LoginAutoCompleteResult");
  Services.prefs.setBoolPref("signon.showAutoCompleteFooter", true);
  Services.prefs.setBoolPref("signon.showAutoCompleteOrigins", true);

  expectedResults.forEach(pattern => {
    info(JSON.stringify(pattern, null, 2));
    Services.prefs.setBoolPref(
      PREF_INSECURE_FIELD_WARNING_ENABLED,
      pattern.insecureFieldWarningEnabled
    );
    let actual = new LoginAutoCompleteResult(
      pattern.searchString || "",
      pattern.matchingLogins,
      pattern.formHostPort || "mochi.test:8888",
      {
        hostname: "mochi.test",
        generatedPassword: pattern.generatedPassword,
        isSecure: pattern.isSecure,
        isPasswordField: pattern.isPasswordField,
      }
    );
    equal(actual.matchCount, pattern.items.length, "Check matching row count");
    pattern.items.forEach((item, index) => {
      equal(actual.getValueAt(index), item.value, `Value ${index}`);
      equal(actual.getLabelAt(index), item.label, `Label ${index}`);
      equal(actual.getStyleAt(index), item.style, `Style ${index}`);
      let actualComment = actual.getCommentAt(index);
      if (typeof item.comment == "object") {
        let parsedComment = JSON.parse(actualComment);
        equal(
          parsedComment.comment,
          item.comment.comment,
          `Comment.comment ${index}`
        );
      } else {
        equal(actualComment, item.comment, `Comment ${index}`);
      }
    });

    if (pattern.items.length != 0) {
      Assert.throws(
        () => actual.getValueAt(pattern.items.length),
        /Index out of range\./
      );

      Assert.throws(
        () => actual.getLabelAt(pattern.items.length),
        /Index out of range\./
      );

      Assert.throws(
        () => actual.removeValueAt(pattern.items.length, true),
        /Index out of range\./
      );
    }
  });
});
