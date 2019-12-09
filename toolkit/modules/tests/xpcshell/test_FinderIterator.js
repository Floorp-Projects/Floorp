const { FinderIterator } = ChromeUtils.import(
  "resource://gre/modules/FinderIterator.jsm"
);

let finderIterator = new FinderIterator();

var gFindResults = [];
// Stub the method that instantiates nsIFind and does all the interaction with
// the docShell to be searched through.
finderIterator._iterateDocument = function*(word, window, finder) {
  for (let range of gFindResults) {
    yield range;
  }
};

finderIterator._rangeStartsInLink = fakeRange => fakeRange.startsInLink;

function FakeRange(textContent, startsInLink = false) {
  this.startContainer = {};
  this.startsInLink = startsInLink;
  this.toString = () => textContent;
}

var gMockWindow = {
  setTimeout(cb, delay) {
    Cc["@mozilla.org/timer;1"]
      .createInstance(Ci.nsITimer)
      .initWithCallback(cb, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  },
};

var gMockFinder = {
  _getWindow() {
    return gMockWindow;
  },
};

function prepareIterator(findText, rangeCount) {
  gFindResults = [];
  for (let i = rangeCount; --i >= 0; ) {
    gFindResults.push(new FakeRange(findText));
  }
}

add_task(async function test_start() {
  let findText = "test";
  let rangeCount = 300;
  prepareIterator(findText, rangeCount);

  let count = 0;
  await finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
        Assert.equal(range.toString(), findText, "Text content should match");
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  Assert.equal(rangeCount, count, "Amount of ranges yielded should match!");
  Assert.ok(!finderIterator.running, "Running state should match");
  Assert.equal(
    finderIterator._previousRanges.length,
    rangeCount,
    "Ranges cache should match"
  );

  finderIterator.reset();
});

add_task(async function test_subframes() {
  let findText = "test";
  let rangeCount = 300;
  prepareIterator(findText, rangeCount);

  let count = 0;
  await finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
        Assert.equal(range.toString(), findText, "Text content should match");
      },
    },
    matchDiacritics: false,
    word: findText,
    useSubFrames: true,
  });

  Assert.equal(rangeCount, count, "Amount of ranges yielded should match!");
  Assert.ok(!finderIterator.running, "Running state should match");
  Assert.equal(
    finderIterator._previousRanges.length,
    rangeCount,
    "Ranges cache should match"
  );

  finderIterator.reset();
});

