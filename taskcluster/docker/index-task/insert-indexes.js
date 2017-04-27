let taskcluster = require("taskcluster-client");

// Create instance of index client
let index = new taskcluster.Index({
  delayFactor:    750,  // Good solid delay for background process
  retries:        8,    // A few extra retries for robustness
  baseUrl:        "taskcluster/index/v1",
});

// Create queue instance for fetching taskId
let queue = new taskcluster.Queue({
    delayFactor:    750,  // Good solid delay for background process
    retries:        8,    // A few extra retries for robustness
});

// Load input
let taskId = process.env.TARGET_TASKID;
let namespaces = process.argv.slice(2);

// Validate input
if (!taskId) {
  console.log("Expected target task as environment variable: TARGET_TASKID");
  process.exit(1);
}

// Fetch task definition to get expiration and then insert into index
queue.task(taskId).then(task => task.expires).then(expires => {
  return Promise.all(namespaces.map(namespace => {
    console.log("Inserting %s into index under: %s", taskId, namespace);
    return index.insertTask(namespace, {
      taskId,
      rank: 0,
      data: {},
      expires,
    });
  }));
}).then(() => {
  console.log("indexing successfully completed.");
  process.exit(0);
}).catch(err => {
  console.log("Error:\n%s", err);
  if (err.stack) {
    console.log("Stack:\n%s", err.stack);
  }
  console.log("Properties:\n%j", err);
  throw err;
}).catch(() => process.exit(1));
