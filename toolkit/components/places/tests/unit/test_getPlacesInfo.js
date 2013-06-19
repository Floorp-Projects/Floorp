/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function promiseGetPlacesInfo(aPlacesIdentifiers) {
  let deferred = Promise.defer();
  PlacesUtils.asyncHistory.getPlacesInfo(aPlacesIdentifiers, {
    _results: [],
    _errors: [],

    handleResult: function handleResult(aPlaceInfo) {
      this._results.push(aPlaceInfo);
    },
    handleError: function handleError(aResultCode, aPlaceInfo) {
      this._errors.push({ resultCode: aResultCode, info: aPlaceInfo });
    },
    handleCompletion: function handleCompletion() {
      deferred.resolve({ errors: this._errors, results: this._results });
    }
  });

  return deferred.promise;
}

function ensurePlacesInfoObjectsAreEqual(a, b) {
  do_check_true(a.uri.equals(b.uri));
  do_check_eq(a.title, b.title);
  do_check_eq(a.guid, b.guid);
  do_check_eq(a.placeId, b.placeId);
}

function test_getPlacesInfoExistentPlace() {
  let testURI = NetUtil.newURI("http://www.example.tld");
  yield promiseAddVisits(testURI);

  let getPlacesInfoResult = yield promiseGetPlacesInfo([testURI]);
  do_check_eq(getPlacesInfoResult.results.length, 1);
  do_check_eq(getPlacesInfoResult.errors.length, 0);

  let placeInfo = getPlacesInfoResult.results[0];
  do_check_true(placeInfo instanceof Ci.mozIPlaceInfo);

  do_check_true(placeInfo.uri.equals(testURI));
  do_check_eq(placeInfo.title, "test visit for " + testURI.spec);
  do_check_true(placeInfo.guid.length > 0);
  do_check_eq(placeInfo.visits, null);
}
add_task(test_getPlacesInfoExistentPlace);

function test_getPlacesInfoNonExistentPlace() {
  let testURI = NetUtil.newURI("http://www.example_non_existent.tld");
  let getPlacesInfoResult = yield promiseGetPlacesInfo(testURI);
  do_check_eq(getPlacesInfoResult.results.length, 0);
  do_check_eq(getPlacesInfoResult.errors.length, 1);
}
add_task(test_getPlacesInfoNonExistentPlace);

function test_promisedHelper() {
  let (uri = NetUtil.newURI("http://www.helper_existent_example.tld")) {
    yield promiseAddVisits(uri);
    let placeInfo = yield PlacesUtils.promisePlaceInfo(uri);
    do_check_true(placeInfo instanceof Ci.mozIPlaceInfo);
  };

  let (uri = NetUtil.newURI("http://www.helper_non_existent_example.tld")) {
    try {
      let placeInfo = yield PlacesUtils.promisePlaceInfo(uri);
      do_throw("PlacesUtils.promisePlaceInfo should have rejected the promise");
    }
    catch(ex) { }
  };
}
add_task(test_promisedHelper);

function test_infoByGUID() {
  let testURI = NetUtil.newURI("http://www.guid_example.tld");
  yield promiseAddVisits(testURI);

  let placeInfoByURI = yield PlacesUtils.promisePlaceInfo(testURI);
  let placeInfoByGUID = yield PlacesUtils.promisePlaceInfo(placeInfoByURI.guid);
  ensurePlacesInfoObjectsAreEqual(placeInfoByURI, placeInfoByGUID);
}
add_task(test_infoByGUID);

function test_invalid_guid() {
  try {
    let placeInfoByGUID = yield PlacesUtils.promisePlaceInfo("###");
    do_throw("getPlacesInfo should fail for invalid guids")
  }
  catch(ex) { }
}
add_task(test_invalid_guid);

function test_mixed_selection() {
  let placeInfo1, placeInfo2;
  let (uri = NetUtil.newURI("http://www.mixed_selection_test_1.tld")) {
    yield promiseAddVisits(uri);
    placeInfo1 = yield PlacesUtils.promisePlaceInfo(uri);
  };

  let (uri = NetUtil.newURI("http://www.mixed_selection_test_2.tld")) {
    yield promiseAddVisits(uri);
    placeInfo2 = yield PlacesUtils.promisePlaceInfo(uri);
  };

  let getPlacesInfoResult = yield promiseGetPlacesInfo([placeInfo1.uri, placeInfo2.guid]);
  do_check_eq(getPlacesInfoResult.results.length, 2);
  do_check_eq(getPlacesInfoResult.errors.length, 0);

  do_check_eq(getPlacesInfoResult.results[0].uri.spec, placeInfo1.uri.spec);
  do_check_eq(getPlacesInfoResult.results[1].guid, placeInfo2.guid);
}
add_task(test_mixed_selection);

function run_test() {
  run_next_test();
}
