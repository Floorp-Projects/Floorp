# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozbuild.base import MozbuildObject
from mozversioncontrol import get_repository_object

from tryselect.cli import BaseTryParser

from .again import run as again_run
from .fuzzy import run as fuzzy_run

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)


class CompareParser(BaseTryParser):
    name = "compare"
    arguments = [
        [
            ["-cc", "--compare-commit"],
            {
                "default": None,
                "help": "The commit that you want to compare your current revision with",
            },
        ],
    ]
    common_groups = ["task"]
    task_configs = [
        "rebuild",
    ]

    def get_revisions_to_run(vcs, compare_commit):
        if compare_commit is None:
            compare_commit = vcs.base_ref
        if vcs.branch:
            current_revision_ref = vcs.branch
        else:
            current_revision_ref = vcs.head_ref

        return compare_commit, current_revision_ref


def run(compare_commit=None, **kwargs):
    vcs = get_repository_object(build.topsrcdir)
    compare_commit, current_revision_ref = CompareParser.get_revisions_to_run(
        vcs, compare_commit
    )
    print("********************************************")
    print("* 2 commits are created with this command *")
    print("********************************************")

    try:
        fuzzy_run(**kwargs)
        print("********************************************")
        print("*    The base commit can be found above    *")
        print("********************************************")
        vcs.update(compare_commit)
        again_run()
        print("*****************************************")
        print("* The compare commit can be found above *")
        print("*****************************************")
    finally:
        vcs.update(current_revision_ref)
