/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/ExtensionSearchHandler.jsm");

add_task(function* test_correct_errors_are_thrown() {
  let keyword = "foo";
  let anotherKeyword = "bar";
  let unregisteredKeyword = "baz";

  // Register a keyword.
  ExtensionSearchHandler.registerKeyword(keyword, { emit: () => {} });

  // Try registering the keyword again.
  Assert.throws(() => ExtensionSearchHandler.registerKeyword(keyword, { emit: () => {} }));

  // Register a different keyword.
  ExtensionSearchHandler.registerKeyword(anotherKeyword, { emit: () => {} });

  // Try calling handleSearch for an unregistered keyword.
  Assert.throws(() => ExtensionSearchHandler.handleSearch(unregisteredKeyword, `${unregisteredKeyword} `, () => {}));

  // Try calling handleSearch without a callback.
  Assert.throws(() => ExtensionSearchHandler.handleSearch(unregisteredKeyword, `${unregisteredKeyword} `));

  // Try getting the description for a keyword which isn't registered.
  Assert.throws(() => ExtensionSearchHandler.getDescription(unregisteredKeyword));

  // Try getting the extension name for a keyword which isn't registered.
  Assert.throws(() => ExtensionSearchHandler.getExtensionName(unregisteredKeyword));

  // Try setting the default suggestion for a keyword which isn't registered.
  Assert.throws(() => ExtensionSearchHandler.setDefaultSuggestion(unregisteredKeyword, "suggestion"));

  // Try calling handleInputCancelled when there is no active session.
  Assert.throws(() => ExtensionSearchHandler.handleInputCancelled());

  // Start a session by calling handleSearch with the registered keyword.
  ExtensionSearchHandler.handleSearch(keyword, `${keyword} test`, () => {});

  // Try calling handleSearch for an inactive keyword
  Assert.throws(() => ExtensionSearchHandler.handleSearch(anotherKeyword, `${anotherKeyword} `, () => {}));

  // Try providing suggestions for an unregistered keyword.
  Assert.throws(() => ExtensionSearchHandler.addSuggestions(unregisteredKeyword, 1, []));

  // Try providing suggestions for an inactive keyword.
  Assert.throws(() => ExtensionSearchHandler.addSuggestions(anotherKeyword, 1, []));

  // Try providing suggestions with an inactive callback ID.
  Assert.throws(() => ExtensionSearchHandler.addSuggestions(keyword, 0, []));

  // Try providing suggestions with another inactive callback ID.
  Assert.throws(() => ExtensionSearchHandler.addSuggestions(keyword, 2, []));

  ExtensionSearchHandler.addSuggestions(keyword, 1, []);

  // End the input session by calling handleInputCancelled.
  ExtensionSearchHandler.handleInputCancelled();

  // Try handling input after the session has ended using handleInputCancelled.
  Assert.throws(() => ExtensionSearchHandler.handleInputCancelled());

  // Start a new session by calling handleSearch with a different keyword
  ExtensionSearchHandler.handleSearch(anotherKeyword, `${anotherKeyword} test`, () => {});

  // Set the default suggestion.
  ExtensionSearchHandler.setDefaultSuggestion(anotherKeyword, {description: "test result"});

  // End the session by calling handleInputEntered.
  ExtensionSearchHandler.handleInputEntered(anotherKeyword, `${anotherKeyword} test`, "tab");

  // Try handling input after the session has ended using handleInputCancelled.
  Assert.throws(() => ExtensionSearchHandler.handleInputCancelled());

  // Unregister the keyword.
  ExtensionSearchHandler.unregisterKeyword(keyword);

  // Try setting the default suggestion for the unregistered keyword.
  Assert.throws(() => ExtensionSearchHandler.setDefaultSuggestion(keyword, {description: "test"}));

  // Try handling a search with the unregistered keyword.
  Assert.throws(() => ExtensionSearchHandler.handleSearch(keyword, `${keyword} test`, () => {}));

  // Try unregistering the keyword again.
  Assert.throws(() => ExtensionSearchHandler.unregisterKeyword(keyword));

  // Unregister the other keyword.
  ExtensionSearchHandler.unregisterKeyword(anotherKeyword);

  // Try unregistering the word which was never registered.
  Assert.throws(() => ExtensionSearchHandler.unregisterKeyword(unregisteredKeyword));

  // Try setting the default suggestion for a word that was never registered.
  Assert.throws(() => ExtensionSearchHandler.setDefaultSuggestion(unregisteredKeyword, {description: "test"}));

  yield cleanup();
});

add_task(function* test_correct_events_are_emitted() {
  let events = [];
  function checkEvents(expectedEvents) {
    Assert.equal(events.length, expectedEvents.length, "The correct number of events fired");
    expectedEvents.forEach((e, i) => Assert.equal(e, events[i], `Expected "${e}" event to fire`));
    events = [];
  }

  let mockExtension = { emit: message => events.push(message) };

  let keyword = "foo";
  let anotherKeyword = "bar";

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);
  ExtensionSearchHandler.registerKeyword(anotherKeyword, mockExtension);

  ExtensionSearchHandler.handleSearch(keyword, `${keyword} `, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_STARTED]);

  ExtensionSearchHandler.handleSearch(keyword, `${keyword} f`, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_CHANGED]);

  ExtensionSearchHandler.handleInputEntered(keyword, `${keyword} f`, "tab");
  checkEvents([ExtensionSearchHandler.MSG_INPUT_ENTERED]);

  ExtensionSearchHandler.handleSearch(keyword, `${keyword} f`, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_STARTED, ExtensionSearchHandler.MSG_INPUT_CHANGED]);

  ExtensionSearchHandler.handleInputCancelled();
  checkEvents([ExtensionSearchHandler.MSG_INPUT_CANCELLED]);

  ExtensionSearchHandler.handleSearch(anotherKeyword, `${anotherKeyword} baz`, () => {});
  checkEvents([ExtensionSearchHandler.MSG_INPUT_STARTED, ExtensionSearchHandler.MSG_INPUT_CHANGED]);

  ExtensionSearchHandler.handleInputEntered(keyword, `${anotherKeyword} baz`, "tab");
  checkEvents([ExtensionSearchHandler.MSG_INPUT_ENTERED]);

  ExtensionSearchHandler.unregisterKeyword(keyword);
});

