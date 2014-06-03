/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the LayoutChangesObserver

let {
  getLayoutChangesObserver,
  releaseLayoutChangesObserver,
  LayoutChangesObserver
} = devtools.require("devtools/server/actors/layout");

// Override set/clearTimeout on LayoutChangesObserver to avoid depending on
// time in this unit test. This means that LayoutChangesObserver.eventLoopTimer
// will be the timeout callback instead of the timeout itself, so test cases
// will need to execute it to fake a timeout
LayoutChangesObserver.prototype._setTimeout = cb => cb;
LayoutChangesObserver.prototype._clearTimeout = function() {};

// Mock the tabActor since we only really want to test the LayoutChangesObserver
// and don't want to depend on a window object, nor want to test protocol.js
function MockTabActor() {
  this.window = new MockWindow();
  this.windows = [this.window];
}

function MockWindow() {}
MockWindow.prototype = {
  QueryInterface: function() {
    let self = this;
    return {
      getInterface: function() {
        return {
          QueryInterface: function() {
            if (!self.docShell) {
              self.docShell = new MockDocShell();
            }
            return self.docShell;
          }
        };
      }
    };
  },
  setTimeout: function(cb) {
    // Simply return the cb itself so that we can execute it in the test instead
    // of depending on a real timeout
    return cb;
  },
  clearTimeout: function() {}
};

function MockDocShell() {
  this.observer = null;
}
MockDocShell.prototype = {
  addWeakReflowObserver: function(observer) {
    this.observer = observer;
  },
  removeWeakReflowObserver: function(observer) {}
};

function run_test() {
  instancesOfObserversAreSharedBetweenWindows();
  eventsAreBatched();
  noEventsAreSentWhenThereAreNoReflowsAndLoopTimeouts();
  observerIsAlreadyStarted();
  destroyStopsObserving();
  stoppingAndStartingSeveralTimesWorksCorrectly();
  reflowsArentStackedWhenStopped();
  stackedReflowsAreResetOnStop();
}

function instancesOfObserversAreSharedBetweenWindows() {
  do_print("Checking that when requesting twice an instances of the observer " +
    "for the same TabActor, the instance is shared");

  do_print("Checking 2 instances of the observer for the tabActor 1");
  let tabActor1 = new MockTabActor();
  let obs11 = getLayoutChangesObserver(tabActor1);
  let obs12 = getLayoutChangesObserver(tabActor1);
  do_check_eq(obs11, obs12);

  do_print("Checking 2 instances of the observer for the tabActor 2");
  let tabActor2 = new MockTabActor();
  let obs21 = getLayoutChangesObserver(tabActor2);
  let obs22 = getLayoutChangesObserver(tabActor2);
  do_check_eq(obs21, obs22);

  do_print("Checking that observers instances for 2 different tabActors are " +
    "different");
  do_check_neq(obs11, obs21);

  releaseLayoutChangesObserver(tabActor1);
  releaseLayoutChangesObserver(tabActor1);
  releaseLayoutChangesObserver(tabActor2);
  releaseLayoutChangesObserver(tabActor2);
}

function eventsAreBatched() {
  do_print("Checking that reflow events are batched and only sent when the " +
    "timeout expires");

  // Note that in this test, we mock the TabActor and its window property, so we
  // also mock the setTimeout/clearTimeout mechanism and just call the callback
  // manually
  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);

  let reflowsEvents = [];
  let onReflows = (event, reflows) => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  do_print("Fake one reflow event");
  tabActor.window.docShell.observer.reflow();
  do_print("Checking that no batched reflow event has been emitted");
  do_check_eq(reflowsEvents.length, 0);

  do_print("Fake another reflow event");
  tabActor.window.docShell.observer.reflow();
  do_print("Checking that still no batched reflow event has been emitted");
  do_check_eq(reflowsEvents.length, 0);

  do_print("Faking timeout expiration and checking that reflow events are sent");
  observer.eventLoopTimer();
  do_check_eq(reflowsEvents.length, 1);
  do_check_eq(reflowsEvents[0].length, 2);

  observer.off("reflows", onReflows);
  releaseLayoutChangesObserver(tabActor);
}

function noEventsAreSentWhenThereAreNoReflowsAndLoopTimeouts() {
 do_print("Checking that if no reflows were detected and the event batching " +
  "loop expires, then no reflows event is sent");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);

  let reflowsEvents = [];
  let onReflows = (event, reflows) => reflowsEvents.push(reflows);
  observer.on("reflows", onReflows);

  do_print("Faking timeout expiration and checking for reflows");
  observer.eventLoopTimer();
  do_check_eq(reflowsEvents.length, 0);

  observer.off("reflows", onReflows);
  releaseLayoutChangesObserver(tabActor);
}

function observerIsAlreadyStarted() {
  do_print("Checking that the observer is already started when getting it");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);
  do_check_true(observer.observing);

  observer.stop();
  do_check_false(observer.observing);

  observer.start();
  do_check_true(observer.observing);

  releaseLayoutChangesObserver(tabActor);
}

function destroyStopsObserving() {
  do_print("Checking that the destroying the observer stops it");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);
  do_check_true(observer.observing);

  observer.destroy();
  do_check_false(observer.observing);

  releaseLayoutChangesObserver(tabActor);
}

function stoppingAndStartingSeveralTimesWorksCorrectly() {
  do_print("Checking that the stopping and starting several times the observer" +
    " works correctly");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);

  do_check_true(observer.observing);
  observer.start();
  observer.start();
  observer.start();
  do_check_true(observer.observing);

  observer.stop();
  do_check_false(observer.observing);

  observer.stop();
  observer.stop();
  do_check_false(observer.observing);

  releaseLayoutChangesObserver(tabActor);
}

function reflowsArentStackedWhenStopped() {
  do_print("Checking that when stopped, reflows aren't stacked in the observer");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);

  do_print("Stoping the observer");
  observer.stop();

  do_print("Faking reflows");
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();

  do_print("Checking that reflows aren't recorded");
  do_check_eq(observer.reflows.length, 0);

  do_print("Starting the observer and faking more reflows");
  observer.start();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();
  tabActor.window.docShell.observer.reflow();

  do_print("Checking that reflows are recorded");
  do_check_eq(observer.reflows.length, 3);

  releaseLayoutChangesObserver(tabActor);
}

function stackedReflowsAreResetOnStop() {
  do_print("Checking that stacked reflows are reset on stop");

  let tabActor = new MockTabActor();
  let observer = getLayoutChangesObserver(tabActor);

  tabActor.window.docShell.observer.reflow();
  do_check_eq(observer.reflows.length, 1);

  observer.stop();
  do_check_eq(observer.reflows.length, 0);

  tabActor.window.docShell.observer.reflow();
  do_check_eq(observer.reflows.length, 0);

  observer.start();
  do_check_eq(observer.reflows.length, 0);

  tabActor.window.docShell.observer.reflow();
  do_check_eq(observer.reflows.length, 1);

  releaseLayoutChangesObserver(tabActor);
}
