// Utilities for running tests in an e10s environment.

function e10s_init() {
  // Listen for an 'oop-browser-crashed' event and log it so people analysing
  // test logs have a clue about what is going on.
  window.addEventListener("oop-browser-crashed", (event) => {
    let uri = event.target.currentURI;
    Cu.reportError("remote browser crashed while on " +
                   (uri ? uri.spec : "<unknown>") + "\n");
  }, true);
}
