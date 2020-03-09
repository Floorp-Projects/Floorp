# coding: utf-8
from __future__ import absolute_import, division, print_function, unicode_literals

import collections
import hashlib
import os
from contextlib import contextmanager
from functools import partial
from shutil import rmtree

from .._compat import (
    FAVORITE_HASH,
    PIP_VERSION,
    Link,
    PyPI,
    RequirementSet,
    Resolver as PipResolver,
    TemporaryDirectory,
    Wheel,
    WheelCache,
    contextlib,
    get_requirement_tracker,
    global_tempdir_manager,
    is_dir_url,
    is_file_url,
    is_vcs_url,
    normalize_path,
    path_to_url,
    url_to_path,
)
from ..click import progressbar
from ..exceptions import NoCandidateFound
from ..logging import log
from ..utils import (
    create_install_command,
    fs_str,
    is_pinned_requirement,
    is_url_requirement,
    lookup_table,
    make_install_requirement,
)
from .base import BaseRepository

FILE_CHUNK_SIZE = 4096
FileStream = collections.namedtuple("FileStream", "stream size")


class PyPIRepository(BaseRepository):
    DEFAULT_INDEX_URL = PyPI.simple_url

    """
    The PyPIRepository will use the provided Finder instance to lookup
    packages.  Typically, it looks up packages on PyPI (the default implicit
    config), but any other PyPI mirror can be used if index_urls is
    changed/configured on the Finder.
    """

    def __init__(self, pip_args, cache_dir, build_isolation=False):
        self.build_isolation = build_isolation

        # Use pip's parser for pip.conf management and defaults.
        # General options (find_links, index_url, extra_index_url, trusted_host,
        # and pre) are deferred to pip.
        self.command = create_install_command()
        self.options, _ = self.command.parse_args(pip_args)
        if self.options.cache_dir:
            self.options.cache_dir = normalize_path(self.options.cache_dir)

        self.session = self.command._build_session(self.options)
        self.finder = self.command._build_package_finder(
            options=self.options, session=self.session
        )

        # Caches
        # stores project_name => InstallationCandidate mappings for all
        # versions reported by PyPI, so we only have to ask once for each
        # project
        self._available_candidates_cache = {}

        # stores InstallRequirement => list(InstallRequirement) mappings
        # of all secondary dependencies for the given requirement, so we
        # only have to go to disk once for each requirement
        self._dependencies_cache = {}

        # Setup file paths
        self.freshen_build_caches()
        self._cache_dir = normalize_path(cache_dir)
        self._download_dir = fs_str(os.path.join(self._cache_dir, "pkgs"))
        self._wheel_download_dir = fs_str(os.path.join(self._cache_dir, "wheels"))

    def freshen_build_caches(self):
        """
        Start with fresh build/source caches.  Will remove any old build
        caches from disk automatically.
        """
        self._build_dir = TemporaryDirectory(fs_str("build"))
        self._source_dir = TemporaryDirectory(fs_str("source"))

    @property
    def build_dir(self):
        return self._build_dir.name

    @property
    def source_dir(self):
        return self._source_dir.name

    def clear_caches(self):
        rmtree(self._download_dir, ignore_errors=True)
        rmtree(self._wheel_download_dir, ignore_errors=True)

    def find_all_candidates(self, req_name):
        if req_name not in self._available_candidates_cache:
            candidates = self.finder.find_all_candidates(req_name)
            self._available_candidates_cache[req_name] = candidates
        return self._available_candidates_cache[req_name]

    def find_best_match(self, ireq, prereleases=None):
        """
        Returns a Version object that indicates the best match for the given
        InstallRequirement according to the external repository.
        """
        if ireq.editable or is_url_requirement(ireq):
            return ireq  # return itself as the best match

        all_candidates = self.find_all_candidates(ireq.name)
        candidates_by_version = lookup_table(
            all_candidates, key=lambda c: c.version, unique=True
        )
        matching_versions = ireq.specifier.filter(
            (candidate.version for candidate in all_candidates), prereleases=prereleases
        )

        # Reuses pip's internal candidate sort key to sort
        matching_candidates = [candidates_by_version[ver] for ver in matching_versions]
        if not matching_candidates:
            raise NoCandidateFound(ireq, all_candidates, self.finder)

        if PIP_VERSION < (19, 1):
            best_candidate = max(
                matching_candidates, key=self.finder._candidate_sort_key
            )
        elif PIP_VERSION < (19, 2):
            evaluator = self.finder.candidate_evaluator
            best_candidate = evaluator.get_best_candidate(matching_candidates)
        elif PIP_VERSION < (19, 3):
            evaluator = self.finder.make_candidate_evaluator(ireq.name)
            best_candidate = evaluator.get_best_candidate(matching_candidates)
        else:
            evaluator = self.finder.make_candidate_evaluator(ireq.name)
            best_candidate_result = evaluator.compute_best_candidate(
                matching_candidates
            )
            best_candidate = best_candidate_result.best_candidate

        if PIP_VERSION[:2] <= (19, 3):
            best_candidate_name = best_candidate.project
        else:
            best_candidate_name = best_candidate.name

        # Turn the candidate into a pinned InstallRequirement
        return make_install_requirement(
            best_candidate_name,
            best_candidate.version,
            ireq.extras,
            constraint=ireq.constraint,
        )

    def resolve_reqs(self, download_dir, ireq, wheel_cache):
        results = None

        if PIP_VERSION < (10,):
            reqset = RequirementSet(
                self.build_dir,
                self.source_dir,
                download_dir=download_dir,
                wheel_download_dir=self._wheel_download_dir,
                session=self.session,
                wheel_cache=wheel_cache,
            )
            results = reqset._prepare_file(self.finder, ireq)
        else:
            from pip._internal.operations.prepare import RequirementPreparer

            preparer_kwargs = {
                "build_dir": self.build_dir,
                "src_dir": self.source_dir,
                "download_dir": download_dir,
                "wheel_download_dir": self._wheel_download_dir,
                "progress_bar": "off",
                "build_isolation": self.build_isolation,
            }
            resolver_kwargs = {
                "finder": self.finder,
                "session": self.session,
                "upgrade_strategy": "to-satisfy-only",
                "force_reinstall": False,
                "ignore_dependencies": False,
                "ignore_requires_python": False,
                "ignore_installed": True,
                "use_user_site": False,
            }
            make_install_req_kwargs = {"isolated": False, "wheel_cache": wheel_cache}

            if PIP_VERSION < (19, 3):
                resolver_kwargs.update(**make_install_req_kwargs)
            else:
                from pip._internal.req.constructors import install_req_from_req_string

                make_install_req = partial(
                    install_req_from_req_string, **make_install_req_kwargs
                )
                resolver_kwargs["make_install_req"] = make_install_req

            if PIP_VERSION >= (20,):
                del resolver_kwargs["session"]
                del preparer_kwargs["progress_bar"]

            resolver = None
            preparer = None

            if PIP_VERSION[:2] <= (19, 3):
                tmp_dir_cm = contextlib.nullcontext()
            else:
                from pip._internal.utils.temp_dir import TempDirectory

                tmp_dir_cm = TempDirectory(kind="req-tracker")

            with get_requirement_tracker() as req_tracker, tmp_dir_cm as temp_build_dir:
                # Pip 18 uses a requirement tracker to prevent fork bombs
                if req_tracker:
                    preparer_kwargs["req_tracker"] = req_tracker

                if PIP_VERSION[:2] <= (19, 3):
                    preparer = RequirementPreparer(**preparer_kwargs)
                else:
                    preparer = self.command.make_requirement_preparer(
                        temp_build_dir=temp_build_dir,
                        options=self.options,
                        req_tracker=req_tracker,
                        session=self.session,
                        finder=self.finder,
                        use_user_site=self.options.use_user_site,
                    )

                resolver_kwargs["preparer"] = preparer
                reqset = RequirementSet()
                ireq.is_direct = True
                reqset.add_requirement(ireq)

                resolver = PipResolver(**resolver_kwargs)
                resolver.require_hashes = False
                results = resolver._resolve_one(reqset, ireq)

                reqset.cleanup_files()

        return set(results)

    def get_dependencies(self, ireq):
        """
        Given a pinned, URL, or editable InstallRequirement, returns a set of
        dependencies (also InstallRequirements, but not necessarily pinned).
        They indicate the secondary dependencies for the given requirement.
        """
        if not (
            ireq.editable or is_url_requirement(ireq) or is_pinned_requirement(ireq)
        ):
            raise TypeError(
                "Expected url, pinned or editable InstallRequirement, got {}".format(
                    ireq
                )
            )

        if ireq not in self._dependencies_cache:
            if ireq.editable and (ireq.source_dir and os.path.exists(ireq.source_dir)):
                # No download_dir for locally available editable requirements.
                # If a download_dir is passed, pip will  unnecessarely
                # archive the entire source directory
                download_dir = None
            elif ireq.link and is_vcs_url(ireq.link):
                # No download_dir for VCS sources.  This also works around pip
                # using git-checkout-index, which gets rid of the .git dir.
                download_dir = None
            else:
                download_dir = self._download_dir
                if not os.path.isdir(download_dir):
                    os.makedirs(download_dir)
            if not os.path.isdir(self._wheel_download_dir):
                os.makedirs(self._wheel_download_dir)

            wheel_cache = WheelCache(self._cache_dir, self.options.format_control)
            prev_tracker = os.environ.get("PIP_REQ_TRACKER")
            try:
                with global_tempdir_manager():
                    self._dependencies_cache[ireq] = self.resolve_reqs(
                        download_dir, ireq, wheel_cache
                    )
            finally:
                if "PIP_REQ_TRACKER" in os.environ:
                    if prev_tracker:
                        os.environ["PIP_REQ_TRACKER"] = prev_tracker
                    else:
                        del os.environ["PIP_REQ_TRACKER"]

                # WheelCache.cleanup() introduced in pip==10.0.0
                if PIP_VERSION >= (10,):
                    wheel_cache.cleanup()
        return self._dependencies_cache[ireq]

    def get_hashes(self, ireq):
        """
        Given an InstallRequirement, return a set of hashes that represent all
        of the files for a given requirement. Unhashable requirements return an
        empty set. Unpinned requirements raise a TypeError.
        """

        if ireq.link:
            link = ireq.link

            if is_vcs_url(link) or (is_file_url(link) and is_dir_url(link)):
                # Return empty set for unhashable requirements.
                # Unhashable logic modeled on pip's
                # RequirementPreparer.prepare_linked_requirement
                return set()

            if is_url_requirement(ireq):
                # Directly hash URL requirements.
                # URL requirements may have been previously downloaded and cached
                # locally by self.resolve_reqs()
                cached_path = os.path.join(self._download_dir, link.filename)
                if os.path.exists(cached_path):
                    cached_link = Link(path_to_url(cached_path))
                else:
                    cached_link = link
                return {self._get_file_hash(cached_link)}

        if not is_pinned_requirement(ireq):
            raise TypeError("Expected pinned requirement, got {}".format(ireq))

        # We need to get all of the candidates that match our current version
        # pin, these will represent all of the files that could possibly
        # satisfy this constraint.
        all_candidates = self.find_all_candidates(ireq.name)
        candidates_by_version = lookup_table(all_candidates, key=lambda c: c.version)
        matching_versions = list(
            ireq.specifier.filter((candidate.version for candidate in all_candidates))
        )
        matching_candidates = candidates_by_version[matching_versions[0]]

        log.debug("  {}".format(ireq.name))

        def get_candidate_link(candidate):
            if PIP_VERSION < (19, 2):
                return candidate.location
            return candidate.link

        return {
            self._get_file_hash(get_candidate_link(candidate))
            for candidate in matching_candidates
        }

    def _get_file_hash(self, link):
        log.debug("    Hashing {}".format(link.url_without_fragment))
        h = hashlib.new(FAVORITE_HASH)
        with open_local_or_remote_file(link, self.session) as f:
            # Chunks to iterate
            chunks = iter(lambda: f.stream.read(FILE_CHUNK_SIZE), b"")

            # Choose a context manager depending on verbosity
            if log.verbosity >= 1:
                iter_length = f.size / FILE_CHUNK_SIZE if f.size else None
                context_manager = progressbar(chunks, length=iter_length, label="  ")
            else:
                context_manager = contextlib.nullcontext(chunks)

            # Iterate over the chosen context manager
            with context_manager as bar:
                for chunk in bar:
                    h.update(chunk)
        return ":".join([FAVORITE_HASH, h.hexdigest()])

    @contextmanager
    def allow_all_wheels(self):
        """
        Monkey patches pip.Wheel to allow wheels from all platforms and Python versions.

        This also saves the candidate cache and set a new one, or else the results from
        the previous non-patched calls will interfere.
        """

        def _wheel_supported(self, tags=None):
            # Ignore current platform. Support everything.
            return True

        def _wheel_support_index_min(self, tags=None):
            # All wheels are equal priority for sorting.
            return 0

        original_wheel_supported = Wheel.supported
        original_support_index_min = Wheel.support_index_min
        original_cache = self._available_candidates_cache

        Wheel.supported = _wheel_supported
        Wheel.support_index_min = _wheel_support_index_min
        self._available_candidates_cache = {}

        try:
            yield
        finally:
            Wheel.supported = original_wheel_supported
            Wheel.support_index_min = original_support_index_min
            self._available_candidates_cache = original_cache


@contextmanager
def open_local_or_remote_file(link, session):
    """
    Open local or remote file for reading.

    :type link: pip.index.Link
    :type session: requests.Session
    :raises ValueError: If link points to a local directory.
    :return: a context manager to a FileStream with the opened file-like object
    """
    url = link.url_without_fragment

    if is_file_url(link):
        # Local URL
        local_path = url_to_path(url)
        if os.path.isdir(local_path):
            raise ValueError("Cannot open directory for read: {}".format(url))
        else:
            st = os.stat(local_path)
            with open(local_path, "rb") as local_file:
                yield FileStream(stream=local_file, size=st.st_size)
    else:
        # Remote URL
        headers = {"Accept-Encoding": "identity"}
        response = session.get(url, headers=headers, stream=True)

        # Content length must be int or None
        try:
            content_length = int(response.headers["content-length"])
        except (ValueError, KeyError, TypeError):
            content_length = None

        try:
            yield FileStream(stream=response.raw, size=content_length)
        finally:
            response.close()
