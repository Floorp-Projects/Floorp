#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import subprocess
import sys

import hglib
import pygit2

DEBUG = False


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def debugprint(*args, **kwargs):
    if DEBUG:
        eprint(*args, **kwargs)


class HgCommit:
    def __init__(self, parent1, parent2):
        self.parents = []
        if parent1 == NULL_PARENT_REV:
            raise Exception(
                "Encountered a hg changeset with no parents! We don't handle this...."
            )
        self.parents.append(parent1)
        if parent2 != NULL_PARENT_REV:
            self.parents.append(parent2)
        self.touches_sync_code = False
        self.children = []

    def add_child(self, rev):
        self.children.append(rev)


class GitCommit:
    def __init__(self, hg_rev, commit_obj):
        self.hg_rev = hg_rev
        self.commit_obj = commit_obj


def load_git_repository():
    commit_map = dict()
    # First, scan the tags for "mozilla-xxx" that keep track of manually synchronized changes
    sync_tags = filter(
        lambda ref: ref.startswith("refs/tags/mozilla-"),
        list(downstream_git_repo.references),
    )
    for desc in sync_tags:
        commit = downstream_git_repo.lookup_reference(desc).peel()
        # cut out the revision hash from the output
        hg_rev = desc[18:]
        commit_map[hg_rev] = GitCommit(hg_rev, commit)
        debugprint("Loaded pre-existing tag hg %s -> git %s" % (hg_rev, commit.oid))

    # Next, scan the commits for a specific message format
    re_commitmsg = re.compile(
        r"^\[(ghsync|wrupdater)\] From https://hg.mozilla.org/mozilla-central/rev/([0-9a-fA-F]+)$",
        re.MULTILINE,
    )
    for commit in downstream_git_repo.walk(downstream_git_repo.head.target):
        m = re_commitmsg.search(commit.message)
        if not m:
            continue
        hg_rev = m.group(2)
        commit_map[hg_rev] = GitCommit(hg_rev, commit)
        debugprint("Loaded pre-existing commit hg %s -> git %s" % (hg_rev, commit.oid))
    return commit_map


def timeof(git_commit):
    return git_commit.commit_obj.commit_time + git_commit.commit_obj.commit_time_offset


def find_newest_commit(commit_map):
    newest_hg_rev = None
    newest_commit_time = None

    for hg_rev, git_commit in commit_map.items():
        if newest_hg_rev is None or timeof(git_commit) > newest_commit_time:
            newest_hg_rev = hg_rev
            newest_commit_time = timeof(git_commit)

    return newest_hg_rev


def get_single_rev(revset):
    output = subprocess.check_output(
        ["hg", "log", "-r", revset, "--template", "{node}"]
    )
    output = str(output, "ascii")
    return output


def get_multiple_revs(revset, template):
    output = subprocess.check_output(
        ["hg", "log", "-r", revset, "--template", template + "\\n"]
    )
    for line in output.splitlines():
        yield str(line, "ascii")


def get_base_hg_rev(commit_map):
    base_hg_rev = find_newest_commit(commit_map)
    eprint("Using %s as base hg revision" % base_hg_rev)
    return base_hg_rev


def load_hg_commits(commits, query):
    for cset in get_multiple_revs(query, "{node} {p1node} {p2node}"):
        tokens = cset.split()
        commits[tokens[0]] = HgCommit(tokens[1], tokens[2])
    return commits


