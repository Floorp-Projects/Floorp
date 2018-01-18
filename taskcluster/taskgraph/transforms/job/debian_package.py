# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from taskgraph.util.schema import Schema
from voluptuous import Any, Optional, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import add_public_artifacts

from taskgraph.util.hash import hash_path
from taskgraph import GECKO
from taskgraph.util.cached_tasks import add_optimization

DSC_PACKAGE_RE = re.compile('.*(?=_)')
SOURCE_PACKAGE_RE = re.compile('.*(?=[-_]\d)')

source_definition = {
    Required('url'): basestring,
    Required('sha256'): basestring,
}

run_schema = Schema({
    Required('using'): 'debian-package',
    # Debian distribution
    Required('dist'): basestring,

    # Date of the snapshot (from snapshot.debian.org) to use, in the format
    # YYYYMMDDTHHMMSSZ. The same date is used for the base docker-image name
    # (only the YYYYMMDD part).
    Required('snapshot'): basestring,

    # URL/SHA256 of a source file to build, which can either be a source
    # control (.dsc), or a tarball.
    Required(Any('dsc', 'tarball')): source_definition,

    # Patch to apply to the extracted source.
    Optional('patch'): basestring,

    # Command to run before dpkg-buildpackage.
    Optional('pre-build-command'): basestring,

    # List of package tasks to get build dependencies from.
    Optional('packages'): [basestring],

    # What resolver to use to install build dependencies. The default
    # (apt-get) is good in most cases, but in subtle cases involving
    # a *-backports archive, its solver might not be able to find a
    # solution that satisfies the build dependencies.
    Optional('resolver'): Any('apt-get', 'aptitude'),
})


@run_job_using("docker-worker", "debian-package", schema=run_schema)
def docker_worker_debian_package(config, job, taskdesc):
    run = job['run']

    name = taskdesc['label'].replace('{}-'.format(config.kind), '', 1)

    worker = taskdesc['worker']
    worker['artifacts'] = []
    worker['docker-image'] = 'debian:{dist}-{date}'.format(
        dist=run['dist'],
        date=run['snapshot'][:8])

    add_public_artifacts(config, job, taskdesc, path='/tmp/artifacts')

    env = worker.setdefault('env', {})
    env['DEBFULLNAME'] = 'Mozilla build team'
    env['DEBEMAIL'] = 'dev-builds@lists.mozilla.org'

    if 'dsc' in run:
        src = run['dsc']
        unpack = 'dpkg-source -x {src_file} {package}'
        package_re = DSC_PACKAGE_RE
    elif 'tarball' in run:
        src = run['tarball']
        unpack = ('mkdir {package} && '
                  'tar -C {package} -axf {src_file} --strip-components=1')
        package_re = SOURCE_PACKAGE_RE
    else:
        raise RuntimeError('Unreachable')
    src_url = src['url']
    src_file = os.path.basename(src_url)
    src_sha256 = src['sha256']
    package = package_re.match(src_file).group(0)
    unpack = unpack.format(src_file=src_file, package=package)

    base_deps = [
        'apt-utils',
        'build-essential',
        'devscripts',
        'fakeroot',
    ]

    resolver = run.get('resolver', 'apt-get')
    if resolver == 'apt-get':
        resolver = 'apt-get -yyq --no-install-recommends'
    elif resolver == 'aptitude':
        resolver = ('aptitude -y --without-recommends -o '
                    'Aptitude::ProblemResolver::Hints::KeepBuildDeps='
                    '"reject {}-build-deps :UNINST"').format(package)
        base_deps.append('aptitude')
    else:
        raise RuntimeError('Unreachable')

    adjust = ''
    if 'patch' in run:
        # We can't depend on docker images, so we don't have robustcheckout or
        # or run-task to get a checkout. So for this one file we'd need
        # from a checkout, download it.
        env['PATCH_URL'] = '{head_repo}/raw-file/{head_rev}/build/debian-packages/{patch}'.format(
            head_repo=config.params['head_repository'],
            head_rev=config.params['head_rev'],
            patch=run['patch'],
        )
        adjust += 'curl -sL $PATCH_URL | patch -p1 && '
    if 'pre-build-command' in run:
        adjust += run['pre-build-command'] + ' && '
    if 'tarball' in run:
        adjust += 'mv ../{src_file} ../{package}_{ver}.orig.tar.gz && '.format(
            src_file=src_file,
            package=package,
            ver='$(dpkg-parsechangelog | awk \'$1=="Version:"{print $2}\' | cut -f 1 -d -)',
        )
    if 'patch' not in run and 'pre-build-command' not in run:
        adjust += ('debchange -l ".{prefix}moz" --distribution "{dist}"'
                   ' "Mozilla backport for {dist}." < /dev/null && ').format(
            prefix=name.split('-', 1)[0],
            dist=run['dist'],
        )

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
        'echo "deb http://snapshot.debian.org/archive/debian'
        '/{snapshot}/ {dist}-backports main" >> /etc/apt/sources.list && '
        'echo "deb http://snapshot.debian.org/archive/debian-security'
        '/{snapshot}/ {dist}/updates main" >> /etc/apt/sources.list && '
        'apt-get update -o Acquire::Check-Valid-Until=false -q && '
        # Add sources for packages coming from other package tasks.
        'apt-get install -yyq apt-transport-https ca-certificates && '
        'for task in $PACKAGES; do '
        '  echo "deb [trusted=yes] https://queue.taskcluster.net/v1/task'
        '/$task/runs/0/artifacts/public/build/ debian/" '
        '>> /etc/apt/sources.list; '
        'done && '
        # Install the base utilities required to build debian packages.
        'apt-get update -o Acquire::Check-Valid-Until=false -q && '
        'apt-get install -yyq {base_deps} && '
        'cd /tmp && '
        # Get, validate and extract the package source.
        'dget -d -u {src_url} && '
        'echo "{src_sha256}  {src_file}" | sha256sum -c && '
        '{unpack} && '
        'cd {package} && '
        # Optionally apply patch and/or pre-build command.
        '{adjust}'
        # Install the necessary build dependencies.
        'mk-build-deps -i -r debian/control -t \'{resolver}\' && '
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
            src_url=src_url,
            src_file=src_file,
            src_sha256=src_sha256,
            unpack=unpack,
            adjust=adjust,
            artifacts='/tmp/artifacts',
            base_deps=' '.join(base_deps),
            resolver=resolver,
        )
    ]

    # Use the command generated above as the base for the index hash.
    # We rely on it not varying depending on the head_repository or head_rev.
    data = list(worker['command'])
    if 'patch' in run:
        data.append(hash_path(os.path.join(GECKO, 'build', 'debian-packages', run['patch'])))

    if run.get('packages'):
        env = worker.setdefault('env', {})
        env['PACKAGES'] = {
            'task-reference': ' '.join('<{}>'.format(p)
                                       for p in run['packages'])
        }
        deps = taskdesc.setdefault('dependencies', {})
        for p in run['packages']:
            deps[p] = 'packages-{}'.format(p)
            data.append(p)

    add_optimization(config, taskdesc, cache_type='packages.v1',
                     cache_name=name, digest_data=data)
