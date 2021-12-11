# Github synchronization scripts

This tool aims to help synchronizing changes from mozilla-central to Github on pushes.
This is useful for Gecko sub-projects that have Github mirrors, like `gfx/wr` linking to `https://github.com/servo/webrender`.
Originally, the tools were developed in `https://github.com/staktrace/wrupdater`,
then got moved under `gfx/wr/ci-scripts/wrupdater`,
and finally migrated here while also abstracting away from WebRender specifically.

The main entry point is the `sync-to-github.sh` script that is called with the following arguments:
  1. name of the project, matching the repository under `https://github.com/moz-gfx` user (e.g. `webrender`)
  2. relative folder in mozilla-central, which is the upstream for the changes (e.g. `gfx/wr`)
  3. downstream repository specified as "organization/project-name" (e.g. `servo/webrender`)
  4. name to call for auto-approving the pull request (e.g. `bors` or `@bors-servo`)

It creates a staging directory at `~/.ghsync` if one doesn't already exist,
and clones the the downstream repo into it.
The script also requires the `GECKO_PATH` environment variable
to point to a mercurial clone of `mozilla-central`, and access to the
taskcluster secrets service to get a Github API token.

The `sync-to-github.sh` script does some setup steps but the bulk of the actual work
is done by the `converter.py` script. This script scans the mercurial
repository for new changes to the relative folder in m-c,
and adds commits to the git repository corresponding to those changes.
There are some details in the implementation that make it more robust
than simply exporting patches and attempting to reapply them;
in particular it builds a commit tree structure that mirrors what is found in
the `mozilla-central` repository with respect to branches and merges.
So if conflicting changes land on autoland and inbound, and then get
merged, the git repository commits will have the same structure with
a fork/merge in the commit history. This was discovered to be
necessary after a previous version ran into multiple cases where
the simple patch approach didn't really work.

One of the actions the `converter.py` takes is to find the last sync point
between Github and mozilla-central. This is done based on the following markers:
  - commit message containing the string "[ghsync] From https://hg.mozilla.org/mozilla-central/rev/xxx"
  - commit message containing the string "[wrupdater] From https://hg.mozilla.org/mozilla-central/rev/xxx"
  - commit with tag "mozilla-xxx"
(where xxx is always a mozilla-central hg revision identifier).

Once the converter is done converting, the `sync-to-github.sh` script
finishes the process by pushing the new commits to the `github-sync` branch
of the `https://github.com/moz-gfx/<project-name>` repository,
and generating a pull request against the downstream repository. It also
leaves a comment on the PR that triggers testing and automatic merge of the PR.
If there is already a pull request (perhaps from a previous run) the
pre-existing PR is force-updated instead. This allows for graceful
handling of scenarios where the PR failed to get merged (e.g. due to
CI failures on the Github side).

The script is intended to by run by taskcluster for any changes that
touch the relative folder that land on `mozilla-central`. This may mean
that multiple instances of this script run concurrently, or even out
of order (i.e. the task for an older m-c push runs after the task for
a newer m-c push). The script was written with these possibilities in
mind and should be able to eventually recover from any such scenario
automatically (although it may take additional changes to mozilla-central
for such recovery to occur). That being said, the number of pathological
scenarios here is quite large and they were not really tested.

## Ownership and access

When this tool is run in Firefox CI, it needs to have push permissions to
the `moz-gfx` github user's account. It gets this permission via a secret token
stored in the Firefox CI taskcluster secrets service. If you need to update
the token, you need to find somebody who is a member of the
[webrender-ci access group](https://people.mozilla.org/a/webrender-ci/). The
Google Drive associated with that access group has additional documentation
on the `moz-gfx` github user and the secret token.

## Debugging

To debug the converter.py script, you need to have a hg checkout of
mozilla-central, let's assume it's at $MOZILLA. First create a virtualenv
with the right dependencies installed:

```
mkdir -p $HOME/.ghsync
virtualenv --python=python3 $HOME/.ghsync/venv
source $HOME/.ghsync/venv/bin/activate
pip3 install -r $MOZILLA/taskcluster/docker/github-sync/requirements.txt
```

Also create a checkout of the downstream github repo and set up a `github-sync`
branch to the point where you want port commits to. For example, for WebRender
you'd do:

```
cd $HOME/.ghsync
git clone https://github.com/servo/webrender
cd webrender
git checkout -b github-sync master
```

(You can set the github-sync branch to a past revision if you want to replicate
a failure that already got committed).

Then run the converter from your hg checkout:

```
cd $MOZILLA
tools/github-sync/converter.py $HOME/.ghsync/webrender gfx/wr
```

You can set the DEBUG variable in the script to True to get more output.
