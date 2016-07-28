const { interfaces: Ci, classes: Cc, utils: Cu } = Components;
const { FinderIterator } = Cu.import("resource://gre/modules/FinderIterator.jsm", {});
Cu.import("resource://gre/modules/Promise.jsm");

var gFindResults = [];
// Stub the method that instantiates nsIFind and does all the interaction with
// the docShell to be searched through.
FinderIterator._iterateDocument = function* (word, window, finder) {
  for (let range of gFindResults)
    yield range;
};

FinderIterator._rangeStartsInLink = fakeRange => fakeRange.startsInLink;

function FakeRange(textContent, startsInLink = false) {
  this.startContainer = {};
  this.startsInLink = startsInLink;
  this.toString = () => textContent;
}

var gMockWindow = {
  setTimeout(cb, delay) {
    Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer)
      .initWithCallback(cb, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  }
};

var gMockFinder = {
  _getWindow() { return gMockWindow; }
};

function prepareIterator(findText, rangeCount) {
  gFindResults = [];
  for (let i = rangeCount; --i >= 0;)
    gFindResults.push(new FakeRange(findText));
}

add_task(function* test_start() {
  let findText = "test";
  let rangeCount = 300;
  prepareIterator(findText, rangeCount);

  let count = 0;
  yield FinderIterator.start({
    finder: gMockFinder,
    onRange: range => {
      ++count;
      Assert.equal(range.toString(), findText, "Text content should match");
    },
    word: findText
  });

  Assert.equal(rangeCount, count, "Amount of ranges yielded should match!");
  Assert.ok(!FinderIterator.running, "Running state should match");
  Assert.equal(FinderIterator._previousRanges.length, rangeCount, "Ranges cache should match");
});

add_task(function* test_valid_arguments() {
  let findText = "foo";
  let rangeCount = 20;
  prepareIterator(findText, rangeCount);

  let count = 0;

  yield FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count,
    word: findText
  });

  let params = FinderIterator._previousParams;
  Assert.ok(!params.linksOnly, "Default for linksOnly is false");
  Assert.ok(!params.useCache, "Default for useCache is false");
  Assert.equal(params.word, findText, "Words should match");

  count = 0;
  Assert.throws(() => FinderIterator.start({
    onRange: range => ++count,
    word: findText
  }), /Missing required option 'finder'/, "Should throw when missing an argument");
  FinderIterator.reset();

  Assert.throws(() => FinderIterator.start({
    finder: gMockFinder,
    word: findText
  }), /Missing valid, required option 'onRange'/, "Should throw when missing an argument");
  FinderIterator.reset();

  Assert.throws(() => FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count
  }), /Missing required option 'word'/, "Should throw when missing an argument");
  FinderIterator.reset();

  Assert.equal(count, 0, "No ranges should've been counted");
});

add_task(function* test_stop() {
  let findText = "bar";
  let rangeCount = 120;
  prepareIterator(findText, rangeCount);

  let count = 0;
  let whenDone = FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count,
    word: findText
  });

  FinderIterator.stop();

  yield whenDone;

  Assert.equal(count, 100, "Number of ranges should match `kIterationSizeMax`");
});

add_task(function* test_reset() {
  let findText = "tik";
  let rangeCount = 142;
  prepareIterator(findText, rangeCount);

  let count = 0;
  let whenDone = FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count,
    word: findText
  });

  Assert.ok(FinderIterator.running, "Yup, running we are");
  Assert.equal(count, 100, "Number of ranges should match `kIterationSizeMax`");
  Assert.equal(FinderIterator.ranges.length, 100,
    "Number of ranges should match `kIterationSizeMax`");

  FinderIterator.reset();

  Assert.ok(!FinderIterator.running, "Nope, running we are not");
  Assert.equal(FinderIterator.ranges.length, 0, "No ranges after reset");
  Assert.equal(FinderIterator._previousRanges.length, 0, "No ranges after reset");

  yield whenDone;

  Assert.equal(count, 100, "Number of ranges should match `kIterationSizeMax`");
});

add_task(function* test_parallel_starts() {
  let findText = "tak";
  let rangeCount = 2143;
  prepareIterator(findText, rangeCount);

  // Start off the iterator.
  let count = 0;
  let whenDone = FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count,
    word: findText
  });

  // Start again after a few milliseconds.
  yield new Promise(resolve => gMockWindow.setTimeout(resolve, 2));
  Assert.ok(FinderIterator.running, "We ought to be running here");

  let count2 = 0;
  let whenDone2 = FinderIterator.start({
    finder: gMockFinder,
    onRange: range => ++count2,
    word: findText
  });

  // Let the iterator run for a little while longer before we assert the world.
  yield new Promise(resolve => gMockWindow.setTimeout(resolve, 10));
  FinderIterator.stop();

  Assert.ok(!FinderIterator.running, "Stop means stop");

  yield whenDone;
  yield whenDone2;

  Assert.greater(count, FinderIterator.kIterationSizeMax, "At least one range should've been found");
  Assert.less(count, rangeCount, "Not all ranges should've been found");
  Assert.greater(count2, FinderIterator.kIterationSizeMax, "At least one range should've been found");
  Assert.less(count2, rangeCount, "Not all ranges should've been found");

  Assert.less(count2, count, "The second start was later, so should have fewer results");
});