def get_real_base_hg_rev(hg_data, commit_map):
    # Some of the HG commits we want to port to github may have landed on codelines
    # that branched off central prior to base_hg_rev. So when we create the git
    # equivalents, they will have parents that are not the HEAD of the git repo,
    # but instead will be descendants of older commits in the git repo. In order
    # to do this correctly, we need to find the hg-equivalents of all of those
    # possible git parents. So first we identify all the "tail" hg revisions in
    # our hg_data set (think "tail" as in opposite of "head" which is the tipmost
    # commit). The "tail" hg revisions are the ones for which we don't have their
    # ancestors in hg_data.
    tails = []
    for rev, cset in hg_data.items():
        for parent in cset.parents:
            if parent not in hg_data:
                tails.append(rev)
    eprint("Found hg tail revisions %s" % tails)
    # Then we find their common ancestor, which will be some ancestor of base_hg_rev
    # from which those codelines.
    if len(tails) == 0:
        common_ancestor = get_single_rev(".")
    else:
        common_ancestor = get_single_rev("ancestor(" + ",".join(tails) + ")")
    eprint("Found common ancestor of tail revisions: %s" % common_ancestor)

    # And then we find the newest git commit whose hg-equivalent is an ancestor of
    # that common ancestor, to make sure we are starting from a known hg/git
    # commit pair.
    for git_commit in sorted(commit_map.values(), key=timeof, reverse=True):
        new_base = get_single_rev(
            "ancestor(" + common_ancestor + "," + git_commit.hg_rev + ")"
        )
        if new_base == common_ancestor:
            eprint(
                "Pre-existing git commit %s from hg rev %s is descendant of common ancestor; %s"
                % (
                    git_commit.commit_obj.id,
                    git_commit.hg_rev,
                    "walking back further...",
                )
            )
            continue
        if new_base != git_commit.hg_rev:
            eprint(
                "Pre-existing git commit %s from hg rev %s is on sibling branch"
                " of common ancestor; %s"
                % (
                    git_commit.commit_obj.id,
                    git_commit.hg_rev,
                    "walking back further...",
                )
            )
            continue
        eprint(
            "Pre-existing git commit %s from hg rev %s is sufficiently old; stopping walk"
            % (git_commit.commit_obj.id, git_commit.hg_rev)
        )
        common_ancestor = new_base
        break

    return common_ancestor


# Now we prune out all the uninteresting changesets from hg_commits. The
# uninteresting ones are ones that don't touch the target code, are not merges,
# and are not referenced by mozilla tags in the git repo.
# We do this by rewriting the parents to the "interesting" ancestor.
def prune_boring(rev):
    while rev in hg_commits:
        parent_pruned = False
        for i in range(len(hg_commits[rev].parents)):
            parent_rev = hg_commits[rev].parents[i]
            if parent_rev not in hg_commits:
                continue
            if hg_commits[parent_rev].touches_sync_code:
                continue
            if len(hg_commits[parent_rev].parents) > 1:
                continue
            if parent_rev in hg_to_git_commit_map:
                continue

            # If we get here, then `parent_rev` is a boring revision and we can
            # prune it. Connect `rev` to its grandparent, and prune the parent
            grandparent_rev = hg_commits[parent_rev].parents[0]
            hg_commits[rev].parents[i] = grandparent_rev
            # eprint("Pruned %s as boring parent of %s, using %s now" %
            #    (parent_rev, rev, grandparent_rev))
            parent_pruned = True

        if parent_pruned:
            # If we pruned a parent, process `rev` again as we might want to
            # prune more parents
            continue

        # Collapse identical parents, because if the parents are identical
        # we don't need to keep multiple copies of them.
        hg_commits[rev].parents = list(dict.fromkeys(hg_commits[rev].parents))

        # If we get here, all of `rev`s parents are interesting, so we can't
        # prune them. Move up to the parent rev and start processing that, or
        # if we have multiple parents then recurse on those nodes.
        if len(hg_commits[rev].parents) == 1:
            rev = hg_commits[rev].parents[0]
            continue

        for parent_rev in hg_commits[rev].parents:
            prune_boring(parent_rev)
        return


class FakeCommit:
    def __init__(self, oid):
        self.oid = oid


def fake_commit(hg_rev, parent1, parent2):
    if parent1 is None:
        eprint("ERROR: Trying to build on None")
        exit(1)
    oid = "githash_%s" % hash(parent1)
    eprint("Fake-built %s" % oid)
    return FakeCommit(oid)


def build_tree(builder, treedata):
    for name, value in treedata.items():
        if isinstance(value, dict):
            subbuilder = downstream_git_repo.TreeBuilder()
            build_tree(subbuilder, value)
            builder.insert(name, subbuilder.write(), pygit2.GIT_FILEMODE_TREE)
        else:
            (filemode, contents) = value
            blob_oid = downstream_git_repo.create_blob(contents)
            builder.insert(name, blob_oid, filemode)


def author_to_signature(author):
    pieces = author.strip().split("<")
    if len(pieces) != 2 or pieces[1][-1] != ">":
        # We could probably handle this better
        return pygit2.Signature(author, "")
    name = pieces[0].strip()
    email = pieces[1][:-1].strip()
    return pygit2.Signature(name, email)


