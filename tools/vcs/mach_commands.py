# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import logging
import os
import re
import subprocess
import sys

import mozpack.path as mozpath
from mach.decorators import Command, CommandArgument

GITHUB_ROOT = "https://github.com/"
PR_REPOSITORIES = {
    "webrender": {
        "github": "servo/webrender",
        "path": "gfx/wr",
        "bugzilla_product": "Core",
        "bugzilla_component": "Graphics: WebRender",
    },
    "webgpu": {
        "github": "gfx-rs/wgpu",
        "path": "gfx/wgpu",
        "bugzilla_product": "Core",
        "bugzilla_component": "Graphics: WebGPU",
    },
    "debugger": {
        "github": "firefox-devtools/debugger",
        "path": "devtools/client/debugger",
        "bugzilla_product": "DevTools",
        "bugzilla_component": "Debugger",
    },
}


@Command(
    "import-pr",
    category="misc",
    description="Import a pull request from Github to the local repo.",
)
@CommandArgument("-b", "--bug-number", help="Bug number to use in the commit messages.")
@CommandArgument(
    "-t",
    "--bugzilla-token",
    help="Bugzilla API token used to file a new bug if no bug number is provided.",
)
@CommandArgument("-r", "--reviewer", help="Reviewer nick to apply to commit messages.")
@CommandArgument(
    "pull_request",
    help="URL to the pull request to import (e.g. "
    "https://github.com/servo/webrender/pull/3665).",
)
def import_pr(
    command_context,
    pull_request,
    bug_number=None,
    bugzilla_token=None,
    reviewer=None,
):
    import requests

    pr_number = None
    repository = None
    for r in PR_REPOSITORIES.values():
        if pull_request.startswith(GITHUB_ROOT + r["github"] + "/pull/"):
            # sanitize URL, dropping anything after the PR number
            pr_number = int(re.search("/pull/([0-9]+)", pull_request).group(1))
            pull_request = GITHUB_ROOT + r["github"] + "/pull/" + str(pr_number)
            repository = r
            break

    if repository is None:
        command_context.log(
            logging.ERROR,
            "unrecognized_repo",
            {},
            "The pull request URL was not recognized; add it to the list of "
            "recognized repos in PR_REPOSITORIES in %s" % __file__,
        )
        sys.exit(1)

    command_context.log(
        logging.INFO,
        "import_pr",
        {"pr_url": pull_request},
        "Attempting to import {pr_url}",
    )
    dirty = [
        f
        for f in command_context.repository.get_changed_files(mode="all")
        if f.startswith(repository["path"])
    ]
    if dirty:
        command_context.log(
            logging.ERROR,
            "dirty_tree",
            repository,
            "Local {path} tree is dirty; aborting!",
        )
        sys.exit(1)
    target_dir = mozpath.join(
        command_context.topsrcdir, os.path.normpath(repository["path"])
    )

    if bug_number is None:
        if bugzilla_token is None:
            command_context.log(
                logging.WARNING,
                "no_token",
                {},
                "No bug number or bugzilla API token provided; bug number will not "
                "be added to commit messages.",
            )
        else:
            bug_number = _file_bug(
                command_context, bugzilla_token, repository, pr_number
            )
    elif bugzilla_token is not None:
        command_context.log(
            logging.WARNING,
            "too_much_bug",
            {},
            "Providing a bugzilla token is unnecessary when a bug number is provided. "
            "Using bug number; ignoring token.",
        )

    pr_patch = requests.get(pull_request + ".patch")
    pr_patch.raise_for_status()
    for patch in _split_patches(pr_patch.content, bug_number, pull_request, reviewer):
        command_context.log(
            logging.INFO,
            "commit_msg",
            patch,
            "Processing commit [{commit_summary}] by [{author}] at [{date}]",
        )
        patch_cmd = subprocess.Popen(
            ["patch", "-p1", "-s"], stdin=subprocess.PIPE, cwd=target_dir
        )
        patch_cmd.stdin.write(patch["diff"].encode("utf-8"))
        patch_cmd.stdin.close()
        patch_cmd.wait()
        if patch_cmd.returncode != 0:
            command_context.log(
                logging.ERROR,
                "commit_fail",
                {},
                'Error applying diff from commit via "patch -p1 -s". Aborting...',
            )
            sys.exit(patch_cmd.returncode)
        command_context.repository.commit(
            patch["commit_msg"], patch["author"], patch["date"], [target_dir]
        )
        command_context.log(logging.INFO, "commit_pass", {}, "Committed successfully.")


def _file_bug(command_context, token, repo, pr_number):
    import requests

    bug = requests.post(
        "https://bugzilla.mozilla.org/rest/bug?api_key=%s" % token,
        json={
            "product": repo["bugzilla_product"],
            "component": repo["bugzilla_component"],
            "summary": "Land %s#%s in mozilla-central" % (repo["github"], pr_number),
            "version": "unspecified",
        },
    )
    bug.raise_for_status()
    command_context.log(logging.DEBUG, "new_bug", {}, bug.content)
    bugnumber = json.loads(bug.content)["id"]
    command_context.log(
        logging.INFO, "new_bug", {"bugnumber": bugnumber}, "Filed bug {bugnumber}"
    )
    return bugnumber


def _split_patches(patchfile, bug_number, pull_request, reviewer):
    INITIAL = 0
    HEADERS = 1
    STAT_AND_DIFF = 2

    patch = b""
    state = INITIAL
    for line in patchfile.splitlines():
        if state == INITIAL:
            if line.startswith(b"From "):
                state = HEADERS
        elif state == HEADERS:
            patch += line + b"\n"
            if line == b"---":
                state = STAT_AND_DIFF
        elif state == STAT_AND_DIFF:
            if line.startswith(b"From "):
                yield _parse_patch(patch, bug_number, pull_request, reviewer)
                patch = b""
                state = HEADERS
            else:
                patch += line + b"\n"
    if len(patch) > 0:
        yield _parse_patch(patch, bug_number, pull_request, reviewer)
    return


def _parse_patch(patch, bug_number, pull_request, reviewer):
    import email
    from email import header, policy

    parse_policy = policy.compat32.clone(max_line_length=None)
    parsed_mail = email.message_from_bytes(patch, policy=parse_policy)

    def header_as_unicode(key):
        decoded = header.decode_header(parsed_mail[key])
        return str(header.make_header(decoded))

    author = header_as_unicode("From")
    date = header_as_unicode("Date")
    commit_summary = header_as_unicode("Subject")
    email_body = parsed_mail.get_payload(decode=True).decode("utf-8")
    (commit_body, diff) = ("\n" + email_body).rsplit("\n---\n", 1)

    bug_prefix = ""
    if bug_number is not None:
        bug_prefix = "Bug %s - " % bug_number
    commit_summary = re.sub(r"^\[PATCH[0-9 /]*\] ", bug_prefix, commit_summary)
    if reviewer is not None:
        commit_summary += " r=" + reviewer

    commit_msg = commit_summary + "\n"
    if len(commit_body) > 0:
        commit_msg += commit_body + "\n"
    commit_msg += "\n[import_pr] From " + pull_request + "\n"

    patch_obj = {
        "author": author,
        "date": date,
        "commit_summary": commit_summary,
        "commit_msg": commit_msg,
        "diff": diff,
    }
    return patch_obj
