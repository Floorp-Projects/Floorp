# coding: utf-8
from __future__ import absolute_import, division, print_function, unicode_literals

from contextlib import contextmanager

from pip._internal.utils.hashes import FAVORITE_HASH

from piptools.utils import as_tuple, key_from_ireq, make_install_requirement

from .base import BaseRepository


def ireq_satisfied_by_existing_pin(ireq, existing_pin):
    """
    Return True if the given InstallationRequirement is satisfied by the
    previously encountered version pin.
    """
    version = next(iter(existing_pin.req.specifier)).version
    return ireq.req.specifier.contains(
        version, prereleases=existing_pin.req.specifier.prereleases
    )


class LocalRequirementsRepository(BaseRepository):
    """
    The LocalRequirementsRepository proxied the _real_ repository by first
    checking if a requirement can be satisfied by existing pins (i.e. the
    result of a previous compile step).

    In effect, if a requirement can be satisfied with a version pinned in the
    requirements file, we prefer that version over the best match found in
    PyPI.  This keeps updates to the requirements.txt down to a minimum.
    """

    def __init__(self, existing_pins, proxied_repository, reuse_hashes=True):
        self._reuse_hashes = reuse_hashes
        self.repository = proxied_repository
        self.existing_pins = existing_pins

    @property
    def options(self):
        return self.repository.options

    @property
    def finder(self):
        return self.repository.finder

    @property
    def session(self):
        return self.repository.session

    @property
    def DEFAULT_INDEX_URL(self):
        return self.repository.DEFAULT_INDEX_URL

    def clear_caches(self):
        self.repository.clear_caches()

    @contextmanager
    def freshen_build_caches(self):
        with self.repository.freshen_build_caches():
            yield

    def find_best_match(self, ireq, prereleases=None):
        key = key_from_ireq(ireq)
        existing_pin = self.existing_pins.get(key)
        if existing_pin and ireq_satisfied_by_existing_pin(ireq, existing_pin):
            project, version, _ = as_tuple(existing_pin)
            return make_install_requirement(
                project, version, ireq.extras, constraint=ireq.constraint
            )
        else:
            return self.repository.find_best_match(ireq, prereleases)

    def get_dependencies(self, ireq):
        return self.repository.get_dependencies(ireq)

    def get_hashes(self, ireq):
        existing_pin = self._reuse_hashes and self.existing_pins.get(
            key_from_ireq(ireq)
        )
        if existing_pin and ireq_satisfied_by_existing_pin(ireq, existing_pin):
            hashes = existing_pin.hash_options
            hexdigests = hashes.get(FAVORITE_HASH)
            if hexdigests:
                return {
                    ":".join([FAVORITE_HASH, hexdigest]) for hexdigest in hexdigests
                }
        return self.repository.get_hashes(ireq)

    @contextmanager
    def allow_all_wheels(self):
        with self.repository.allow_all_wheels():
            yield

    def copy_ireq_dependencies(self, source, dest):
        self.repository.copy_ireq_dependencies(source, dest)
