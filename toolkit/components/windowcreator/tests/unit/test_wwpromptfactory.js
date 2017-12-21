function run_test() {
  // Make sure that getting both nsIAuthPrompt and nsIAuthPrompt2 works
  // (these should work independently of whether the application has
  // nsIPromptService)
  var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService();

  var prompt;

  prompt = ww.nsIWindowWatcher.getNewPrompter(null);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIWindowWatcher.getNewAuthPrompter(null);
  Assert.notEqual(prompt, null);

  prompt = ww.nsIPromptFactory.getPrompt(null,
                                         Components.interfaces.nsIPrompt);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIPromptFactory.getPrompt(null,
                                         Components.interfaces.nsIAuthPrompt);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIPromptFactory.getPrompt(null,
                                         Components.interfaces.nsIAuthPrompt2);
  Assert.notEqual(prompt, null);
}
