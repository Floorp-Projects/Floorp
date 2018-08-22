var INPUTS = {
  HTML: "Hi, I want to be transformed!",
  SCRIPTURL: "http://this.is.a.scripturl.test/",
  URL: "http://hello.i.am.an.url/"
};

var RESULTS = {
  HTML: "Quack, I want to be a duck!",
  SCRIPTURL: "http://this.is.a.successful.test/",
  URL: "http://hooray.i.am.successfully.transformed/"
};

function createHTMLJS(html) {
  return html.replace("Hi", "Quack")
      .replace("transformed", "a duck");
}

function createScriptURLJS(scripturl) {
  return scripturl.replace("scripturl", "successful");
}

function createURLJS(url) {
  return url.replace("hello", "hooray")
      .replace("an.url", "successfully.transformed");
}

function createHTML_policy(win) {
  return win.trustedTypes.createPolicy('SomeName', { createHTML: createHTMLJS });
}

function createScriptURL_policy(win) {
  return win.trustedTypes.createPolicy('SomeName', { createScriptURL: createScriptURLJS });
}

function createURL_policy(win) {
  return win.trustedTypes.createPolicy('SomeName', { createURL: createURLJS });
}

function assert_element_accepts_trusted_html(win, t, tag, attribute, expected) {
  createHTML_policy(win)
      .then(t.step_func_done(p => {
          let html = p.createHTML(INPUTS.HTML);
          assert_element_accepts_trusted_type(tag, attribute, html, expected);
      }));
}

function assert_element_accepts_trusted_script_url(win, t, tag, attribute, expected) {
  createScriptURL_policy(win)
      .then(t.step_func_done(p => {
          let scripturl = p.createScriptURL(INPUTS.SCRIPTURL);
          assert_element_accepts_trusted_type(tag, attribute, scripturl, expected);
      }));
}

function assert_element_accepts_trusted_url(win, t, tag, attribute, expected) {
  createURL_policy(win)
      .then(t.step_func_done(p => {
          let url = p.createURL(INPUTS.URL);
          assert_element_accepts_trusted_type(tag, attribute, url, expected);
      }));
}

function assert_element_accepts_trusted_type(tag, attribute, value, expected) {
  let elem = document.createElement(tag);
  elem[attribute] = value;
  assert_equals(elem[attribute] + "", expected);
}

function assert_throws_no_trusted_type(tag, attribute, value) {
  let elem = document.createElement(tag);
  assert_throws(new TypeError(), _ => {
    elem[attribute] = value;
  });
}
