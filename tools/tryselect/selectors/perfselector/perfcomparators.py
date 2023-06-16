# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import importlib
import inspect
import pathlib

BUILTIN_COMPARATORS = {}


class ComparatorNotFound(Exception):
    """Raised when we can't find the specified comparator.

    Triggered when either the comparator name is incorrect for a builtin one,
    or when a path to a specified comparator cannot be found.
    """

    pass


class GithubRequestFailure(Exception):
    """Raised when we hit a failure during PR link parsing."""

    pass


class BadComparatorArgs(Exception):
    """Raised when the args given to the comparator are incorrect."""

    pass


def comparator(comparator_klass):
    BUILTIN_COMPARATORS[comparator_klass.__name__] = comparator_klass
    return comparator_klass


@comparator
class BasePerfComparator:
    def __init__(self, vcs, compare_commit, current_revision_ref, comparator_args):
        """Initialize the standard/default settings for Comparators.

        :param vcs object: Used for updating the local repo.
        :param compare_commit str: The base revision found for the local repo.
        :param current_revision_ref str: The current revision of the local repo.
        :param comparator_args list: List of comparator args in the format NAME=VALUE.
        """
        self.vcs = vcs
        self.compare_commit = compare_commit
        self.current_revision_ref = current_revision_ref
        self.comparator_args = comparator_args

        # Used to ensure that the local repo gets cleaned up appropriately on failures
        self._updated = False

    def setup_base_revision(self, extra_args):
        """Setup the base try run/revision.

        In this case, we update to the repo to the base revision and
        push that to try. The extra_args can be used to set additional
        arguments for Raptor (not available for other harnesses).

        :param extra_args list: A list of extra arguments to pass to the try tasks.
        """
        self.vcs.update(self.compare_commit)
        self._updated = True

    def teardown_base_revision(self):
        """Teardown the setup for the base revision."""
        if self._updated:
            self.vcs.update(self.current_revision_ref)
            self._updated = False

    def setup_new_revision(self, extra_args):
        """Setup the new try run/revision.

        Note that the extra_args are reset between the base, and new revision runs.

        :param extra_args list: A list of extra arguments to pass to the try tasks.
        """
        pass

    def teardown_new_revision(self):
        """Teardown the new run/revision setup."""
        pass

    def teardown(self):
        """Teardown for failures.

        This method can be used for ensuring that the repo is cleaned up
        when a failure is hit at any point in the process of doing the
        new/base revision setups, or the pushes to try.
        """
        self.teardown_base_revision()


def get_github_pull_request_info(link):
    """Returns information about a PR link.

    This method accepts a Github link in either of these formats:
        https://github.com/mozilla-mobile/firefox-android/pull/1627,
        https://github.com/mozilla-mobile/firefox-android/pull/1876/commits/17c7350cc37a4a85cea140a7ce54e9fd037b5365 #noqa

    and returns the Github link, branch, and revision of the commit.
    """
    from urllib.parse import urlparse

    import requests

    # Parse the url, and get all the necessary info
    parsed_url = urlparse(link)
    path_parts = parsed_url.path.strip("/").split("/")
    owner, repo = path_parts[0], path_parts[1]
    pr_number = path_parts[-1]

    if "/pull/" not in parsed_url.path:
        raise GithubRequestFailure(
            f"Link for Github PR is invalid (missing /pull/): {link}"
        )

    # Get the commit being targeted in the PR
    pr_commit = None
    if "/commits/" in parsed_url.path:
        pr_commit = path_parts[-1]
        pr_number = path_parts[-3]

    # Make the request, and get the PR info, otherwise,
    # raise an exception if the response code is not 200
    api_url = f"https://api.github.com/repos/{owner}/{repo}/pulls/{pr_number}"
    response = requests.get(api_url)
    if response.status_code == 200:
        link_info = response.json()
        return (
            link_info["head"]["repo"]["html_url"],
            pr_commit if pr_commit else link_info["head"]["sha"],
            link_info["head"]["ref"],
        )

    raise GithubRequestFailure(
        f"The following url returned a non-200 status code: {api_url}"
    )


