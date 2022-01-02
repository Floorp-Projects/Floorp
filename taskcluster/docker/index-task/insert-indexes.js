/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let taskcluster = require("taskcluster-client");

// Create instance of index client
let index = new taskcluster.Index({
  delayFactor: 750, // Good solid delay for background process
  retries: 8, // A few extra retries for robustness
  rootUrl:
    process.env.TASKCLUSTER_PROXY_URL || process.env.TASKCLUSTER_ROOT_URL,
});

// Create queue instance for fetching taskId
let queue = new taskcluster.Queue({
  delayFactor: 750, // Good solid delay for background process
  retries: 8, // A few extra retries for robustness
  rootUrl:
    process.env.TASKCLUSTER_PROXY_URL || process.env.TASKCLUSTER_ROOT_URL,
});

// Load input
let taskId = process.env.TARGET_TASKID;
let rank = parseInt(process.env.INDEX_RANK, 10);
let namespaces = process.argv.slice(2);

// Validate input
if (!taskId) {
  console.log("Expected target task as environment variable: TARGET_TASKID");
  process.exit(1);
}

if (isNaN(rank)) {
  console.log("Expected index rank as environment variable: INDEX_RANK");
  process.exit(1);
}

// Fetch task definition to get expiration and then insert into index
queue
  .task(taskId)
  .then(task => task.expires)
  .then(expires => {
    return Promise.all(
      namespaces.map(namespace => {
        console.log(
          "Inserting %s into index (rank %d) under: %s",
          taskId,
          rank,
          namespace
        );
        return index.insertTask(namespace, {
          taskId,
          rank,
          data: {},
          expires,
        });
      })
    );
  })
  .then(() => {
    console.log("indexing successfully completed.");
    process.exit(0);
  })
  .catch(err => {
    console.log("Error:\n%s", err);
    if (err.stack) {
      console.log("Stack:\n%s", err.stack);
    }
    console.log("Properties:\n%j", err);
    throw err;
  })
  .catch(() => process.exit(1));
