'use strict';

// Creates a <div> element, appends it to the document body and
// removes the created element during test cleanup.
function createDiv(test, doc) {
  return createElement(test, 'div', doc);
}

// Creates an element with the given |tagName|, appends it to the document body
// and removes the created element during test cleanup.
// If |tagName| is null or undefined, creates a <div> element.
function createElement(test, tagName, doc) {
  if (!doc) {
    doc = document;
  }
  var element = doc.createElement(tagName || 'div');
  doc.body.appendChild(element);
  test.add_cleanup(function() {
    element.remove();
  });
  return element;
}
