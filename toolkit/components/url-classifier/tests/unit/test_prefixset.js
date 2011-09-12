// newPset: returns an empty nsIUrlClassifierPrefixSet.
function newPset() {
  return Cc["@mozilla.org/url-classifier/prefixset;1"]
           .createInstance(Ci.nsIUrlClassifierPrefixSet);
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

    do_check_false(pset.contains(randInt));
  }
}

// doExpectedLookups: we use this to test expected membership.
//    pset: a nsIUrlClassifierPrefixSet to test.
//    prefixes:
function doExpectedLookups(pset, prefixes, N) {
  for (let i = 0; i < N; i++) {
    prefixes.forEach(function (x) {
      dump("Checking " + x + "\n");
      do_check_true(pset.contains(x));
    });
  }
}

// testBasicPset: A very basic test of the prefix set to make sure that it
// exists and to give a basic example of its use.
function testBasicPset() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [2,100,50,2000,78000,1593203];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(pset.contains(100));
  do_check_false(pset.contains(100000));
  do_check_true(pset.contains(1593203));
  do_check_false(pset.contains(999));
  do_check_false(pset.contains(0));
}

function testDuplicates() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [1,1,2,2,2,3,3,3,3,3,3,5,6,6,7,7,9,9,9];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(pset.contains(1));
  do_check_true(pset.contains(2));
  do_check_true(pset.contains(5));
  do_check_true(pset.contains(9));
  do_check_false(pset.contains(4));
  do_check_false(pset.contains(8));
}

function testSimplePset() {
  let pset = newPset();
  let prefixes = [1,2,100,400,123456789];
  pset.setPrefixes(prefixes, prefixes.length);

  doRandomLookups(pset, prefixes, 100);
  doExpectedLookups(pset, prefixes, 1);
}

function testUnsortedPset() {
  let pset = newPset();
  let prefixes = [5,1,20,100,200000,100000];
  pset.setPrefixes(prefixes, prefixes.length);

  doRandomLookups(pset, prefixes, 100);
  doExpectedLookups(pset, prefixes, 1);
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
    do_check_false(pset.contains(prefixes[i]));
  }
}

function testLargeSet() {
  let N = 1000;
  let arr = [];

  for (let i = 0; i < N; i++) {
    let randInt = Math.floor(Math.random() * Math.pow(2, 32));
    arr.push(randInt);
  }

  arr.sort(function(x,y) x - y);

  let pset = newPset();
  pset.setPrefixes(arr, arr.length);

  doExpectedLookups(pset, arr, 1);
  doRandomLookups(pset, arr, 1000);
}

function testTinySet() {
  let pset = Cc["@mozilla.org/url-classifier/prefixset;1"]
               .createInstance(Ci.nsIUrlClassifierPrefixSet);
  let prefixes = [1];
  pset.setPrefixes(prefixes, prefixes.length);

  do_check_true(pset.contains(1));
  do_check_false(pset.contains(100000));

  prefixes = [];
  pset.setPrefixes(prefixes, prefixes.length);
  do_check_false(pset.contains(1));
}

let tests = [testBasicPset,
             testSimplePset,
             testUnsortedPset,
             testReSetPrefixes,
             testLargeSet,
             testDuplicates,
             testTinySet];

function run_test() {
  // None of the tests use |executeSoon| or any sort of callbacks, so we can
  // just run them in succession.
  for (let i = 0; i < tests.length; i++) {
    dump("Running " + tests[i].name + "\n");
    tests[i]();
  }
}
