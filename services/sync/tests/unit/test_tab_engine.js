Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/util.js");

function fakeSessionSvc() {
  let tabs = [];
  for(let i = 0; i < arguments.length; i++) {
    tabs.push({
      index: 1,
      entries: [{
        url: arguments[i],
        title: "title"
      }],
      attributes: {
        image: "image"
      },
      extData: {
        weaveLastUsed: 1
      }
    });
  }
  let obj = {windows: [{tabs: tabs}]};

  // delete the getter, or the previously created fake Session
  delete Svc.Session;
  Svc.Session = {
    getBrowserState: function() JSON.stringify(obj)
  };
}

function run_test() {

  _("test locallyOpenTabMatchesURL");
  let engine = new TabEngine();

  // 3 tabs
  fakeSessionSvc("http://bar.com", "http://foo.com", "http://foobar.com");

  let matches;
 
  _("  test matching works (true)");
  matches = engine.locallyOpenTabMatchesURL("http://foo.com");
  do_check_true(matches);

  _("  test matching works (false)");
  matches = engine.locallyOpenTabMatchesURL("http://barfoo.com");
  do_check_false(matches);
}
