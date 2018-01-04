# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import sys
import subprocess
import tarfile
import tempfile
import which
from subprocess import Popen, PIPE
from io import BytesIO

from taskgraph.util import docker
from taskgraph.util.taskcluster import (
    find_task_id,
    get_artifact_url,
)
from taskgraph.util.cached_tasks import cached_index_path
from . import GECKO


def load_image_by_name(image_name, tag=None):
    context_path = os.path.join(GECKO, 'taskcluster', 'docker', image_name)
    context_hash = docker.generate_context_hash(GECKO, context_path, image_name)

    index_path = cached_index_path(
        trust_domain='gecko',
        level=3,
        cache_type='docker-images.v1',
        cache_name=image_name,
        digest=context_hash,
    )
    task_id = find_task_id(index_path)

    return load_image_by_task_id(task_id, tag)


def load_image_by_task_id(task_id, tag=None):
    artifact_url = get_artifact_url(task_id, 'public/image.tar.zst')
    result = load_image(artifact_url, tag)
    print("Found docker image: {}:{}".format(result['image'], result['tag']))
    if tag:
        print("Re-tagged as: {}".format(tag))
    else:
        tag = '{}:{}'.format(result['image'], result['tag'])
    print("Try: docker run -ti --rm {} bash".format(tag))
    return True


def build_context(name, outputFile, args=None):
    """Build a context.tar for image with specified name.
    """
    if not name:
        raise ValueError('must provide a Docker image name')
    if not outputFile:
        raise ValueError('must provide a outputFile')

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception('image directory does not exist: %s' % image_dir)

    docker.create_context_tar(GECKO, image_dir, outputFile, "", args)


def build_image(name, args=None):
    """Build a Docker image of specified name.

    Output from image building process will be printed to stdout.
    """
    if not name:
        raise ValueError('must provide a Docker image name')

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception('image directory does not exist: %s' % image_dir)

    tag = docker.docker_image(name, by_tag=True)

    docker_bin = which.which('docker')

    # Verify that Docker is working.
    try:
        subprocess.check_output([docker_bin, '--version'])
    except subprocess.CalledProcessError:
        raise Exception('Docker server is unresponsive. Run `docker ps` and '
                        'check that Docker is running')

    # We obtain a context archive and build from that. Going through the
    # archive creation is important: it normalizes things like file owners
    # and mtimes to increase the chances that image generation is
    # deterministic.
    fd, context_path = tempfile.mkstemp()
    os.close(fd)
    try:
        docker.create_context_tar(GECKO, image_dir, context_path, name, args)
        docker.build_from_context(docker_bin, context_path, name, tag)
    finally:
        os.unlink(context_path)

    print('Successfully built %s and tagged with %s' % (name, tag))

    if tag.endswith(':latest'):
        print('*' * 50)
        print('WARNING: no VERSION file found in image directory.')
        print('Image is not suitable for deploying/pushing.')
        print('Create an image suitable for deploying/pushing by creating')
        print('a VERSION file in the image directory.')
        print('*' * 50)


def load_image(url, imageName=None, imageTag=None):
    """
    Load docker image from URL as imageName:tag, if no imageName or tag is given
    it will use whatever is inside the zstd compressed tarball.

    Returns an object with properties 'image', 'tag' and 'layer'.
    """
    # If imageName is given and we don't have an imageTag
    # we parse out the imageTag from imageName, or default it to 'latest'
    # if no imageName and no imageTag is given, 'repositories' won't be rewritten
    if imageName and not imageTag:
        if ':' in imageName:
            imageName, imageTag = imageName.split(':', 1)
        else:
            imageTag = 'latest'

    curl, zstd, docker = None, None, None
    image, tag, layer = None, None, None
    error = None
    try:
        # Setup piping: curl | zstd | tarin
        curl = Popen(['curl', '-#', '--fail', '-L', '--retry', '8', url], stdout=PIPE)
        zstd = Popen(['zstd', '-d'], stdin=curl.stdout, stdout=PIPE)
        tarin = tarfile.open(mode='r|', fileobj=zstd.stdout)
        # Seutp piping: tarout | docker
        docker = Popen(['docker', 'load'], stdin=PIPE)
        tarout = tarfile.open(mode='w|', fileobj=docker.stdin, format=tarfile.GNU_FORMAT)

        # Read from tarin and write to tarout
        for member in tarin:
            # Write non-file members directly (don't use extractfile on links)
            if not member.isfile():
                tarout.addfile(member)
                continue

            # Open reader for the member
            reader = tarin.extractfile(member)

            # If member is repository, we parse and possibly rewrite the image tags
            if member.name == 'repositories':
                # Read and parse repositories
                repos = json.loads(reader.read())
                reader.close()

                # If there is more than one image or tag, we can't handle it here
                if len(repos.keys()) > 1:
                    raise Exception('file contains more than one image')
                image = repos.keys()[0]
                if len(repos[image].keys()) > 1:
                    raise Exception('file contains more than one tag')
                tag = repos[image].keys()[0]
                layer = repos[image][tag]

                # Rewrite the repositories file
                data = json.dumps({imageName or image: {imageTag or tag: layer}})
                reader = BytesIO(data)
                member.size = len(data)

            # Add member and reader
            tarout.addfile(member, reader)
            reader.close()
        tarout.close()
    except Exception:
        error = sys.exc_info()[0]
    finally:
        def trykill(proc):
            try:
                proc.kill()
            except:
                pass

        # Check that all subprocesses finished correctly
        if curl and curl.wait() != 0:
            trykill(zstd)
            trykill(docker)
            raise Exception('failed to download from url: {}'.format(url))
        if zstd and zstd.wait() != 0:
            trykill(docker)
            raise Exception('zstd decompression failed')
        if docker:
            docker.stdin.close()
        if docker and docker.wait() != 0:
            raise Exception('loading into docker failed')
        if error:
            raise error

    # Check that we found a repositories file
    if not image or not tag or not layer:
        raise Exception('No repositories file found!')

    return {'image': image, 'tag': tag, 'layer': layer}
