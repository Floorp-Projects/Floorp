/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as queue from "./queue";
import context_hash from "./context_hash";
import taskcluster from "taskcluster-client";

async function taskHasImageArtifact(taskId) {
  let queue = new taskcluster.Queue();
  let {artifacts} = await queue.listLatestArtifacts(taskId);
  return artifacts.some(artifact => artifact.name == "public/image.tar");
}

async function findTaskWithImageArtifact(ns) {
  let index = new taskcluster.Index();
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
    name: "Image Builder",
    image: "taskcluster/image_builder:0.1.5",
    routes: ["index." + ns],
    env: {
      HEAD_REPOSITORY: process.env.NSS_HEAD_REPOSITORY,
      BASE_REPOSITORY: process.env.NSS_HEAD_REPOSITORY,
      HEAD_REV: process.env.NSS_HEAD_REVISION,
      HEAD_REF: process.env.NSS_HEAD_REVISION,
      PROJECT: process.env.TC_PROJECT,
      CONTEXT_PATH: path,
      HASH: hash
    },
    artifacts: {
      "public/image.tar": {
        type: "file",
        expires: 24 * 365,
        path: "/artifacts/image.tar"
      }
    },
    command: [
      "/bin/bash",
      "-c",
      "/home/worker/bin/build_image.sh"
    ],
    platform: "nss-decision",
    features: ["dind"],
    kind: "build",
    symbol: "I"
  };
}
