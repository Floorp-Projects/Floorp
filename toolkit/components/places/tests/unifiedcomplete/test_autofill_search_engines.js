/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The autoFill.searchEngines pref autofills the domains of engines registered
// with the search service.  That's what this test checks.  It's a different
// path in UnifiedComplete.js from normal moz_places autofill, which is tested
// in test_autofill_origins.js and test_autofill_urls.js.

"use strict";

add_task(async function searchEngines() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);

  let schemes = ["http", "https"];
  for (let i = 0; i < schemes.length; i++) {
    let scheme = schemes[i];
    Services.search.addEngineWithDetails("TestEngine", "", "", "", "GET",
                                         scheme + "://www.example.com/");
    let engine = Services.search.getEngineByName("TestEngine");
    engine.addParam("q", "{searchTerms}", null);

    await check_autocomplete({
      search: "ex",
      autofilled: "example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: "example.com",
      autofilled: "example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: "example.com/",
      autofilled: "example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: "www.ex",
      autofilled: "www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: "www.example.com",
      autofilled: "www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: "www.example.com/",
      autofilled: "www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: "www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://ex",
      autofilled: scheme + "://example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://example.com",
      autofilled: scheme + "://example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://example.com/",
      autofilled: scheme + "://example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://www.ex",
      autofilled: scheme + "://www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://www.example.com",
      autofilled: scheme + "://www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    await check_autocomplete({
      search: scheme + "://www.example.com/",
      autofilled: scheme + "://www.example.com/",
      completed: scheme + "://www.example.com/",
      matches: [{
        value: scheme + "://www.example.com/",
        comment: "TestEngine",
        style: ["heuristic", "priority-search"],
      }],
    });

    let otherScheme = schemes[(i + 1) % schemes.length];
    await check_autocomplete({
      search: otherScheme + "://ex",
      matches: [],
    });
    await check_autocomplete({
      search: otherScheme + "://www.ex",
      matches: [],
    });

    Services.search.removeEngine(engine);
  }

  await cleanup();
});
