/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Testing that we dedupe results that have the same URL and title as another
// except for their prefix (e.g. http://www.).
add_task(async function dedupe_prefix() {
  // We need to set the title or else we won't dedupe. We only dedupe when
  // titles match up to mitigate deduping when the www. version of a site is
  // completely different from it's www-less counterpart and thus presumably
  // has a different title.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "http://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
    {
      uri: "https://www.example.com/foo/",
      title: "Example Page",
    },
  ]);

  // We should get https://www. as the heuristic result and https:// in the
  // results since the latter's prefix is a higher priority.
  await check_autocomplete({
    search: "example.com/foo/",
    autofilled: "example.com/foo/",
    completed: "https://www.example.com/foo/",
    matches: [
      {
        value: "example.com/foo/",
        comment: "https://www.example.com/foo/",
        style: ["autofill", "heuristic"],
      },
      {
        value: "https://example.com/foo/",
        comment: "Example Page",
      },
    ],
  });

  // Add more visits to the lowest-priority prefix. It should be the heuristic
  // result but we should still show our highest-priority result. https://www.
  // should not appear at all.
  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www.example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  await check_autocomplete({
    search: "example.com/foo/",
    autofilled: "example.com/foo/",
    completed: "http://www.example.com/foo/",
    matches: [
      {
        value: "example.com/foo/",
        comment: "www.example.com/foo/",
        style: ["autofill", "heuristic"],
      },
      {
        value: "https://example.com/foo/",
        comment: "Example Page",
      },
    ],
  });

  // Add enough https:// vists for it to have the highest frecency. It should
  // be the heuristic result. We should still get the https://www. result
  // because we still show results with the same key and protocol if they differ
  // from the heuristic result in having www.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://example.com/foo/",
        title: "Example Page",
      },
    ]);
  }

  await check_autocomplete({
    search: "example.com/foo/",
    autofilled: "example.com/foo/",
    completed: "https://example.com/foo/",
    matches: [
      {
        value: "example.com/foo/",
        comment: "https://example.com/foo/",
        style: ["autofill", "heuristic"],
      },
      {
        value: "https://www.example.com/foo/",
        comment: "Example Page",
      },
    ],
  });

  await cleanup();
});