add_task(function* test_removes_suggestion_if_its_content_is_typed_in() {
  let keyword = "test";
  let extensionName = "Foo Bar";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          {content: "foo", description: "first suggestion"},
          {content: "bar", description: "second suggestion"},
          {content: "baz", description: "third suggestion"},
        ]);
      }
    }
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  yield check_autocomplete({
    search: `${keyword} `,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} `}),
    ]
  });

  yield check_autocomplete({
    search: `${keyword} unmatched`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} unmatched`}),
      makeExtensionMatch({keyword, content: `${keyword} foo`, description: "first suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} bar`, description: "second suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} baz`, description: "third suggestion"})
    ]
  });

  yield check_autocomplete({
    search: `${keyword} foo`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} foo`}),
      makeExtensionMatch({keyword, content: `${keyword} bar`, description: "second suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} baz`, description: "third suggestion"})
    ]
  });

  yield check_autocomplete({
    search: `${keyword} bar`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} bar`}),
      makeExtensionMatch({keyword, content: `${keyword} foo`, description: "first suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} baz`, description: "third suggestion"})
    ]
  });

  yield check_autocomplete({
    search: `${keyword} baz`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} baz`}),
      makeExtensionMatch({keyword, content: `${keyword} foo`, description: "first suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} bar`, description: "second suggestion"})
    ]
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  yield cleanup();
});

add_task(function* test_extension_results_should_come_first() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let uri = NetUtil.newURI(`http://a.com/b`);
  yield PlacesTestUtils.addVisits([
    { uri, title: `${keyword} -` },
  ]);

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          {content: "foo", description: "first suggestion"},
          {content: "bar", description: "second suggestion"},
          {content: "baz", description: "third suggestion"},
        ]);
      }
    }
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  // Start an input session before testing MSG_INPUT_CHANGED.
  ExtensionSearchHandler.handleSearch(keyword, `${keyword} `, () => {});

  yield check_autocomplete({
    search: `${keyword} -`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} -`}),
      makeExtensionMatch({keyword, content: `${keyword} foo`, description: "first suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} bar`, description: "second suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} baz`, description: "third suggestion"}),
      { uri, title: `${keyword} -` }
    ]
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  yield cleanup();
});

add_task(function* test_setting_the_default_suggestion() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let mockExtension = {
    name: extensionName,
    emit() {}
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  ExtensionSearchHandler.setDefaultSuggestion(keyword, {
    description: "hello world"
  });

  let searchString = `${keyword} search query`;
  yield check_autocomplete({
    search: searchString,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: "hello world", content: searchString}),
    ]
  });

  ExtensionSearchHandler.setDefaultSuggestion(keyword, {
    description: "foo bar"
  });

  yield check_autocomplete({
    search: searchString,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: "foo bar", content: searchString}),
    ]
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  yield cleanup();
});

add_task(function* test_maximum_number_of_suggestions_is_enforced() {
  let keyword = "test";
  let extensionName = "Omnibox Example";

  let mockExtension = {
    name: extensionName,
    emit(message, text, id) {
      if (message === ExtensionSearchHandler.MSG_INPUT_CHANGED) {
        ExtensionSearchHandler.addSuggestions(keyword, id, [
          {content: "a", description: "first suggestion"},
          {content: "b", description: "second suggestion"},
          {content: "c", description: "third suggestion"},
          {content: "d", description: "fourth suggestion"},
          {content: "e", description: "fifth suggestion"},
          {content: "f", description: "sixth suggestion"},
          {content: "g", description: "seventh suggestion"},
          {content: "h", description: "eigth suggestion"},
          {content: "i", description: "ninth suggestion"},
          {content: "j", description: "tenth suggestion"},
        ]);
      }
    }
  };

  ExtensionSearchHandler.registerKeyword(keyword, mockExtension);

  // Start an input session before testing MSG_INPUT_CHANGED.
  ExtensionSearchHandler.handleSearch(keyword, `${keyword} `, () => {});

  yield check_autocomplete({
    search: `${keyword} #`,
    searchParam: "enable-actions",
    matches: [
      makeExtensionMatch({heuristic: true, keyword, description: extensionName, content: `${keyword} #`}),
      makeExtensionMatch({keyword, content: `${keyword} a`, description: "first suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} b`, description: "second suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} c`, description: "third suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} d`, description: "fourth suggestion"}),
      makeExtensionMatch({keyword, content: `${keyword} e`, description: "fifth suggestion"}),
    ]
  });

  ExtensionSearchHandler.unregisterKeyword(keyword);
  yield cleanup();
});
