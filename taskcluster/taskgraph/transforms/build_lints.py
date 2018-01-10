# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def check_mozharness_perfherder_options(config, jobs):
    """Verify that multiple jobs don't use the same perfherder bucket.

    Build jobs record perfherder metrics by default. Perfherder metrics go
    to a bucket derived by the platform by default. The name can further be
    customized by the presence of "extra options" either defined in
    mozharness sub-configs or in an environment variable.

    This linter tries to verify that no 2 jobs will send Perfherder metrics
    to the same bucket by looking for jobs not defining extra options when
    their platform or mozharness config are otherwise similar.
    """
    seen_configs = {}

    for job in jobs:
        if job['run']['using'] != 'mozharness':
            yield job
            continue

        worker = job.get('worker', {})

        platform = job['treeherder']['platform']
        primary_config = job['run']['config'][0]
        options = worker.get('env', {}).get('PERFHERDER_EXTRA_OPTIONS')
        nightly = job.get('attributes', {}).get('nightly', False)

        # This isn't strictly necessary. But the Perfherder code looking at the
        # values we care about is only active on builds. So it doesn't make
        # sense to run this linter elsewhere.
        assert primary_config.startswith('builds/')

        key = (platform, primary_config, nightly, options)

        if key in seen_configs:
            raise Exception('Non-unique Perfherder data collection for jobs '
                            '%s and %s: set PERFHERDER_EXTRA_OPTIONS in worker '
                            'environment variables or use different mozconfigs'
                            % (job['name'], seen_configs[key]))

        seen_configs[key] = job['name']

        yield job
