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
import which

from taskgraph.util import docker

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))
IMAGE_DIR = os.path.join(GECKO, 'testing', 'docker')
INDEX_URL = 'https://index.taskcluster.net/v1/task/docker.images.v1.{}.{}.hash.{}'
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'


def load_image_by_name(image_name):
    context_path = os.path.join(GECKO, 'testing', 'docker', image_name)
    context_hash = docker.generate_context_hash(GECKO, context_path, image_name)

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


def build_image(name):
    """Build a Docker image of specified name.

    Output from image building process will be printed to stdout.
    """
    if not name:
        raise ValueError('must provide a Docker image name')

    image_dir = os.path.join(IMAGE_DIR, name)
    if not os.path.isdir(image_dir):
        raise Exception('image directory does not exist: %s' % image_dir)

    docker_bin = which.which('docker')

    # Verify that Docker is working.
    try:
        subprocess.check_output([docker_bin, '--version'])
    except subprocess.CalledProcessError:
        raise Exception('Docker server is unresponsive. Run `docker ps` and '
                        'check that Docker is running')

    args = [os.path.join(IMAGE_DIR, 'build.sh'), name]
    res = subprocess.call(args, cwd=IMAGE_DIR)
    if res:
        raise Exception('error building image')
