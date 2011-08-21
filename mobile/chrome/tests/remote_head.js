// XXX Those constants are here because EventUtils.js need them
window = content.document.defaultView.wrappedJSObject;
Element = Components.interfaces.nsIDOMElement;
netscape = window.netscape;

let AsyncTests = {
  _tests: [],

  add: function(aMessage, aCallback) {
    addMessageListener(aMessage, this);
    this._tests.push({ name: aMessage, callback: aCallback });
  },

  receiveMessage: function(aMessage) {
    let rv = { };
    let name = aMessage.name;
    try {
      let tests = this._tests;
      for (let i = 0; i < tests.length; i++) {
        if (tests[i].name == name) {
          rv.result = tests[i].callback(name, aMessage.json);
          break;
        }
      }
      // Don't send test callback if rv.result == undefined, this allow to
      // use a custom callback
      if (rv.result != undefined)
        sendAsyncMessage(name, rv);
    }
    catch(e) {
      dump("receiveMessage: " + name + " - " + e + "\n");
    }
  }
};

