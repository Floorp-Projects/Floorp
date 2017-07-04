/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {clone} from "merge";
import merge from "./merge";
import slugid from "slugid";
import taskcluster from "taskcluster-client";
import * as image_builder from "./image_builder";

let maps = [];
let filters = [];

let tasks = new Map();
let image_tasks = new Map();

let queue = new taskcluster.Queue({
  baseUrl: "http://taskcluster/queue/v1"
});

function fromNow(hours) {
  let d = new Date();
  d.setHours(d.getHours() + (hours|0));
  return d.toJSON();
}

function parseRoutes(routes) {
  let rv = [
    `tc-treeherder.v2.${process.env.TC_PROJECT}.${process.env.NSS_HEAD_REVISION}.${process.env.NSS_PUSHLOG_ID}`,
    ...routes
  ];

  // Notify about failures (except on try).
  if (process.env.TC_PROJECT != "nss-try") {
    rv.push(`notify.email.${process.env.TC_OWNER}.on-failed`,
            `notify.email.${process.env.TC_OWNER}.on-exception`);
  }

  return rv;
}

function parseFeatures(list) {
  return list.reduce((map, feature) => {
    map[feature] = true;
    return map;
  }, {});
}

function parseArtifacts(artifacts) {
  let copy = clone(artifacts);
  Object.keys(copy).forEach(key => {
    copy[key].expires = fromNow(copy[key].expires);
  });
  return copy;
}

function parseCollection(name) {
  let collection = {};
  collection[name] = true;
  return collection;
}

function parseTreeherder(def) {
  let treeherder = {
    build: {
      platform: def.platform
    },
    machine: {
      platform: def.platform
    },
    symbol: def.symbol,
    jobKind: def.kind
  };

  if (def.group) {
    treeherder.groupSymbol = def.group;
  }

  if (def.collection) {
    treeherder.collection = parseCollection(def.collection);
  }

  if (def.tier) {
    treeherder.tier = def.tier;
  }

  return treeherder;
}

function convertTask(def) {
  let scopes = [];
  let dependencies = [];

  let env = merge({
    NSS_HEAD_REPOSITORY: process.env.NSS_HEAD_REPOSITORY,
    NSS_HEAD_REVISION: process.env.NSS_HEAD_REVISION
  }, def.env || {});

  if (def.parent) {
    dependencies.push(def.parent);
    env.TC_PARENT_TASK_ID = def.parent;
  }

  if (def.tests) {
    env.NSS_TESTS = def.tests;
  }

  if (def.cycle) {
    env.NSS_CYCLES = def.cycle;
  }

  let payload = {
    env,
    command: def.command,
    maxRunTime: def.maxRunTime || 3600
  };

  if (def.image) {
    payload.image = def.image;
  }

  if (def.artifacts) {
    payload.artifacts = parseArtifacts(def.artifacts);
  }

  if (def.features) {
    payload.features = parseFeatures(def.features);

    if (payload.features.allowPtrace) {
      scopes.push("docker-worker:feature:allowPtrace");
    }
  }

  return {
    provisionerId: def.provisioner || "aws-provisioner-v1",
    workerType: def.workerType || "hg-worker",
    schedulerId: "task-graph-scheduler",

    scopes,
    created: fromNow(0),
    deadline: fromNow(24),

    dependencies,
    routes: parseRoutes(def.routes || []),

    metadata: {
      name: def.name,
      description: def.name,
      owner: process.env.TC_OWNER,
      source: process.env.TC_SOURCE
    },

    payload,

    extra: {
      treeherder: parseTreeherder(def)
    }
  };
}

export function map(fun) {
  maps.push(fun);
}

export function filter(fun) {
  filters.push(fun);
}

export function scheduleTask(def) {
  let taskId = slugid.v4();
  tasks.set(taskId, merge({}, def));
  return taskId;
}

export async function submit() {
  let promises = new Map();

  for (let [taskId, task] of tasks) {
    // Allow filtering tasks before we schedule them.
    if (!filters.every(filter => filter(task))) {
      continue;
    }

    // Allow changing tasks before we schedule them.
    maps.forEach(map => { task = map(merge({}, task)) });

    let log_id = `${task.name} @ ${task.platform}[${task.collection || "opt"}]`;
    console.log(`+ Submitting ${log_id}.`);

    let parent = task.parent;

    // Convert the task definition.
    task = await convertTask(task);

    // Convert the docker image definition.
    let image_def = task.payload.image;
    if (image_def && image_def.hasOwnProperty("path")) {
      let key = `${image_def.name}:${image_def.path}`;
      let data = {};

      // Check the cache first.
      if (image_tasks.has(key)) {
        data = image_tasks.get(key);
      } else {
        data.taskId = await image_builder.findTask(image_def);
        data.isPending = !data.taskId;

        // No task found.
        if (data.isPending) {
          let image_task = await image_builder.buildTask(image_def);

          // Schedule a new image builder task immediately.
          data.taskId = slugid.v4();

          try {
            await queue.createTask(data.taskId, convertTask(image_task));
          } catch (e) {
            console.error("! FAIL: Scheduling image builder task failed.");
            continue; /* Skip this task on failure. */
          }
        }

        // Store in cache.
        image_tasks.set(key, data);
      }

      if (data.isPending) {
        task.dependencies.push(data.taskId);
      }

      task.payload.image = {
        path: "public/image.tar",
        taskId: data.taskId,
        type: "task-image"
      };
    }

    // Wait for the parent task to be created before scheduling dependants.
    let predecessor = parent ? promises.get(parent) : Promise.resolve();

    promises.set(taskId, predecessor.then(() => {
      // Schedule the task.
      return queue.createTask(taskId, task).catch(err => {
        console.error(`! FAIL: Scheduling ${log_id} failed.`, err);
      });
    }));
  }

  // Wait for all requests to finish.
  if (promises.length) {
    await Promise.all([...promises.values()]);
    console.log("=== Total:", promises.length, "tasks. ===");
  }

  tasks.clear();
}
