# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

import os

from taskgraph.util.schema import Schema
from voluptuous import Optional, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import add_public_artifacts

from taskgraph.util.hash import hash_paths
from taskgraph import GECKO
from taskgraph.util.cached_tasks import add_optimization

run_schema = Schema({
    Required('using'): 'debian-package',
    # Debian distribution
    Optional('dist'): basestring,

    # Date of the snapshot (from snapshot.debian.org) to use, in the format
    # YYYYMMDDTHHMMSSZ. The same date is used for the base docker-image name
    # (only the YYYYMMDD part).
    Optional('snapshot'): basestring,

    # URL of the source control (.dsc) file to build.
    Required('dsc'): basestring,

    # SHA256 of the source control (.dsc) file.
    Required('dsc-sha256'): basestring,

    # Patch to apply to the extracted source.
    Optional('patch'): basestring,

    # Command to run before dpkg-buildpackage.
    Optional('pre-build-command'): basestring,
})


@run_job_using("docker-worker", "debian-package", schema=run_schema)
def docker_worker_debian_package(config, job, taskdesc):
    run = job['run']
    run.setdefault('dist', 'wheezy')
    run.setdefault('snapshot', '20171210T214726Z')

    worker = taskdesc['worker']
    worker['artifacts'] = []
    worker['docker-image'] = 'debian:{dist}-{date}'.format(
        dist=run['dist'],
        date=run['snapshot'][:8])

    add_public_artifacts(config, job, taskdesc, path='/tmp/artifacts')

    dsc_file = os.path.basename(run['dsc'])
    package = dsc_file[:dsc_file.index('_')]

    adjust = ''
    if 'patch' in run:
        # We can't depend on docker images, so we don't have robustcheckout
        # or run-task to get a checkout. So for this one file we'd need
        # from a checkout, download it.
        adjust += ('curl -sL {head_repo}/raw-file/{head_rev}'
                   '/build/debian-packages/{patch} | patch -p1 && ').format(
            head_repo=config.params['head_repository'],
            head_rev=config.params['head_rev'],
            patch=run['patch'],
        )
    if 'pre-build-command' in run:
        adjust += run['pre-build-command'] + ' && '

    # We can't depend on docker images (since docker images depend on packages),
    # so we inline the whole script here.
    worker['command'] = [
        'sh',
        '-x',
        '-c',
        # Fill /etc/apt/sources.list with the relevant snapshot repository.
        'echo "deb http://snapshot.debian.org/archive/debian'
        '/{snapshot}/ {dist} main" > /etc/apt/sources.list && '
        'echo "deb http://snapshot.debian.org/archive/debian'
        '/{snapshot}/ {dist}-updates main" >> /etc/apt/sources.list && '
        'echo "deb http://snapshot.debian.org/archive/debian-security'
        '/{snapshot}/ {dist}/updates main" >> /etc/apt/sources.list && '
        # Install the base utilities required to build debian packages.
        'apt-get update -o Acquire::Check-Valid-Until=false -q && '
        'apt-get install -yyq fakeroot build-essential devscripts apt-utils && '
        'cd /tmp && '
        # Get, validate and extract the package source.
        'dget -d -u {dsc} && '
        'echo "{dsc_sha256}  {dsc_file}" | sha256sum -c && '
        'dpkg-source -x {dsc_file} {package} && '
        'cd {package} && '
        # Optionally apply patch and/or pre-build command.
        '{adjust}'
        # Install the necessary build dependencies.
        'mk-build-deps -i -r debian/control -t "apt-get -yyq --no-install-recommends" && '
        # Build the package
        'DEB_BUILD_OPTIONS="parallel=$(nproc) nocheck" dpkg-buildpackage && '
        # Copy the artifacts
        'mkdir -p {artifacts}/debian && '
        'dcmd cp ../{package}_*.changes {artifacts}/debian/ && '
        'cd {artifacts} && '
        # Make the artifacts directory usable as an APT repository.
        'apt-ftparchive sources debian | gzip -c9 > debian/Sources.gz && '
        'apt-ftparchive packages debian | gzip -c9 > debian/Packages.gz && '
        'apt-ftparchive release -o APT::FTPArchive::Release::Codename={dist} debian > Release && '
        'mv Release debian/'
        .format(
            package=package,
            snapshot=run['snapshot'],
            dist=run['dist'],
            dsc=run['dsc'],
            dsc_file=dsc_file,
            dsc_sha256=run['dsc-sha256'],
            adjust=adjust,
            artifacts='/tmp/artifacts',
        )
    ]

    name = taskdesc['label'].replace('{}-'.format(config.kind), '', 1)
    files = [
        # This file
        'taskcluster/taskgraph/transforms/job/debian_package.py',
    ]
    if 'patch' in run:
        files.append('build/debian-packages/{}'.format(run['patch']))
    data = [hash_paths(GECKO, files)]
    for k in ('snapshot', 'dist', 'dsc-sha256', 'pre-build-command'):
        if k in run:
            data.append(run[k])
    add_optimization(config, taskdesc, cache_type='packages.v1',
                     cache_name=name, digest_data=data)