add_task(async function test_valid_arguments() {
  let findText = "foo";
  let rangeCount = 20;
  prepareIterator(findText, rangeCount);

  let count = 0;

  await finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  let params = finderIterator._previousParams;
  Assert.ok(!params.linksOnly, "Default for linksOnly is false");
  Assert.ok(!params.useCache, "Default for useCache is false");
  Assert.equal(params.word, findText, "Words should match");

  count = 0;
  Assert.throws(
    () =>
      finderIterator.start({
        entireWord: false,
        listener: {
          onIteratorRangeFound(range) {
            ++count;
          },
        },
        matchDiacritics: false,
        word: findText,
      }),
    /Missing required option 'caseSensitive'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.throws(
    () =>
      finderIterator.start({
        caseSensitive: false,
        entireWord: false,
        listener: {
          onIteratorRangeFound(range) {
            ++count;
          },
        },
        word: findText,
      }),
    /Missing required option 'matchDiacritics'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.throws(
    () =>
      finderIterator.start({
        caseSensitive: false,
        listener: {
          onIteratorRangeFound(range) {
            ++count;
          },
        },
        matchDiacritics: false,
        word: findText,
      }),
    /Missing required option 'entireWord'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.throws(
    () =>
      finderIterator.start({
        caseSensitive: false,
        entireWord: false,
        listener: {
          onIteratorRangeFound(range) {
            ++count;
          },
        },
        matchDiacritics: false,
        word: findText,
      }),
    /Missing required option 'finder'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.throws(
    () =>
      finderIterator.start({
        caseSensitive: true,
        entireWord: false,
        finder: gMockFinder,
        matchDiacritics: false,
        word: findText,
      }),
    /Missing valid, required option 'listener'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.throws(
    () =>
      finderIterator.start({
        caseSensitive: false,
        entireWord: true,
        finder: gMockFinder,
        listener: {
          onIteratorRangeFound(range) {
            ++count;
          },
        },
        matchDiacritics: false,
      }),
    /Missing required option 'word'/,
    "Should throw when missing an argument"
  );
  finderIterator.reset();

  Assert.equal(count, 0, "No ranges should've been counted");
});

add_task(async function test_stop() {
  let findText = "bar";
  let rangeCount = 120;
  prepareIterator(findText, rangeCount);

  let count = 0;
  let whenDone = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  finderIterator.stop();

  await whenDone;

  Assert.equal(count, 0, "Number of ranges should be 0");

  finderIterator.reset();
});

add_task(async function test_reset() {
  let findText = "tik";
  let rangeCount = 142;
  prepareIterator(findText, rangeCount);

  let count = 0;
  let whenDone = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  Assert.ok(finderIterator.running, "Yup, running we are");
  Assert.equal(count, 0, "Number of ranges should match 0");
  Assert.equal(
    finderIterator.ranges.length,
    0,
    "Number of ranges should match 0"
  );

  finderIterator.reset();

  Assert.ok(!finderIterator.running, "Nope, running we are not");
  Assert.equal(finderIterator.ranges.length, 0, "No ranges after reset");
  Assert.equal(
    finderIterator._previousRanges.length,
    0,
    "No ranges after reset"
  );

  await whenDone;

  Assert.equal(count, 0, "Number of ranges should match 0");
});

add_task(async function test_parallel_starts() {
  let findText = "tak";
  let rangeCount = 2143;
  prepareIterator(findText, rangeCount);

  // Start off the iterator.
  let count = 0;
  let whenDone = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  await new Promise(resolve => gMockWindow.setTimeout(resolve, 100));
  Assert.ok(finderIterator.running, "We ought to be running here");

  let count2 = 0;
  let whenDone2 = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count2;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  // Let the iterator run for a little while longer before we assert the world.
  await new Promise(resolve => gMockWindow.setTimeout(resolve, 10));
  finderIterator.stop();

  Assert.ok(!finderIterator.running, "Stop means stop");

  await whenDone;
  await whenDone2;

  Assert.greater(
    count,
    finderIterator.kIterationSizeMax,
    "At least one range should've been found"
  );
  Assert.less(count, rangeCount, "Not all ranges should've been found");
  Assert.greater(
    count2,
    finderIterator.kIterationSizeMax,
    "At least one range should've been found"
  );
  Assert.less(count2, rangeCount, "Not all ranges should've been found");

  Assert.equal(
    count2,
    count,
    "The second start was later, but should have caught up"
  );

  finderIterator.reset();
});

add_task(async function test_allowDistance() {
  let findText = "gup";
  let rangeCount = 20;
  prepareIterator(findText, rangeCount);

  // Start off the iterator.
  let count = 0;
  let whenDone = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count;
      },
    },
    matchDiacritics: false,
    word: findText,
  });

  let count2 = 0;
  let whenDone2 = finderIterator.start({
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count2;
      },
    },
    matchDiacritics: false,
    word: "gu",
  });

  let count3 = 0;
  let whenDone3 = finderIterator.start({
    allowDistance: 1,
    caseSensitive: false,
    entireWord: false,
    finder: gMockFinder,
    listener: {
      onIteratorRangeFound(range) {
        ++count3;
      },
    },
    matchDiacritics: false,
    word: "gu",
  });

  await Promise.all([whenDone, whenDone2, whenDone3]);

  Assert.equal(
    count,
    rangeCount,
    "The first iterator invocation should yield all results"
  );
  Assert.equal(
    count2,
    0,
    "The second iterator invocation should yield _no_ results"
  );
  Assert.equal(
    count3,
    rangeCount,
    "The first iterator invocation should yield all results"
  );

  finderIterator.reset();
});
