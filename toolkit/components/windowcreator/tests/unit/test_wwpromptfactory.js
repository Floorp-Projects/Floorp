function run_test() {
  // Make sure that getting both nsIAuthPrompt and nsIAuthPrompt2 works
  // (these should work independently of whether the application has
  // nsIPromptService)
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].getService();

  var prompt;

  prompt = ww.nsIWindowWatcher.getNewPrompter(null);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIWindowWatcher.getNewAuthPrompter(null);
  Assert.notEqual(prompt, null);

  prompt = ww.nsIPromptFactory.getPrompt(null, Ci.nsIPrompt);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt);
  Assert.notEqual(prompt, null);
  prompt = ww.nsIPromptFactory.getPrompt(null, Ci.nsIAuthPrompt2);
  Assert.notEqual(prompt, null);
}
