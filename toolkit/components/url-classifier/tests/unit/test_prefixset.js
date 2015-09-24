// newPset: returns an empty nsIUrlClassifierPrefixSet.
function newPset() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
            .createInstance(Ci.nsIUrlClassifierPrefixSet);
  pset.init("all");
  return pset;
}

// arrContains: returns true if |arr| contains the element |target|. Uses binary
// search and requires |arr| to be sorted.
function arrContains(arr, target) {
  let start = 0;
  let end = arr.length - 1;
  let i = 0;

  while (end > start) {
    i = start + (end - start >> 1);
    let value = arr[i];

    if (value < target)
      start = i+1;
    else if (value > target)
      end = i-1;
    else
      break;
  }
  if (start == end)
    i = start;

  return (!(i < 0 || i >= arr.length) && arr[i] == target);
}

// checkContents: Check whether the PrefixSet pset contains
// the prefixes in the passed array.
function checkContents(pset, prefixes) {
  var outcount = {}, outset = {};
  outset = pset.getPrefixes(outcount);
  let inset = prefixes;
  do_check_eq(inset.length, outset.length);
  inset.sort((x,y) => x - y);
  for (let i = 0; i < inset.length; i++) {
    do_check_eq(inset[i], outset[i]);
  }
}

function wrappedProbe(pset, prefix) {
  return pset.contains(prefix);
};

// doRandomLookups: we use this to test for false membership with random input
// over the range of prefixes (unsigned 32-bits integers).
//    pset: a nsIUrlClassifierPrefixSet to test.
//    prefixes: an array of prefixes supposed to make up the prefix set.
//    N: number of random lookups to make.
function doRandomLookups(pset, prefixes, N) {
  for (let i = 0; i < N; i++) {
    let randInt = prefixes[0];
    while (arrContains(prefixes, randInt))
      randInt = Math.floor(Math.random() * Math.pow(2, 32));

    do_check_false(wrappedProbe(pset, randInt));
  }
}

// doExpectedLookups: we use this to test expected membership.
//    pset: a nsIUrlClassifierPrefixSet to test.
//    prefixes:
function doExpectedLookups(pset, prefixes, N) {
  for (let i = 0; i < N; i++) {
    prefixes.forEach(function (x) {
      dump("Checking " + x + "\n");
      do_check_true(wrappedProbe(pset, x));
    });
  }
}

// testBasicPset: A very basic test of the prefix set to make sure that it
// exists and to give a basic example of its use.
function testBasicPset() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [2,50,100,2000,78000,1593203];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(wrappedProbe(pset, 100));
  do_check_false(wrappedProbe(pset, 100000));
  do_check_true(wrappedProbe(pset, 1593203));
  do_check_false(wrappedProbe(pset, 999));
  do_check_false(wrappedProbe(pset, 0));


  checkContents(pset, prefixes);
}

function testDuplicates() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [1,1,2,2,2,3,3,3,3,3,3,5,6,6,7,7,9,9,9];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(wrappedProbe(pset, 1));
  do_check_true(wrappedProbe(pset, 2));
  do_check_true(wrappedProbe(pset, 5));
  do_check_true(wrappedProbe(pset, 9));
  do_check_false(wrappedProbe(pset, 4));
  do_check_false(wrappedProbe(pset, 8));


  checkContents(pset, prefixes);
}

function testSimplePset() {
  let pset = newPset();
  let prefixes = [1,2,100,400,123456789];
  pset.setPrefixes(prefixes, prefixes.length);

  doRandomLookups(pset, prefixes, 100);
  doExpectedLookups(pset, prefixes, 1);


  checkContents(pset, prefixes);
}

function testReSetPrefixes() {
  let pset = newPset();
  let prefixes = [1, 5, 100, 1000, 150000];
  pset.setPrefixes(prefixes, prefixes.length);

  doExpectedLookups(pset, prefixes, 1);

  let secondPrefixes = [12, 50, 300, 2000, 5000, 200000];
  pset.setPrefixes(secondPrefixes, secondPrefixes.length);

  doExpectedLookups(pset, secondPrefixes, 1);
  for (let i = 0; i < prefixes.length; i++) {
    do_check_false(wrappedProbe(pset, prefixes[i]));
  }


  checkContents(pset, secondPrefixes);
}

function testLoadSaveLargeSet() {
  let N = 1000;
  let arr = [];

  for (let i = 0; i < N; i++) {
    let randInt = Math.floor(Math.random() * Math.pow(2, 32));
    arr.push(randInt);
  }

  arr.sort((x,y) => x - y);

  let pset = newPset();
  pset.setPrefixes(arr, arr.length);

  doExpectedLookups(pset, arr, 1);
  doRandomLookups(pset, arr, 1000);

  checkContents(pset, arr);

  // Now try to save, restore, and redo the lookups
  var file = dirSvc.get('ProfLD', Ci.nsIFile);
  file.append("testLarge.pset");

  pset.storeToFile(file);

  let psetLoaded = newPset();
  psetLoaded.loadFromFile(file);

  doExpectedLookups(psetLoaded, arr, 1);
  doRandomLookups(psetLoaded, arr, 1000);

  checkContents(psetLoaded, arr);
}

function testTinySet() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [1];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(wrappedProbe(pset, 1));
  do_check_false(wrappedProbe(pset, 100000));
  checkContents(pset, prefixes);

  prefixes = [];
  pset.setPrefixes(prefixes, prefixes.length);
  do_check_false(wrappedProbe(pset, 1));
  checkContents(pset, prefixes);
}

function testLoadSaveNoDelta() {
  let N = 100;
  let arr = [];

  for (let i = 0; i < N; i++) {
    // construct a tree without deltas by making the distance
    // between entries larger than 16 bits
    arr.push(((1 << 16) + 1) * i);
  }

  let pset = newPset();
  pset.setPrefixes(arr, arr.length);

  doExpectedLookups(pset, arr, 1);

  var file = dirSvc.get('ProfLD', Ci.nsIFile);
  file.append("testNoDelta.pset");

  pset.storeToFile(file);
  pset.loadFromFile(file);

  doExpectedLookups(pset, arr, 1);
}

var tests = [testBasicPset,
             testSimplePset,
             testReSetPrefixes,
             testLoadSaveLargeSet,
             testDuplicates,
             testTinySet,
             testLoadSaveNoDelta];

function run_test() {
  // None of the tests use |executeSoon| or any sort of callbacks, so we can
  // just run them in succession.
  for (let i = 0; i < tests.length; i++) {
    dump("Running " + tests[i].name + "\n");
    tests[i]();
  }
}
