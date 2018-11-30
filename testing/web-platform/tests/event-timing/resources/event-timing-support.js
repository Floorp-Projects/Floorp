function clickOnElement(id, resolve) {
  const element = document.getElementById(id);
  const clickHandler = () => {
    element.removeEventListener("click", clickHandler);
    resolve();
  };
  element.addEventListener("click", clickHandler);
  test_driver.click(element);
}

function mainThreadBusy(duration) {
  const now = performance.now();
  while (performance.now() < now + duration);
}

// This method should receive an entry of type 'event'. |is_first| is true only
// when the event also happens to correspond to the first event. In this case,
// the timings of the 'firstInput' entry should be equal to those of this entry.
function verifyClickEvent(entry, is_first=false) {
  assert_true(entry.cancelable);
  assert_equals(entry.name, 'click');
  assert_equals(entry.entryType, 'event');
  assert_greater_than(entry.duration, 50,
      "The entry's duration should be greater than 50ms.");
  assert_greater_than(entry.processingStart, entry.startTime,
      "The entry's processingStart should be greater than startTime.");
  assert_greater_than_equal(entry.processingEnd, entry.processingStart,
      "The entry's processingEnd must be at least as large as processingStart.");
  assert_greater_than_equal(entry.duration, entry.processingEnd - entry.startTime,
      "The entry's duration must be at least as large as processingEnd - startTime.");
  if (is_first) {
    let firstInputs = performance.getEntriesByType('firstInput');
    assert_equals(firstInputs.length, 1, 'There should be a single firstInput entry');
    let firstInput = firstInputs[0];
    assert_equals(firstInput.name, entry.name);
    assert_equals(firstInput.entryType, 'firstInput');
    assert_equals(firstInput.startTime, entry.startTime);
    assert_equals(firstInput.duration, entry.duration);
    assert_equals(firstInput.processingStart, entry.processingStart);
    assert_equals(firstInput.processingEnd, entry.processingEnd);
    assert_equals(firstInput.cancelable, entry.cancelable);
  }
}

function wait() {
  return new Promise((resolve, reject) => {
    step_timeout(() => {
      resolve();
    }, 0);
  });
}

function clickAndBlockMain(id) {
  return new Promise((resolve, reject) => {
    clickOnElement(id, resolve);
    mainThreadBusy(300);
  });
}
