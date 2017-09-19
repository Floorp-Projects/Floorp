Releasing geckodriver
=====================

Releasing geckodriver is not as easy as it once used to be when the
project’s canonical home was on GitHub.  Today geckodriver is hosted
in [mozilla-central], and whilst we do want to make future releases
from [Mozilla’s CI infrastructure], we are currently in between two
worlds: development happens in m-c, but releases continue to be made
from GitHub using Travis.

The reason for this is that we do not compile geckodriver for all
our target platforms, that Rust cross-compilation on TaskCluster
builders is somewhat broken, and that tests are not run in automation.
We intend to fix all these problems.

In any case, the steps to release geckodriver are as follows:

[mozilla-central]: https://hg.mozilla.org/mozilla-central/
[Mozilla’s CI infrastructure]: https://treeherder.mozilla.org/


Release new webdriver crate
---------------------------

geckodriver depends on the [webdriver] crate, also hosted in
mozilla-central, by pointing to its in-tree relative path:

	[dependencies]
	webdriver = { path = "../webdriver" }

Because we need to export the geckodriver source code to the old GitHub
repository in order to release, we need to publish any changes that
have been made to webdriver in the interim.  If no changes have been
made, you can skip these steps:

  1. Bump the version number in testing/webdriver/Cargo.toml
  2. `cargo publish`

[webdriver]: ../webdriver


Update the change log
---------------------

Notable changes to geckodriver are mentioned in [CHANGES.md]. Many
users rely on this, so it’s important that you make it **relevant
to end-users**.  For example, we only mention changes that are visible
to users.  The change log is not a complete anthology of commits,
as these often will not convey the essence of a change to end-users.
If a feature was added but removed before release, there is no reason
to list it as a change.

It is good practice to also include relevant information from the
[webdriver] and [rust-mozrunner] crates, since these are the two most
important dependencies of geckodriver and a lot of its functionality
is implemented there.

We follow the writing style of the existing change log, with
one section per version (with a release date), with subsections
‘Added’, ‘Changed’, and ‘Removed’.  If the targetted
Firefox or Selenium versions have changed, it is good to make a
mention of this.  Lines are optimally formatted at roughly 72 columns
to make the file readable in a text editor as well as rendered HTML.
fmt(1) does a splendid job at text formatting.

[CHANGES.md]: ../CHANGES.md
[webdriver]: ../../webdriver
[rust-mozrunner]: https://github.com/jgraham/rust_mozrunner


Update libraries
----------------

Make relevant changes to [Cargo.toml] to upgrade dependencies, then run

	% ./mach vendor rust
	% ./mach build testing/geckodriver

to pull down and vendor the upgraded libraries.  Remember to check
in the [Cargo.lock] file, since unlike we want geckodriver builds to
be reproducible.

Updating dependencies should always be made as a separate commit to
not confuse reviewers because vendoring involves checking in a lot
of extra code reviewed downstream.

[Cargo.toml]: ../Cargo.toml
[Cargo.lock]: ../Cargo.lock


Bump the version number
-----------------------

Bump the version number in [Cargo.toml] to the next version.
geckodriver follows [semantic versioning] so it’s a good idea to
familiarise yourself wih that before deciding on the version number.

After you’ve changed the version number, run

	% ./mach build testing/geckodriver

again to update [Cargo.lock], and check in the file.

[semantic versioning]: http://semver.org/


Export to GitHub
----------------

The canonical GitHub repository is

	https://github.com/mozilla/geckodriver.git

so make sure you have a local clone of that.  It has three branches:
_master_ which only contains the [README.md]; _old_ which was the
state of the project when it was exported to mozilla-central; and
_release_, from where releases are made.  We will export the contents
of [testing/geckodriver] to the latter branch:

	% cd $SRC/geckodriver
	% git checkout release
	% git rm -rf .
	% git clean -fxd
	% cp -r $SRC/gecko/testing/geckodriver/* .
	% git add .
	% git commit -am "import of vX.Y.Z"

[README]: ../README.md
[testing/geckodriver]: ../


Manually change `webdriver` dependency
--------------------------------------

After the source code has been imported we need to change the
dependency information for the [webdriver] crate.  As explained
previously geckodriver depends on a relative path in in the
mozilla-central repository to build with the latest unreleased
source code.

This relative path does not exist in the GitHub repository and the
build will fail unless we change it to the latest [webdriver] crate
version from crates.io.  That version will either be the crate you
published earlier, or the latest version available if no changes have
been made to it since the last geckodriver release.


Tag the release
---------------

Run the following command:

	% git tag -a 'vX.Y.Z'

Write the following in the annotation:

	Tagging release vX.Y.Z


Make the release
----------------

geckodriver is released and automatically uploaded from Travis by
pushing a new version tag to the _release_ branch:

	% git push
	% git push --tags


Update the release description
------------------------------

Copy the raw Markdown source from [CHANGES.md] into the description
of the [latest release].  This will highlight for end-users what
changes were made in that particular package when they visit the
GitHub downloads section.

Congratulations!  You’ve released geckodriver!

[latest release]: https://github.com/mozilla/geckodriver/releases


Future work
-----------

In the future, we intend to [sign releases] so that they are
verifiable.

[sign releases]: https://github.com/mozilla/geckodriver/issues/292