def real_commit(hg_rev, parent1, parent2):
    filetree = dict()
    manifest = mozilla_hg_repo.manifest(rev=hg_rev)
    for nodeid, permission, executable, symlink, filename in manifest:
        if not filename.startswith(relative_path.encode("utf-8")):
            continue
        if symlink:
            filemode = pygit2.GIT_FILEMODE_LINK
        elif executable:
            filemode = pygit2.GIT_FILEMODE_BLOB_EXECUTABLE
        else:
            filemode = pygit2.GIT_FILEMODE_BLOB
        filecontent = mozilla_hg_repo.cat([filename], rev=hg_rev)
        subtree = filetree
        for component in filename.split(b"/")[2:-1]:
            subtree = subtree.setdefault(component.decode("latin-1"), dict())
        filename = filename.split(b"/")[-1]
        subtree[filename.decode("latin-1")] = (filemode, filecontent)

    builder = downstream_git_repo.TreeBuilder()
    build_tree(builder, filetree)
    tree_oid = builder.write()

    parent1_obj = downstream_git_repo.get(parent1)
    if parent1_obj.tree_id == tree_oid:
        eprint("Early-exit; tree matched that of parent git commit %s" % parent1)
        return parent1_obj

    if parent2 is not None:
        parent2_obj = downstream_git_repo.get(parent2)
        if parent2_obj.tree_id == tree_oid:
            eprint("Early-exit; tree matched that of parent git commit %s" % parent2)
            return parent2_obj

    hg_rev_obj = mozilla_hg_repo.log(revrange=hg_rev, limit=1)[0]
    commit_author = hg_rev_obj[4].decode("latin-1")
    commit_message = hg_rev_obj[5].decode("latin-1")
    commit_message += (
        "\n\n[ghsync] From https://hg.mozilla.org/mozilla-central/rev/%s" % hg_rev
        + "\n"
    )

    parents = [parent1]
    if parent2 is not None:
        parents.append(parent2)
    commit_oid = downstream_git_repo.create_commit(
        None,
        author_to_signature(commit_author),
        author_to_signature(commit_author),
        commit_message,
        tree_oid,
        parents,
    )
    eprint("Built git commit %s" % commit_oid)
    return downstream_git_repo.get(commit_oid)


def try_commit(hg_rev, parent1, parent2=None):
    if False:
        return fake_commit(hg_rev, parent1, parent2)
    else:
        return real_commit(hg_rev, parent1, parent2)


def build_git_commits(rev):
    debugprint("build_git_commit(%s)..." % rev)
    if rev in hg_to_git_commit_map:
        debugprint("  maps to %s" % hg_to_git_commit_map[rev].commit_obj.oid)
        return hg_to_git_commit_map[rev].commit_obj.oid

    if rev not in hg_commits:
        debugprint("  not in hg_commits")
        return None

    if len(hg_commits[rev].parents) == 1:
        git_parent = build_git_commits(hg_commits[rev].parents[0])
        if not hg_commits[rev].touches_sync_code:
            eprint(
                "WARNING: Found rev %s that is non-merge and not related to the target"
                % rev
            )
            return git_parent
        eprint("Building git equivalent for %s on top of %s" % (rev, git_parent))
        commit_obj = try_commit(rev, git_parent)
        hg_to_git_commit_map[rev] = GitCommit(rev, commit_obj)
        debugprint("  built %s as %s" % (rev, commit_obj.oid))
        return commit_obj.oid

    git_parent_1 = build_git_commits(hg_commits[rev].parents[0])
    git_parent_2 = build_git_commits(hg_commits[rev].parents[1])
    if git_parent_1 is None or git_parent_2 is None or git_parent_1 == git_parent_2:
        git_parent = git_parent_1 if git_parent_2 is None else git_parent_2
        if not hg_commits[rev].touches_sync_code:
            debugprint(
                "  %s is merge with no parents or doesn't touch WR, returning %s"
                % (rev, git_parent)
            )
            return git_parent

        eprint(
            "WARNING: Found merge rev %s whose parents have identical target code"
            ", but modifies the target" % rev
        )
        eprint("Building git equivalent for %s on top of %s" % (rev, git_parent))
        commit_obj = try_commit(rev, git_parent)
        hg_to_git_commit_map[rev] = GitCommit(rev, commit_obj)
        debugprint("  built %s as %s" % (rev, commit_obj.oid))
        return commit_obj.oid

    # An actual merge
    eprint(
        "Building git equivalent for %s on top of %s, %s"
        % (rev, git_parent_1, git_parent_2)
    )
    commit_obj = try_commit(rev, git_parent_1, git_parent_2)
    hg_to_git_commit_map[rev] = GitCommit(rev, commit_obj)
    debugprint("  built %s as %s" % (rev, commit_obj.oid))
    return commit_obj.oid


