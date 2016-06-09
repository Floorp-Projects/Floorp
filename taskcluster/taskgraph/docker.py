# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import subprocess
import tarfile
import urllib2

from taskgraph.util import docker

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))
INDEX_URL = 'https://index.taskcluster.net/v1/task/docker.images.v1.{}.{}.hash.{}'
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'


def load_image_by_name(image_name):
    context_path = os.path.join(GECKO, 'testing', 'docker', image_name)
    context_hash = docker.generate_context_hash(context_path)

    image_index_url = INDEX_URL.format('mozilla-central', image_name, context_hash)
    print("Fetching", image_index_url)
    task = json.load(urllib2.urlopen(image_index_url))

    return load_image_by_task_id(task['taskId'])


def load_image_by_task_id(task_id):
    # because we need to read this file twice (and one read is not all the way
    # through), it is difficult to stream it.  So we download to disk and then
    # read it back.
    filename = 'temp-docker-image.tar'

    artifact_url = ARTIFACT_URL.format(task_id, 'public/image.tar')
    print("Downloading", artifact_url)
    subprocess.check_call(['curl', '-#', '-L', '-o', filename, artifact_url])

    print("Determining image name")
    tf = tarfile.open(filename)
    repositories = json.load(tf.extractfile('repositories'))
    name = repositories.keys()[0]
    tag = repositories[name].keys()[0]
    name = '{}:{}'.format(name, tag)
    print("Image name:", name)

    print("Loading image into docker")
    try:
        subprocess.check_call(['docker', 'load', '-i', filename])
    except subprocess.CalledProcessError:
        print("*** `docker load` failed.  You may avoid re-downloading that tarball by fixing the")
        print("*** problem and running `docker load < {}`.".format(filename))
        raise

    print("Deleting temporary file")
    os.unlink(filename)

    print("The requested docker image is now available as", name)
    print("Try: docker run -ti --rm {} bash".format(name))
