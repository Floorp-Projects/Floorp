add_task(function* test_encoded() {
  do_print("Searching for over encoded url should not break it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "https://www.mozilla.com/search/top/?q=%25%32%35",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  yield cleanup();
});

add_task(function* test_encoded_trimmed() {
  do_print("Searching for over encoded url should not break it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "mozilla.com/search/top/?q=%25%32%35",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  yield cleanup();
});

add_task(function* test_encoded_partial() {
  do_print("Searching for over encoded url should not break it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "https://www.mozilla.com/search/top/?q=%25",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  yield cleanup();
});

add_task(function* test_encoded_path() {
  do_print("Searching for over encoded url should not break it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/%25%32%35/top/"),
    title: "https://www.mozilla.com/%25%32%35/top/",
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "https://www.mozilla.com/%25%32%35/t",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/%25%32%35/top/"),
                 title: "https://www.mozilla.com/%25%32%35/top/",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/%25%32%35/top/",
    completed: "https://www.mozilla.com/%25%32%35/top/"
  });
  yield cleanup();
});
