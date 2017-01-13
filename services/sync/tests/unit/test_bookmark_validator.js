/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://services-sync/bookmark_validator.js");
Components.utils.import("resource://services-sync/util.js");

function inspectServerRecords(data) {
  return new BookmarkValidator().inspectServerRecords(data);
}

add_test(function test_isr_rootOnServer() {
  let c = inspectServerRecords([{
    id: 'places',
    type: 'folder',
    children: [],
  }]);
  ok(c.problemData.rootOnServer);
  run_next_test();
});

add_test(function test_isr_empty() {
  let c = inspectServerRecords([]);
  ok(!c.problemData.rootOnServer);
  notEqual(c.root, null);
  run_next_test();
});

add_test(function test_isr_cycles() {
  let c = inspectServerRecords([
    {id: 'C', type: 'folder', children: ['A', 'B'], parentid: 'places'},
    {id: 'A', type: 'folder', children: ['B'], parentid: 'B'},
    {id: 'B', type: 'folder', children: ['A'], parentid: 'A'},
  ]).problemData;

  equal(c.cycles.length, 1);
  ok(c.cycles[0].indexOf('A') >= 0);
  ok(c.cycles[0].indexOf('B') >= 0);
  run_next_test();
});

add_test(function test_isr_orphansMultiParents() {
  let c = inspectServerRecords([
    { id: 'A', type: 'bookmark', parentid: 'D' },
    { id: 'B', type: 'folder', parentid: 'places', children: ['A']},
    { id: 'C', type: 'folder', parentid: 'places', children: ['A']},

  ]).problemData;
  deepEqual(c.orphans, [{ id: "A", parent: "D" }]);
  equal(c.multipleParents.length, 1)
  ok(c.multipleParents[0].parents.indexOf('B') >= 0);
  ok(c.multipleParents[0].parents.indexOf('C') >= 0);
  run_next_test();
});

add_test(function test_isr_orphansMultiParents2() {
  let c = inspectServerRecords([
    { id: 'A', type: 'bookmark', parentid: 'D' },
    { id: 'B', type: 'folder', parentid: 'places', children: ['A']},
  ]).problemData;
  equal(c.orphans.length, 1);
  equal(c.orphans[0].id, 'A');
  equal(c.multipleParents.length, 0);
  run_next_test();
});

add_test(function test_isr_deletedParents() {
  let c = inspectServerRecords([
    { id: 'A', type: 'bookmark', parentid: 'B' },
    { id: 'B', type: 'folder', parentid: 'places', children: ['A']},
    { id: 'B', type: 'item', deleted: true},
  ]).problemData;
  deepEqual(c.deletedParents, ['A'])
  run_next_test();
});

add_test(function test_isr_badChildren() {
  let c = inspectServerRecords([
    { id: 'A', type: 'bookmark', parentid: 'places', children: ['B', 'C'] },
    { id: 'C', type: 'bookmark', parentid: 'A' }
  ]).problemData;
  deepEqual(c.childrenOnNonFolder, ['A'])
  deepEqual(c.missingChildren, [{parent: 'A', child: 'B'}]);
  deepEqual(c.parentNotFolder, ['C']);
  run_next_test();
});


add_test(function test_isr_parentChildMismatches() {
  let c = inspectServerRecords([
    { id: 'A', type: 'folder', parentid: 'places', children: [] },
    { id: 'B', type: 'bookmark', parentid: 'A' }
  ]).problemData;
  deepEqual(c.parentChildMismatches, [{parent: 'A', child: 'B'}]);
  run_next_test();
});

add_test(function test_isr_duplicatesAndMissingIDs() {
  let c = inspectServerRecords([
    {id: 'A', type: 'folder', parentid: 'places', children: []},
    {id: 'A', type: 'folder', parentid: 'places', children: []},
    {type: 'folder', parentid: 'places', children: []}
  ]).problemData;
  equal(c.missingIDs, 1);
  deepEqual(c.duplicates, ['A']);
  run_next_test();
});

add_test(function test_isr_duplicateChildren()  {
  let c = inspectServerRecords([
    {id: 'A', type: 'folder', parentid: 'places', children: ['B', 'B']},
    {id: 'B', type: 'bookmark', parentid: 'A'},
  ]).problemData;
  deepEqual(c.duplicateChildren, ['A']);
  run_next_test();
});

// Each compareServerWithClient test mutates these, so we can't just keep them
// global
function getDummyServerAndClient() {
  let server = [
    {
      id: 'menu',
      parentid: 'places',
      type: 'folder',
      parentName: '',
      title: 'foo',
      children: ['bbbbbbbbbbbb', 'cccccccccccc']
    },
    {
      id: 'bbbbbbbbbbbb',
      type: 'bookmark',
      parentid: 'menu',
      parentName: 'foo',
      title: 'bar',
      bmkUri: 'http://baz.com'
    },
    {
      id: 'cccccccccccc',
      parentid: 'menu',
      parentName: 'foo',
      title: '',
      type: 'query',
      bmkUri: 'place:type=6&sort=14&maxResults=10'
    }
  ];

  let client = {
    "guid": "root________",
    "title": "",
    "id": 1,
    "type": "text/x-moz-place-container",
    "children": [
      {
        "guid": "menu________",
        "title": "foo",
        "id": 1000,
        "type": "text/x-moz-place-container",
        "children": [
          {
            "guid": "bbbbbbbbbbbb",
            "title": "bar",
            "id": 1001,
            "type": "text/x-moz-place",
            "uri": "http://baz.com"
          },
          {
            "guid": "cccccccccccc",
            "title": "",
            "id": 1002,
            "annos": [{
              "name": "Places/SmartBookmark",
              "flags": 0,
              "expires": 4,
              "value": "RecentTags"
            }],
            "type": "text/x-moz-place",
            "uri": "place:type=6&sort=14&maxResults=10"
          }
        ]
      }
    ]
  };
  return {server, client};
}


