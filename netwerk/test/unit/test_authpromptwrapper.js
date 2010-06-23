// NOTE: This tests code outside of Necko. The test still lives here because
// the contract is part of Necko.

// TODO:
// - HTTPS
// - Proxies

const nsIAuthInformation = Components.interfaces.nsIAuthInformation;
const nsIAuthPromptAdapterFactory = Components.interfaces.nsIAuthPromptAdapterFactory;

function run_test() {
  const contractID = "@mozilla.org/network/authprompt-adapter-factory;1";
  if (!(contractID in Components.classes)) {
    print("No adapter factory found, skipping testing");
    return;
  }
  var adapter = Components.classes[contractID].getService();
  do_check_eq(adapter instanceof nsIAuthPromptAdapterFactory, true);

  // NOTE: xpconnect lets us get away with passing an empty object here
  // For this part of the test, we only care that this function returns
  // success
  do_check_neq(adapter.createAdapter({}), null);

  const host = "www.mozilla.org";

  var info = {
    username: "",
    password: "",
    domain: "",

    flags: nsIAuthInformation.AUTH_HOST,
    authenticationScheme: "basic",
    realm: "secretrealm"
  };

  const CALLED_PROMPT = 1 << 0;
  const CALLED_PROMPTUP = 1 << 1;
  const CALLED_PROMPTP = 1 << 2;
  function Prompt1() {}
  Prompt1.prototype = {
    called: 0,
    rv: true,

    user: "foo\\bar",
    pw: "bar",

    scheme: "http",

    QueryInterface: function authprompt_qi(iid) {
      if (iid.equals(Components.interfaces.nsISupports) ||
          iid.equals(Components.interfaces.nsIAuthPrompt))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    prompt: function ap1_prompt(title, text, realm, save, defaultText, result) {
      this.called |= CALLED_PROMPT;
      this.doChecks(text, realm);
      return this.rv;
    },

    promptUsernameAndPassword:
      function ap1_promptUP(title, text, realm, savePW, user, pw)
    {
      this.called |= CALLED_PROMPTUP;
      this.doChecks(text, realm);
      user.value = this.user;
      pw.value = this.pw;
      return this.rv;
    },

    promptPassword: function ap1_promptPW(title, text, realm, save, pwd) {
      this.called |= CALLED_PROMPTP;
      this.doChecks(text, realm);
      pwd.value = this.pw;
      return this.rv;
    },

    doChecks: function ap1_check(text, realm) {
      do_check_eq(this.scheme + "://" + host + " (" + info.realm + ")", realm);

      do_check_neq(text.indexOf(host), -1);
      if (info.flags & nsIAuthInformation.ONLY_PASSWORD) {
        // Should have the username in the text
        do_check_neq(text.indexOf(info.username), -1);
      } else {
        // Make sure that we show the realm if we have one and that we don't
        // show "" otherwise
        if (info.realm != "")
          do_check_neq(text.indexOf(info.realm), -1);
        else
          do_check_eq(text.indexOf('""'), -1);
        // No explicit port in the URL; message should not contain -1
        // for those cases
        do_check_eq(text.indexOf("-1"), -1);
      }
    }
  };


  // Also have to make up a channel
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel("http://" + host, "", null);

  function do_tests(expectedRV) {
    var prompt1;
    var wrapper;

    // 1: The simple case
    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    wrapper = adapter.createAdapter(prompt1);

    var rv = wrapper.promptAuth(chan, 0, info);
    do_check_eq(rv, prompt1.rv);
    do_check_eq(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      do_check_eq(info.domain, "");
      do_check_eq(info.username, prompt1.user);
      do_check_eq(info.password, prompt1.pw);
    }

    info.domain = "";
    info.username = "";
    info.password = "";

    // 2: Only ask for a PW
    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    info.flags |= nsIAuthInformation.ONLY_PASSWORD;

    // Initialize the username so that the prompt can show it
    info.username = prompt1.user;

    wrapper = adapter.createAdapter(prompt1);
    rv = wrapper.promptAuth(chan, 0, info);
    do_check_eq(rv, prompt1.rv);
    do_check_eq(prompt1.called, CALLED_PROMPTP);

    if (rv) {
      do_check_eq(info.domain, "");
      do_check_eq(info.username, prompt1.user); // we initialized this
      do_check_eq(info.password, prompt1.pw);
    }

    info.flags &= ~nsIAuthInformation.ONLY_PASSWORD;

    info.domain = "";
    info.username = "";
    info.password = "";

    // 3: user, pw and domain
    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    info.flags |= nsIAuthInformation.NEED_DOMAIN;

    wrapper = adapter.createAdapter(prompt1);
    rv = wrapper.promptAuth(chan, 0, info);
    do_check_eq(rv, prompt1.rv);
    do_check_eq(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      do_check_eq(info.domain, "foo");
      do_check_eq(info.username, "bar");
      do_check_eq(info.password, prompt1.pw);
    }

    info.flags &= ~nsIAuthInformation.NEED_DOMAIN;

    info.domain = "";
    info.username = "";
    info.password = "";

    // 4: username that doesn't contain a domain
    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    info.flags |= nsIAuthInformation.NEED_DOMAIN;

    prompt1.user = "foo";

    wrapper = adapter.createAdapter(prompt1);
    rv = wrapper.promptAuth(chan, 0, info);
    do_check_eq(rv, prompt1.rv);
    do_check_eq(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      do_check_eq(info.domain, "");
      do_check_eq(info.username, prompt1.user);
      do_check_eq(info.password, prompt1.pw);
    }

    info.flags &= ~nsIAuthInformation.NEED_DOMAIN;

    info.domain = "";
    info.username = "";
    info.password = "";

    // 5: FTP
    var ftpchan = ios.newChannel("ftp://" + host, "", null);

    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    prompt1.scheme = "ftp";

    wrapper = adapter.createAdapter(prompt1);
    var rv = wrapper.promptAuth(ftpchan, 0, info);
    do_check_eq(rv, prompt1.rv);
    do_check_eq(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      do_check_eq(info.domain, "");
      do_check_eq(info.username, prompt1.user);
      do_check_eq(info.password, prompt1.pw);
    }

    info.domain = "";
    info.username = "";
    info.password = "";
  }
  do_tests(true);
  do_tests(false);
}

