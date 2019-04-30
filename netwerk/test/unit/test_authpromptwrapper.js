// NOTE: This tests code outside of Necko. The test still lives here because
// the contract is part of Necko.

// TODO:
// - HTTPS
// - Proxies

const nsIAuthInformation = Ci.nsIAuthInformation;
const nsIAuthPromptAdapterFactory = Ci.nsIAuthPromptAdapterFactory;

function run_test() {
  const contractID = "@mozilla.org/network/authprompt-adapter-factory;1";
  if (!(contractID in Cc)) {
    print("No adapter factory found, skipping testing");
    return;
  }
  var adapter = Cc[contractID].getService();
  Assert.equal(adapter instanceof nsIAuthPromptAdapterFactory, true);

  // NOTE: xpconnect lets us get away with passing an empty object here
  // For this part of the test, we only care that this function returns
  // success
  Assert.notEqual(adapter.createAdapter({}), null);

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
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIAuthPrompt))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
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
      Assert.equal(this.scheme + "://" + host + " (" + info.realm + ")", realm);

      Assert.notEqual(text.indexOf(host), -1);
      if (info.flags & nsIAuthInformation.ONLY_PASSWORD) {
        // Should have the username in the text
        Assert.notEqual(text.indexOf(info.username), -1);
      } else {
        // Make sure that we show the realm if we have one and that we don't
        // show "" otherwise
        if (info.realm != "")
          Assert.notEqual(text.indexOf(info.realm), -1);
        else
          Assert.equal(text.indexOf('""'), -1);
        // No explicit port in the URL; message should not contain -1
        // for those cases
        Assert.equal(text.indexOf("-1"), -1);
      }
    }
  };


  // Also have to make up a channel
  var uri = NetUtil.newURI("http://" + host)
  var chan = NetUtil.newChannel({
    uri: uri,
    loadUsingSystemPrincipal: true
  });

  function do_tests(expectedRV) {
    var prompt1;
    var wrapper;

    // 1: The simple case
    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    wrapper = adapter.createAdapter(prompt1);

    var rv = wrapper.promptAuth(chan, 0, info);
    Assert.equal(rv, prompt1.rv);
    Assert.equal(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      Assert.equal(info.domain, "");
      Assert.equal(info.username, prompt1.user);
      Assert.equal(info.password, prompt1.pw);
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
    Assert.equal(rv, prompt1.rv);
    Assert.equal(prompt1.called, CALLED_PROMPTP);

    if (rv) {
      Assert.equal(info.domain, "");
      Assert.equal(info.username, prompt1.user); // we initialized this
      Assert.equal(info.password, prompt1.pw);
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
    Assert.equal(rv, prompt1.rv);
    Assert.equal(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      Assert.equal(info.domain, "foo");
      Assert.equal(info.username, "bar");
      Assert.equal(info.password, prompt1.pw);
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
    Assert.equal(rv, prompt1.rv);
    Assert.equal(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      Assert.equal(info.domain, "");
      Assert.equal(info.username, prompt1.user);
      Assert.equal(info.password, prompt1.pw);
    }

    info.flags &= ~nsIAuthInformation.NEED_DOMAIN;

    info.domain = "";
    info.username = "";
    info.password = "";

    // 5: FTP
    var uri2 = NetUtil.newURI("ftp://" + host);
    var ftpchan = NetUtil.newChannel({
      uri: uri2,
      loadUsingSystemPrincipal: true 
    });

    prompt1 = new Prompt1();
    prompt1.rv = expectedRV;
    prompt1.scheme = "ftp";

    wrapper = adapter.createAdapter(prompt1);
    var rv = wrapper.promptAuth(ftpchan, 0, info);
    Assert.equal(rv, prompt1.rv);
    Assert.equal(prompt1.called, CALLED_PROMPTUP);

    if (rv) {
      Assert.equal(info.domain, "");
      Assert.equal(info.username, prompt1.user);
      Assert.equal(info.password, prompt1.pw);
    }

    info.domain = "";
    info.username = "";
    info.password = "";
  }
  do_tests(true);
  do_tests(false);
}

