# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover-source task to also append `build` as dependency
"""
from __future__ import absolute_import

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def tweak_beetmover_source_dependencies_and_upstream_artifacts(config, jobs):
    for job in jobs:
        # HACK1: instead of grabbing SOURCE file from `release-source` task, we
        # instead take it along with SOURCE.asc directly from the
        # `release-source-signing`.
        #
        # HACK2: This way, we can just overwrite the `build`
        # dependency, which at this point still is `release-source` task, with
        # the actual Nightly en-US linux64 build which contains the
        # `balrog_props` file we're interested in.
        #
        # XXX: this hack should go away by either:
        # * rewriting beetmover transforms to allow more flexibility in deps
        # * ditch balrog_props in beetmover and rely on in-tree task payload

        if job['attributes']['shipping_product'] == 'firefox':
            job['dependencies']['build'] = u'build-linux64-nightly/opt'
        elif job['attributes']['shipping_product'] == 'fennec':
            job['dependencies']['build'] = u'build-android-api-16-nightly/opt'
        elif job['attributes']['shipping_product'] == 'devedition':
            job['dependencies']['build'] = u'build-linux64-devedition-nightly/opt'
        else:
            raise NotImplemented(
                "Unknown shipping_product {} for beetmover_source!".format(
                    job['attributes']['shipping_product']
                )
            )
        upstream_artifacts = job['worker']['upstream-artifacts']
        for artifact in upstream_artifacts:
            if artifact['taskType'] == 'build':
                artifact['paths'].append(u'public/build/balrog_props.json')
                break

        yield job
