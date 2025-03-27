# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys

import buildconfig
from buildconfig import topsrcdir
from run_glean_parser import parse


def main(serverknobs_fd, *args):
    all_objs, options = parse(args)

    only_interesting = set()
    interesting_yamls = buildconfig.substs.get("MOZ_GLEAN_INTERESTING_METRICS_FILES")
    if interesting_yamls:
        interesting = [os.path.join(topsrcdir, x) for x in interesting_yamls]
        interesting.append(
            os.path.join(topsrcdir, "toolkit/components/glean/tags.yaml")
        )
        extra_tags_files = buildconfig.substs.get("MOZ_GLEAN_EXTRA_TAGS_FILES")
        if extra_tags_files:
            tag_files = [os.path.join(topsrcdir, x) for x in extra_tags_files]
            interesting.extend(tag_files)

        interesting_metrics, _options = parse(interesting + list(args[1:]))
        for category_name in interesting_metrics.keys():
            if category_name in ["pings", "tags"]:
                continue
            for name, metric in interesting_metrics[category_name].items():
                fullname = f"{category_name}.{name}"
                only_interesting.add(fullname)

    only_uninteresting = {}

    for category_name in all_objs.keys():
        if category_name in ["pings", "tags"]:
            continue
        for name, metric in all_objs[category_name].items():
            fullname = f"{category_name}.{name}"
            if fullname not in only_interesting:
                only_uninteresting[fullname] = True

    config = {"metrics_enabled": only_uninteresting}
    json.dump(config, serverknobs_fd, sort_keys=True, indent=2)


if __name__ == "__main__":
    main(sys.stdout, *sys.argv[1:])
