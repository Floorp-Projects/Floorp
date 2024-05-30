/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as queue from "./queue";
import context_hash from "./context_hash";
import taskcluster from "taskcluster-client";

const fs = require("fs");
const tar = require("tar");

async function taskHasImageArtifact(taskId) {
  let queue = new taskcluster.Queue(taskcluster.fromEnvVars());
  let {artifacts} = await queue.listLatestArtifacts(taskId);
  return artifacts.some(artifact => artifact.name == "public/image.tar.zst");
}

async function findTaskWithImageArtifact(ns) {
  let index = new taskcluster.Index(taskcluster.fromEnvVars());
  let {taskId} = await index.findTask(ns);
  let has_image = await taskHasImageArtifact(taskId);
  return has_image ? taskId : null;
}

export async function findTask({name, path}) {
  let hash = await context_hash(path);
  let ns = `docker.images.v1.${process.env.TC_PROJECT}.${name}.hash.${hash}`;
  return findTaskWithImageArtifact(ns).catch(() => null);
}

export async function buildTask({name, path}) {
  let hash = await context_hash(path);
  let ns = `docker.images.v1.${process.env.TC_PROJECT}.${name}.hash.${hash}`;
  let fullPath = "/home/worker/nss/" + path
  let contextName = name + ".tar.gz";
  let contextRoot = "/home/worker/docker-contexts/";
  let contextPath = contextRoot + contextName;

  if (!fs.existsSync(contextRoot)) {
    fs.mkdirSync(contextRoot);
  }

  await tar.create({gzip: true, file: contextPath, cwd: fullPath}, ["."]);

  return {
    name: `Image Builder (${name})`,
    image: "mozillareleases/image_builder:5.0.0",
    workerType: "images-gcp",
    routes: ["index." + ns],
    env: {
      IMAGE_NAME: name,
      CONTEXT_PATH: "public/docker-contexts/" + contextName,
      CONTEXT_TASK_ID: process.env.TASK_ID,
      HASH: hash
    },
    artifacts: {
      "public/image.tar.zst": {
        type: "file",
        expires: 24 * 90,
        path: "/workspace/image.tar.zst"
      }
    },
    platform: "nss-decision",
    features: ["allowPtrace", "chainOfTrust"],
    maxRunTime: 7200,
    kind: "build",
    symbol: `I(${name})`
  };
}
