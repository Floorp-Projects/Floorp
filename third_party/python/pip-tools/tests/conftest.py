import json
from contextlib import contextmanager
from functools import partial

from pip._vendor.packaging.version import Version
from pip._vendor.pkg_resources import Requirement
from piptools._compat import InstallRequirement
from pytest import fixture

from piptools.cache import DependencyCache
from piptools.repositories.base import BaseRepository
from piptools.resolver import Resolver
from piptools.utils import as_tuple, key_from_req, make_install_requirement
from piptools.exceptions import NoCandidateFound


class FakeRepository(BaseRepository):
    def __init__(self):
        with open('tests/test_data/fake-index.json', 'r') as f:
            self.index = json.load(f)

        with open('tests/test_data/fake-editables.json', 'r') as f:
            self.editables = json.load(f)

    def get_hashes(self, ireq):
        # Some fake hashes
        return {
            'test:123',
            'sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',
        }

    def find_best_match(self, ireq, prereleases=False):
        if ireq.editable:
            return ireq

        versions = list(ireq.specifier.filter(self.index[key_from_req(ireq.req)],
                                              prereleases=prereleases))
        if not versions:
            raise NoCandidateFound(ireq, self.index[key_from_req(ireq.req)], ['https://fake.url.foo'])
        best_version = max(versions, key=Version)
        return make_install_requirement(key_from_req(ireq.req), best_version, ireq.extras, constraint=ireq.constraint)

    def get_dependencies(self, ireq):
        if ireq.editable:
            return self.editables[str(ireq.link)]

        name, version, extras = as_tuple(ireq)
        # Store non-extra dependencies under the empty string
        extras += ("",)
        dependencies = [dep for extra in extras for dep in self.index[name][version][extra]]
        return [InstallRequirement.from_line(dep, constraint=ireq.constraint) for dep in dependencies]

    @contextmanager
    def allow_all_wheels(self):
        # No need to do an actual pip.Wheel mock here.
        yield


class FakeInstalledDistribution(object):
    def __init__(self, line, deps=None):
        if deps is None:
            deps = []
        self.deps = [Requirement.parse(d) for d in deps]

        self.req = Requirement.parse(line)

        self.key = key_from_req(self.req)
        self.specifier = self.req.specifier

        self.version = line.split("==")[1]

    def requires(self):
        return self.deps

    def as_requirement(self):
        return self.req


@fixture
def fake_dist():
    return FakeInstalledDistribution


@fixture
def repository():
    return FakeRepository()


@fixture
def depcache(tmpdir):
    return DependencyCache(str(tmpdir))


@fixture
def resolver(depcache, repository):
    # TODO: It'd be nicer if Resolver instance could be set up and then
    #       use .resolve(...) on the specset, instead of passing it to
    #       the constructor like this (it's not reusable)
    return partial(Resolver, repository=repository, cache=depcache)


@fixture
def base_resolver(depcache):
    return partial(Resolver, cache=depcache)


@fixture
def from_line():
    return InstallRequirement.from_line


@fixture
def from_editable():
    return InstallRequirement.from_editable
