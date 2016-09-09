/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var fs = require("fs");
var path = require("path");
var crypto = require("crypto");
var slugid = require("slugid");
var flatmap = require("flatmap");
var taskcluster = require("taskcluster-client");

var yaml = require("./yaml");

// Default values for debugging.
var TC_PROJECT = process.env.TC_PROJECT || "{{tc_project}}";
var NSS_PUSHLOG_ID = process.env.NSS_PUSHLOG_ID || "{{nss_pushlog_id}}";
var NSS_HEAD_REVISION = process.env.NSS_HEAD_REVISION || "{{nss_head_rev}}";

// Add base information to the given task.
function decorateTask(task) {
  // Assign random task id.
  task.taskId = slugid.v4();

  // TreeHerder routes.
  task.task.routes = [
    "tc-treeherder-stage.v2." + TC_PROJECT + "." + NSS_HEAD_REVISION + "." + NSS_PUSHLOG_ID,
    "tc-treeherder.v2." + TC_PROJECT + "." + NSS_HEAD_REVISION + "." + NSS_PUSHLOG_ID
  ];
}

// Compute the SHA-256 digest.
function sha256(data) {
  var hash = crypto.createHash("sha256");
  hash.update(data);
  return hash.digest("hex");
}

// Recursively collect a list of all files of a given directory.
function collectFilesInDirectory(dir) {
  return flatmap(fs.readdirSync(dir), function (entry) {
    var entry_path = path.join(dir, entry);

    if (fs.lstatSync(entry_path).isDirectory()) {
      return collectFilesInDirectory(entry_path);
    }

    return [entry_path];
  });
}

// Compute a context hash for the given context path.
function computeContextHash(context_path) {
  var root = path.join(__dirname, "../../..");
  var dir = path.join(root, context_path);
  var files = collectFilesInDirectory(dir).sort();
  var hashes = files.map(function (file) {
    return sha256(file + "|" + fs.readFileSync(file, "utf-8"));
  });

  return sha256(hashes.join(","));
}

// Generates the image-builder task description.
function generateImageBuilderTask(context_path) {
  var task = yaml.parse(path.join(__dirname, "image_builder.yml"), {});

  // Add base info.
  decorateTask(task);

  // Add info for docker image building.
  task.task.payload.env.CONTEXT_PATH = context_path;
  task.task.payload.env.HASH = computeContextHash(context_path);

  return task;
}

// Returns a Promise<bool> that tells whether the task with the given id
// has a public/image.tar artifact with a ready-to-use docker image.
function asyncTaskHasImageArtifact(taskId) {
  var queue = new taskcluster.Queue();

  return queue.listLatestArtifacts(taskId).then(function (result) {
    return result.artifacts.some(function (artifact) {
      return artifact.name == "public/image.tar";
    });
  }, function () {
    return false;
  });
}

// Returns a Promise<task-id|null> with either a task id or null, depending
// on whether we could find a task in the given namespace with a docker image.
function asyncFindTaskWithImageArtifact(ns) {
  var index = new taskcluster.Index();

  return index.findTask(ns).then(function (result) {
    return asyncTaskHasImageArtifact(result.taskId).then(function (has_image) {
      return has_image ? result.taskId : null;
    });
  }, function () {
    return null;
  });
}

// Tweak the given list of tasks by injecting the image-builder task
// and setting the right dependencies where needed.
function asyncTweakTasks(tasks) {
  var id = "linux";
  var cx_path = "automation/taskcluster/docker";
  var hash = computeContextHash(cx_path);
  var ns = "docker.images.v1." + TC_PROJECT + "." + id + ".hash." + hash;
  var additional_tasks = [];

  // Check whether the docker image was already built.
  return asyncFindTaskWithImageArtifact(ns).then(function (taskId) {
    var builder_task;

    if (!taskId) {
      // No docker image found, add a task to build one.
      builder_task = generateImageBuilderTask(cx_path);
      taskId = builder_task.taskId;

      // Add a route so we can find the task later again.
      builder_task.task.routes.push("index." + ns);
      additional_tasks.push(builder_task);
    }

    tasks.forEach(function (task) {
      if (task.task.payload.image == cx_path) {
        task.task.payload.image = {
          path: "public/image.tar",
          type: "task-image",
          taskId: taskId
        };

        // Add a dependency only for top-level tasks (builds & tools) and only
        // if we added an image building task. Otherwise we don't need to wait.
        if (builder_task && !task.requires) {
          task.requires = [taskId];
        }
      }
    });

    return additional_tasks.concat(tasks);
  });
}

module.exports.asyncTweakTasks = asyncTweakTasks;
