# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Chunk the partner repack tasks by subpartner and locale
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from mozbuild.chunkify import chunkify
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.partners import (
    get_partner_config_by_kind,
    locales_per_build_platform,
    apply_partner_priority,
)

transforms = TransformSequence()
transforms.add(apply_partner_priority)


used_repack_ids_by_platform = {}


def _check_repack_ids_by_platform(platform, repack_id):
    """avoid dup chunks, since mac signing and repackages both chunk"""
    if used_repack_ids_by_platform.get(platform, {}).get(repack_id):
        return True
    used_repack_ids_by_platform.setdefault(platform, {})['repack_id'] = True


def _get_repack_ids_by_platform(partner_configs, build_platform):
    combinations = []
    for partner, partner_config in partner_configs.items():
        for sub_partner, cfg in partner_config.items():
            if build_platform not in cfg.get("platforms", []):
                continue
            locales = locales_per_build_platform(build_platform, cfg.get('locales', []))
            for locale in locales:
                combinations.append("{}/{}/{}".format(partner, sub_partner, locale))
    return sorted(combinations)


@transforms.add
def chunk_partners(config, jobs):
    partner_configs = get_partner_config_by_kind(config, config.kind)

    for job in jobs:
        dep_job = job['primary-dependency']
        build_platform = dep_job.attributes["build_platform"]
        repack_id = dep_job.task.get('extra', {}).get('repack_id')
        repack_ids = dep_job.task.get('extra', {}).get('repack_ids')

        # first downstream of the repack task, no chunking or fanout has been done yet
        if not any([repack_id, repack_ids]):
            platform_repack_ids = _get_repack_ids_by_platform(partner_configs, build_platform)
            # we chunk mac signing
            if config.kind in ("release-partner-repack-signing",
                               "release-eme-free-repack-signing"):
                repacks_per_chunk = job.get('repacks-per-chunk')
                chunks, remainder = divmod(len(platform_repack_ids), repacks_per_chunk)
                if remainder:
                    chunks = int(chunks + 1)
                for this_chunk in range(1, chunks + 1):
                    chunk = chunkify(platform_repack_ids, this_chunk, chunks)
                    partner_job = copy.deepcopy(job)
                    partner_job.setdefault('extra', {}).setdefault('repack_ids', chunk)
                    partner_job['extra']['repack_suffix'] = str(this_chunk)
                    yield partner_job
            # linux and windows we fan out immediately to one task per partner-sub_partner-locale
            else:
                for repack_id in platform_repack_ids:
                    if _check_repack_ids_by_platform(build_platform, repack_id):
                        continue
                    partner_job = copy.deepcopy(job)  # don't overwrite dict values here
                    partner_job.setdefault('extra', {})
                    partner_job['extra']['repack_id'] = repack_id
                    yield partner_job
        # fan out chunked mac signing for repackage
        elif repack_ids:
            for repack_id in repack_ids:
                if _check_repack_ids_by_platform(build_platform, repack_id):
                    continue
                partner_job = copy.deepcopy(job)
                partner_job.setdefault('extra', {}).setdefault('repack_id', repack_id)
                yield partner_job
        # otherwise we've fully fanned out already, continue by passing repack_id on
        else:
            if _check_repack_ids_by_platform(build_platform, repack_id):
                continue
            partner_job = copy.deepcopy(job)
            partner_job.setdefault('extra', {}).setdefault('repack_id', repack_id)
            yield partner_job