add_test(function test_cswc_valid() {
  let {server, client} = getDummyServerAndClient();

  let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
  equal(c.clientMissing.length, 0);
  equal(c.serverMissing.length, 0);
  equal(c.differences.length, 0);
  run_next_test();
});

add_test(function test_cswc_serverMissing() {
  let {server, client} = getDummyServerAndClient();
  // remove c
  server.pop();
  server[0].children.pop();

  let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
  deepEqual(c.serverMissing, ['cccccccccccc']);
  equal(c.clientMissing.length, 0);
  deepEqual(c.structuralDifferences, [{id: 'menu', differences: ['childGUIDs']}]);
  run_next_test();
});

add_test(function test_cswc_clientMissing() {
  let {server, client} = getDummyServerAndClient();
  client.children[0].children.pop();

  let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
  deepEqual(c.clientMissing, ['cccccccccccc']);
  equal(c.serverMissing.length, 0);
  deepEqual(c.structuralDifferences, [{id: 'menu', differences: ['childGUIDs']}]);
  run_next_test();
});

add_test(function test_cswc_differences() {
  {
    let {server, client} = getDummyServerAndClient();
    client.children[0].children[0].title = 'asdf';
    let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
    equal(c.clientMissing.length, 0);
    equal(c.serverMissing.length, 0);
    deepEqual(c.differences, [{id: 'bbbbbbbbbbbb', differences: ['title']}]);
  }

  {
    let {server, client} = getDummyServerAndClient();
    server[2].type = 'bookmark';
    let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
    equal(c.clientMissing.length, 0);
    equal(c.serverMissing.length, 0);
    deepEqual(c.differences, [{id: 'cccccccccccc', differences: ['type']}]);
  }
  run_next_test();
});

add_test(function test_cswc_serverUnexpected() {
  let {server, client} = getDummyServerAndClient();
  client.children.push({
    "guid": "dddddddddddd",
    "title": "",
    "id": 2000,
    "annos": [{
      "name": "places/excludeFromBackup",
      "flags": 0,
      "expires": 4,
      "value": 1
    }, {
      "name": "PlacesOrganizer/OrganizerFolder",
      "flags": 0,
      "expires": 4,
      "value": 7
    }],
    "type": "text/x-moz-place-container",
    "children": [{
      "guid": "eeeeeeeeeeee",
      "title": "History",
      "annos": [{
        "name": "places/excludeFromBackup",
        "flags": 0,
        "expires": 4,
        "value": 1
      }, {
        "name": "PlacesOrganizer/OrganizerQuery",
        "flags": 0,
        "expires": 4,
        "value": "History"
      }],
      "type": "text/x-moz-place",
      "uri": "place:type=3&sort=4"
    }]
  });
  server.push({
    id: 'dddddddddddd',
    parentid: 'places',
    parentName: '',
    title: '',
    type: 'folder',
    children: ['eeeeeeeeeeee']
  }, {
    id: 'eeeeeeeeeeee',
    parentid: 'dddddddddddd',
    parentName: '',
    title: 'History',
    type: 'query',
    bmkUri: 'place:type=3&sort=4'
  });

  let c = new BookmarkValidator().compareServerWithClient(server, client).problemData;
  equal(c.clientMissing.length, 0);
  equal(c.serverMissing.length, 0);
  equal(c.serverUnexpected.length, 2);
  deepEqual(c.serverUnexpected, ["dddddddddddd", "eeeeeeeeeeee"]);
  run_next_test();
});

function validationPing(server, client, duration) {
  return wait_for_ping(function() {
    // fake this entirely
    Svc.Obs.notify("weave:service:sync:start");
    Svc.Obs.notify("weave:engine:sync:start", null, "bookmarks");
    Svc.Obs.notify("weave:engine:sync:finish", null, "bookmarks");
    let validator = new BookmarkValidator();
    let data = {
      // We fake duration and version just so that we can verify they're passed through.
      duration,
      version: validator.version,
      recordCount: server.length,
      problems: validator.compareServerWithClient(server, client).problemData,
    };
    Svc.Obs.notify("weave:engine:validate:finish", data, "bookmarks");
    Svc.Obs.notify("weave:service:sync:finish");
  }, true); // Allow "failing" pings, since having validation info indicates failure.
}

add_task(async function test_telemetry_integration() {
  let {server, client} = getDummyServerAndClient();
  // remove "c"
  server.pop();
  server[0].children.pop();
  const duration = 50;
  let ping = await validationPing(server, client, duration);
  ok(ping.engines);
  let bme = ping.engines.find(e => e.name === "bookmarks");
  ok(bme);
  ok(bme.validation);
  ok(bme.validation.problems)
  equal(bme.validation.checked, server.length);
  equal(bme.validation.took, duration);
  bme.validation.problems.sort((a, b) => String(a.name).localeCompare(b.name));
  equal(bme.validation.version, new BookmarkValidator().version);
  deepEqual(bme.validation.problems, [
    { name: "badClientRoots", count: 3 },
    { name: "sdiff:childGUIDs", count: 1 },
    { name: "serverMissing", count: 1 },
    { name: "structuralDifferences", count: 1 },
  ]);
});

function run_test() {
  run_next_test();
}
