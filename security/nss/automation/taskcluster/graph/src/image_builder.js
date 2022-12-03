/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as queue from "./queue";
import context_hash from "./context_hash";
import taskcluster from "taskcluster-client";

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

  return {
    name: `Image Builder (${name})`,
    image: "nssdev/image_builder:0.1.5",
    routes: ["index." + ns],
    env: {
      NSS_HEAD_REPOSITORY: process.env.NSS_HEAD_REPOSITORY,
      NSS_HEAD_REVISION: process.env.NSS_HEAD_REVISION,
      PROJECT: process.env.TC_PROJECT,
      CONTEXT_PATH: path,
      HASH: hash
    },
    artifacts: {
      "public/image.tar.zst": {
        type: "file",
        expires: 24 * 90,
        path: "/artifacts/image.tar.zst"
      }
    },
    command: [
      "/bin/bash",
      "-c",
      "bin/checkout.sh && nss/automation/taskcluster/scripts/build_image.sh"
    ],
    platform: "nss-decision",
    features: ["dind"],
    maxRunTime: 7200,
    kind: "build",
    symbol: "I"
  };
}