@comparator
class BenchmarkComparator(BasePerfComparator):
    def _get_benchmark_info(self, arg_prefix):
        # Get the flag from the comparator args
        benchmark_info = {"repo": None, "branch": None, "revision": None, "link": None}
        for arg in self.comparator_args:
            if arg.startswith(arg_prefix):
                _, settings = arg.split(arg_prefix)
                setting, val = settings.split("=")
                if setting not in benchmark_info:
                    raise BadComparatorArgs(
                        f"Unknown argument provided `{setting}`. Only the following "
                        f"are available (prefixed with `{arg_prefix}`): "
                        f"{list(benchmark_info.keys())}"
                    )
                benchmark_info[setting] = val

        # Parse the link for any required information
        if benchmark_info.get("link", None) is not None:
            (
                benchmark_info["repo"],
                benchmark_info["revision"],
                benchmark_info["branch"],
            ) = get_github_pull_request_info(benchmark_info["link"])

        return benchmark_info

    def _setup_benchmark_args(self, extra_args, benchmark_info):
        # Setup the arguments for Raptor
        extra_args.append(f"benchmark-repository={benchmark_info['repo']}")
        extra_args.append(f"benchmark-revision={benchmark_info['revision']}")

        if benchmark_info.get("branch", None):
            extra_args.append(f"benchmark-branch={benchmark_info['branch']}")

    def setup_base_revision(self, extra_args):
        """Sets up the options for a base benchmark revision run.

        Checks for a `base-link` in the
        command and adds the appropriate commands to the extra_args
        which will be added to the PERF_FLAGS environment variable.

        If that isn't provided, then you must provide the repo, branch,
        and revision directly through these (branch is optional):

            base-repo=https://github.com/mozilla-mobile/firefox-android
            base-branch=main
            base-revision=17c7350cc37a4a85cea140a7ce54e9fd037b5365

        Otherwise, we'll use the default mach try perf
        base behaviour.

        TODO: Get the information automatically from a commit link. Github
        API doesn't provide the branch name from a link like that.
        """
        base_info = self._get_benchmark_info("base-")

        # If no options were provided, use the default BasePerfComparator behaviour
        if not any(v is not None for v in base_info.values()):
            raise BadComparatorArgs(
                f"Could not find the correct base-revision arguments in: {self.comparator_args}"
            )

        self._setup_benchmark_args(extra_args, base_info)

    def setup_new_revision(self, extra_args):
        """Sets up the options for a new benchmark revision run.

        Same as `setup_base_revision`, except it uses
         `new-` as the prefix instead of `base-`.
        """
        new_info = self._get_benchmark_info("new-")

        # If no options were provided, use the default BasePerfComparator behaviour
        if not any(v is not None for v in new_info.values()):
            raise BadComparatorArgs(
                f"Could not find the correct new-revision arguments in: {self.comparator_args}"
            )

        self._setup_benchmark_args(extra_args, new_info)


def get_comparator(comparator):
    if comparator in BUILTIN_COMPARATORS:
        return BUILTIN_COMPARATORS[comparator]

    file = pathlib.Path(comparator)
    if not file.exists():
        raise ComparatorNotFound(
            f"Expected either a path to a file containing a comparator, or a "
            f"builtin comparator from this list: {BUILTIN_COMPARATORS.keys()}"
        )

    # Importing a source file directly
    spec = importlib.util.spec_from_file_location(name=file.name, location=comparator)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    members = inspect.getmembers(
        module,
        lambda c: inspect.isclass(c)
        and issubclass(c, BasePerfComparator)
        and c != BasePerfComparator,
    )

    if not members:
        raise ComparatorNotFound(
            f"The path {comparator} was found but it was not a valid comparator. "
            f"Ensure it is a subclass of BasePerfComparator and optionally contains the  "
            f"following methods: "
            f"{', '.join(inspect.getmembers(BasePerfComparator, predicate=inspect.ismethod))}"
        )

    return members[0][-1]