def pretty_print(rev, cset):
    desc = "  %s" % rev
    desc += " parents: %s" % cset.parents
    if rev in hg_to_git_commit_map:
        desc += " git: %s" % hg_to_git_commit_map[rev].commit_obj.oid
    if rev == hg_tip:
        desc += " (tip)"
    return desc


if len(sys.argv) < 3:
    eprint("Usage: %s <local-checkout-path> <repo-relative-path>" % sys.argv[0])
    eprint("Current dir must be the mozilla hg repo")
    exit(1)

local_checkout_path = sys.argv[1]
relative_path = sys.argv[2]
mozilla_hg_path = os.getcwd()
NULL_PARENT_REV = "0000000000000000000000000000000000000000"

downstream_git_repo = pygit2.Repository(pygit2.discover_repository(local_checkout_path))
mozilla_hg_repo = hglib.open(mozilla_hg_path)
hg_to_git_commit_map = load_git_repository()
base_hg_rev = get_base_hg_rev(hg_to_git_commit_map)
if base_hg_rev is None:
    eprint("Found no sync commits or 'mozilla-xxx' tags")
    exit(1)

hg_commits = load_hg_commits(dict(), "only(.," + base_hg_rev + ")")
eprint("Initial set has %s changesets" % len(hg_commits))
base_hg_rev = get_real_base_hg_rev(hg_commits, hg_to_git_commit_map)
eprint("Using hg rev %s as common ancestor of all interesting changesets" % base_hg_rev)

# Refresh hg_commits with our wider dataset
hg_tip = get_single_rev(".")
wider_range = "%s::%s" % (base_hg_rev, hg_tip)
hg_commits = load_hg_commits(hg_commits, wider_range)
eprint("Updated set has %s changesets" % len(hg_commits))

if DEBUG:
    eprint("Graph of descendants of %s" % base_hg_rev)
    output = subprocess.check_output(
        [
            "hg",
            "log",
            "--graph",
            "-r",
            "descendants(" + base_hg_rev + ")",
            "--template",
            "{node} {desc|firstline}\\n",
        ]
    )
    for line in output.splitlines():
        eprint(line.decode("utf-8", "ignore"))

# Also flag any changes that touch the project
query = "(" + wider_range + ') & file("glob:' + relative_path + '/**")'
for cset in get_multiple_revs(query, "{node}"):
    debugprint("Changeset %s modifies %s" % (cset, relative_path))
    hg_commits[cset].touches_sync_code = True
eprint(
    "Identified %s changesets that touch the target code"
    % sum([1 if v.touches_sync_code else 0 for (k, v) in hg_commits.items()])
)

prune_boring(hg_tip)

# hg_tip itself might be boring
if not hg_commits[hg_tip].touches_sync_code and len(hg_commits[hg_tip].parents) == 1:
    new_tip = hg_commits[hg_tip].parents[0]
    eprint("Pruned tip %s as boring, using %s now" % (hg_tip, new_tip))
    hg_tip = new_tip

eprint("--- Interesting changesets ---")
for rev, cset in hg_commits.items():
    if cset.touches_sync_code or len(cset.parents) > 1 or rev in hg_to_git_commit_map:
        eprint(pretty_print(rev, cset))
if DEBUG:
    eprint("--- Other changesets (not really interesting) ---")
    for rev, cset in hg_commits.items():
        if not (
            cset.touches_sync_code
            or len(cset.parents) > 1
            or rev in hg_to_git_commit_map
        ):
            eprint(pretty_print(rev, cset))

git_tip = build_git_commits(hg_tip)
if git_tip is None:
    eprint("No new changesets generated, exiting.")
else:
    downstream_git_repo.create_reference("refs/heads/github-sync", git_tip, force=True)
    eprint("Updated github-sync branch to %s, done!" % git_tip)
