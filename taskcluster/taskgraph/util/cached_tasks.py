# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import hashlib
import time

import six


TARGET_CACHE_INDEX = "{trust_domain}.cache.level-{level}.{type}.{name}.hash.{digest}"
EXTRA_CACHE_INDEXES = [
    "{trust_domain}.cache.level-{level}.{type}.{name}.latest",
    "{trust_domain}.cache.level-{level}.{type}.{name}.pushdate.{build_date_long}",
]


def add_optimization(
    config, taskdesc, cache_type, cache_name, digest=None, digest_data=None
):
    """
    Allow the results of this task to be cached. This adds index routes to the
    task so it can be looked up for future runs, and optimization hints so that
    cached artifacts can be found. Exactly one of `digest` and `digest_data`
    must be passed.

    :param TransformConfig config: The configuration for the kind being transformed.
    :param dict taskdesc: The description of the current task.
    :param str cache_type: The type of task result being cached.
    :param str cache_name: The name of the object being cached.
    :param digest: A unique string indentifying this version of the artifacts
        being generated. Typically this will be the hash of inputs to the task.
    :type digest: bytes or None
    :param digest_data: A list of bytes representing the inputs of this task.
        They will be concatenated and hashed to create the digest for this
        task.
    :type digest_data: list of bytes or None
    """
    cached_task = taskdesc.get("attributes", {}).get("cached_task")
    if cached_task is False:
        return

    if (digest is None) == (digest_data is None):
        raise Exception("Must pass exactly one of `digest` and `digest_data`.")
    if digest is None:
        digest = hashlib.sha256(six.ensure_binary("\n".join(digest_data))).hexdigest()

    subs = {
        "trust_domain": config.graph_config["trust-domain"],
        "type": cache_type,
        "name": cache_name,
        "digest": digest,
    }

    # We'll try to find a cached version of the toolchain at levels above
    # and including the current level, starting at the highest level.
    index_routes = []
    for level in reversed(range(int(config.params["level"]), 4)):
        subs["level"] = level
        index_routes.append(TARGET_CACHE_INDEX.format(**subs))
    taskdesc["optimization"] = {"index-search": index_routes}

    # ... and cache at the lowest level.
    taskdesc.setdefault("routes", []).append(
        "index.{}".format(TARGET_CACHE_INDEX.format(**subs))
    )

    # ... and add some extra routes for humans
    subs["build_date_long"] = time.strftime(
        "%Y.%m.%d.%Y%m%d%H%M%S", time.gmtime(config.params["build_date"])
    )
    taskdesc["routes"].extend(
        ["index.{}".format(route.format(**subs)) for route in EXTRA_CACHE_INDEXES]
    )

    taskdesc["attributes"]["cached_task"] = {
        "type": cache_type,
        "name": cache_name,
        "digest": digest,
    }

    # Allow future pushes to find this task before it completes
    # Implementation in morphs
    taskdesc["attributes"]["eager_indexes"] = [TARGET_CACHE_INDEX.format(**subs)]
