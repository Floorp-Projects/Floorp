add_task(async function test_encoded() {
  info("Searching for over encoded url should not break it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://www.mozilla.com/search/top/?q=%25%32%35",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  await cleanup();
});

add_task(async function test_encoded_trimmed() {
  info("Searching for over encoded url should not break it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.com/search/top/?q=%25%32%35",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  await cleanup();
});

add_task(async function test_encoded_partial() {
  info("Searching for over encoded url should not break it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
    title: "https://www.mozilla.com/search/top/?q=%25%32%35",
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://www.mozilla.com/search/top/?q=%25",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/search/top/?q=%25%32%35"),
                 title: "https://www.mozilla.com/search/top/?q=%25%32%35",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/search/top/?q=%25%32%35",
    completed: "https://www.mozilla.com/search/top/?q=%25%32%35"
  });
  await cleanup();
});

add_task(async function test_encoded_path() {
  info("Searching for over encoded url should not break it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("https://www.mozilla.com/%25%32%35/top/"),
    title: "https://www.mozilla.com/%25%32%35/top/",
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "https://www.mozilla.com/%25%32%35/t",
    matches: [ { uri: NetUtil.newURI("https://www.mozilla.com/%25%32%35/top/"),
                 title: "https://www.mozilla.com/%25%32%35/top/",
                 style: [ "autofill", "heuristic" ] }],
    autofilled: "https://www.mozilla.com/%25%32%35/top/",
    completed: "https://www.mozilla.com/%25%32%35/top/"
  });
  await cleanup();
});
