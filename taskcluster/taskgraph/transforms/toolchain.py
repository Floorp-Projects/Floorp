# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

import os
from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def use_toolchains(config, jobs):
    """Add dependencies corresponding to toolchains to use, and pass a list
    of corresponding artifacts to jobs using toolchains.
    """
    artifacts = {}
    aliases_by_job = {}

    def get_attribute(dict, key, attributes, attribute_name):
        '''Get `attribute_name` from the given `attributes` dict, and if there
        is a corresponding value, set `key` in `dict` to that value.'''
        value = attributes.get(attribute_name)
        if value:
            dict[key] = value

    # Toolchain jobs can depend on other toolchain jobs, but we don't have full
    # tasks for them, since they're being transformed. So scan the jobs list in
    # that case, otherwise, use the list of tasks for the kind dependencies.
    if config.kind == 'toolchain':
        jobs = list(jobs)
        for job in jobs:
            run = job.get('run', {})
            get_attribute(artifacts, job['name'], run, 'toolchain-artifact')
            get_attribute(aliases_by_job, job['name'], run, 'toolchain-alias')
    else:
        for task in config.kind_dependencies_tasks:
            if task.kind != 'toolchain':
                continue
            name = task.label.replace('%s-' % task.kind, '')
            get_attribute(artifacts, name, task.attributes, 'toolchain-artifact')
            get_attribute(aliases_by_job, name, task.attributes, 'toolchain-alias')

    aliases = {}
    for job, alias in aliases_by_job.items():
        if alias in aliases:
            raise Exception(
                "Cannot use the alias %s for %s, it's already used for %s"
                % (alias, job, aliases[alias]))
        if alias in artifacts:
            raise Exception(
                "Cannot use the alias %s for %s, it's already a toolchain job"
                % (alias, job))
        aliases[alias] = job

    for job in jobs:
        env = job.setdefault('worker', {}).setdefault('env', {})

        toolchains = [aliases.get(t, t)
                      for t in job.pop('toolchains', [])]

        if config.kind == 'toolchain' and job['name'] in toolchains:
            raise Exception("Toolchain job %s can't use itself as toolchain"
                            % job['name'])

        filenames = {}
        for t in toolchains:
            if t not in artifacts:
                raise Exception('Missing toolchain job for %s-%s: %s'
                                % (config.kind, job['name'], t))

            f = os.path.basename(artifacts[t])
            if f in filenames:
                # Build jobs don't support toolchain artifacts with the same
                # name: they would overwrite one with the other.
                raise Exception('%s-%s cannot use both %s and %s toolchains: '
                                'they both have the same artifact name %s'
                                % (config.kind, job['name'], filenames[f],
                                   t, f))
            filenames[f] = t

        if toolchains:
            job.setdefault('dependencies', {}).update(
                ('toolchain-%s' % t, 'toolchain-%s' % t)
                for t in toolchains
            )
            # Pass a list of artifact-path@task-id to the job for all the
            # toolchain artifacts it's going to need, where task-id is
            # corresponding to the (possibly optimized) toolchain job, and
            # artifact-path to the toolchain-artifact defined for that
            # toolchain job.
            env['MOZ_TOOLCHAINS'] = {'task-reference': ' '.join(
                '%s@<toolchain-%s>' % (artifacts[t], t)
                for t in toolchains
            )}

        yield job
