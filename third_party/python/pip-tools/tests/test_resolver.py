import pytest

from piptools.exceptions import NoCandidateFound
from piptools.resolver import combine_install_requirements


@pytest.mark.parametrize(
    ("input", "expected", "prereleases", "unsafe_constraints"),
    (
        (tup + (False, set())[len(tup) - 2 :])  # noqa: E203
        for tup in [
            (["Django"], ["django==1.8"]),
            (
                ["Flask"],
                [
                    "flask==0.10.1",
                    "itsdangerous==0.24 (from flask==0.10.1)",
                    "markupsafe==0.23 (from jinja2==2.7.3->flask==0.10.1)",
                    "jinja2==2.7.3 (from flask==0.10.1)",
                    "werkzeug==0.10.4 (from flask==0.10.1)",
                ],
            ),
            (["Jinja2", "markupsafe"], ["jinja2==2.7.3", "markupsafe==0.23"]),
            # We should return a normal release version if prereleases is False
            (["SQLAlchemy"], ["sqlalchemy==0.9.9"]),
            # We should return the prerelease version if prereleases is True
            (["SQLAlchemy"], ["sqlalchemy==1.0.0b5"], True),
            # Ipython has extras available, but we don't require them in this test
            (
                ["ipython"],
                ["ipython==2.1.0", "gnureadline==6.3.3 (from ipython==2.1.0)"],
            ),
            # We should get dependencies for extras
            (
                ["ipython[notebook]"],
                [
                    "ipython[notebook]==2.1.0",
                    "pyzmq==2.1.12 (from ipython[notebook]==2.1.0)",
                    "jinja2==2.7.3 (from ipython[notebook]==2.1.0)",
                    "tornado==3.2.2 (from ipython[notebook]==2.1.0)",
                    "markupsafe==0.23 (from jinja2==2.7.3->ipython[notebook]==2.1.0)",
                    "gnureadline==6.3.3 (from ipython[notebook]==2.1.0)",
                ],
            ),
            # We should get dependencies for multiple extras
            (
                ["ipython[notebook,nbconvert]"],
                [
                    # Note that the extras should be sorted
                    "ipython[nbconvert,notebook]==2.1.0",
                    "pyzmq==2.1.12 (from ipython[nbconvert,notebook]==2.1.0)",
                    "jinja2==2.7.3 (from ipython[nbconvert,notebook]==2.1.0)",
                    "tornado==3.2.2 (from ipython[nbconvert,notebook]==2.1.0)",
                    (
                        "markupsafe==0.23 "
                        "(from jinja2==2.7.3->ipython[nbconvert,notebook]==2.1.0)"
                    ),
                    "gnureadline==6.3.3 (from ipython[nbconvert,notebook]==2.1.0)",
                    "pygments==1.5 (from ipython[nbconvert,notebook]==2.1.0)",
                    "sphinx==0.3 (from ipython[nbconvert,notebook]==2.1.0)",
                ],
            ),
            # We must take the union of all extras
            (
                ["ipython[notebook]", "ipython[nbconvert]"],
                [
                    # Note that the extras should be sorted
                    "ipython[nbconvert,notebook]==2.1.0",
                    "pyzmq==2.1.12 (from ipython[nbconvert,notebook]==2.1.0)",
                    "jinja2==2.7.3 (from ipython[nbconvert,notebook]==2.1.0)",
                    "tornado==3.2.2 (from ipython[nbconvert,notebook]==2.1.0)",
                    (
                        "markupsafe==0.23 "
                        "(from jinja2==2.7.3->ipython[nbconvert,notebook]==2.1.0)"
                    ),
                    "gnureadline==6.3.3 (from ipython[nbconvert,notebook]==2.1.0)",
                    "pygments==1.5 (from ipython[nbconvert,notebook]==2.1.0)",
                    "sphinx==0.3 (from ipython[nbconvert,notebook]==2.1.0)",
                ],
            ),
            # We must remove child dependencies from result if parent
            # is removed (e.g. vine from amqp>=2.0)
            # See: GH-370
            # because of updated dependencies in the test index, we need to pin celery
            # in order to reproduce vine removal (because it was readded in later
            # releases)
            (
                ["celery<=3.1.23", "librabbitmq"],
                [
                    "amqp==1.4.9 (from librabbitmq==1.6.1)",
                    "anyjson==0.3.3 (from kombu==3.0.35->celery==3.1.23)",
                    "billiard==3.5.0.2 (from celery==3.1.23)",
                    "celery==3.1.23",
                    "kombu==3.0.35 (from celery==3.1.23)",
                    "librabbitmq==1.6.1",
                    "pytz==2016.4 (from celery==3.1.23)",
                ],
            ),
            # Support specifying loose top-level requirements that could also appear as
            # pinned subdependencies.
            (
                ["billiard", "celery", "fake-piptools-test-with-pinned-deps"],
                [
                    "amqp==1.4.9 (from kombu==3.0.35->celery==3.1.18)",
                    "anyjson==0.3.3 (from kombu==3.0.35->celery==3.1.18)",
                    "billiard==3.3.0.23",
                    "celery==3.1.18",  # this is pinned from test subdependency
                    "fake-piptools-test-with-pinned-deps==0.1",
                    "kombu==3.0.35 (from celery==3.1.18)",
                    "pytz==2016.4 (from celery==3.1.18)",
                ],
            ),
            # Exclude package dependcy of setuptools as it is unsafe.
            (
                ["html5lib"],
                ["html5lib==0.999999999"],
                False,
                {"setuptools==35.0.0 (from html5lib==0.999999999)"},
            ),
            # We shouldn't include irrelevant pip constraints
            # See: GH-471
            (
                ["Flask", ("click", True), ("itsdangerous", True)],
                [
                    "flask==0.10.1",
                    "itsdangerous==0.24",
                    "markupsafe==0.23 (from jinja2==2.7.3->flask==0.10.1)",
                    "jinja2==2.7.3 (from flask==0.10.1)",
                    "werkzeug==0.10.4 (from flask==0.10.1)",
                ],
            ),
            # Unsafe dependencies should be filtered
            (
                ["setuptools==35.0.0", "anyjson==0.3.3"],
                ["anyjson==0.3.3"],
                False,
                {"setuptools==35.0.0"},
            ),
            (
                ["fake-piptools-test-with-unsafe-deps==0.1"],
                ["fake-piptools-test-with-unsafe-deps==0.1"],
                False,
                {
                    (
                        "setuptools==34.0.0 (from "
                        "fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                    (
                        "appdirs==1.4.9 (from "
                        "setuptools==34.0.0->fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                    (
                        "packaging==16.8 (from "
                        "setuptools==34.0.0->fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                },
            ),
            # Git URL requirement
            # See: GH-851
            (
                [
                    "git+https://github.com/celery/billiard#egg=billiard==3.5.9999",
                    "celery==4.0.2",
                ],
                [
                    "amqp==2.1.4 (from kombu==4.0.2->celery==4.0.2)",
                    "kombu==4.0.2 (from celery==4.0.2)",
                    "billiard<3.6.0,==3.5.9999,>=3.5.0.2 from "
                    "git+https://github.com/celery/billiard#egg=billiard==3.5.9999",
                    "vine==1.1.3 (from amqp==2.1.4->kombu==4.0.2->celery==4.0.2)",
                    "celery==4.0.2",
                    "pytz==2016.4 (from celery==4.0.2)",
                ],
            ),
            # Check that dependencies of relevant constraints are resolved
            (
                ["aiohttp", ("yarl==1.4.2", True)],
                ["aiohttp==3.6.2", "idna==2.8 (from yarl==1.4.2)", "yarl==1.4.2"],
            ),
        ]
    ),
)
def test_resolver(
    resolver, from_line, input, expected, prereleases, unsafe_constraints
):
    input = [line if isinstance(line, tuple) else (line, False) for line in input]
    input = [from_line(req[0], constraint=req[1]) for req in input]
    resolver = resolver(input, prereleases=prereleases)
    output = resolver.resolve()
    output = {str(line) for line in output}
    assert output == {str(line) for line in expected}
    assert {str(line) for line in resolver.unsafe_constraints} == unsafe_constraints


@pytest.mark.parametrize(
    ("input", "expected", "prereleases"),
    (
        (tup + (False,))[:3]
        for tup in [
            (
                ["setuptools==34.0.0"],
                [
                    "appdirs==1.4.9 (from setuptools==34.0.0)",
                    "packaging==16.8 (from setuptools==34.0.0)",
                    "setuptools==34.0.0",
                ],
            ),
            (
                ["fake-piptools-test-with-unsafe-deps==0.1"],
                [
                    (
                        "appdirs==1.4.9 (from "
                        "setuptools==34.0.0->fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                    (
                        "setuptools==34.0.0 "
                        "(from fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                    (
                        "packaging==16.8 (from "
                        "setuptools==34.0.0->fake-piptools-test-with-unsafe-deps==0.1)"
                    ),
                    "fake-piptools-test-with-unsafe-deps==0.1",
                ],
            ),
        ]
    ),
)
def test_resolver__allows_unsafe_deps(
    resolver, from_line, input, expected, prereleases
):
    input = [line if isinstance(line, tuple) else (line, False) for line in input]
    input = [from_line(req[0], constraint=req[1]) for req in input]
    output = resolver(input, prereleases=prereleases, allow_unsafe=True).resolve()
    output = {str(line) for line in output}
    assert output == {str(line) for line in expected}


def test_resolver__max_number_rounds_reached(resolver, from_line):
    """
    Resolver should raise an exception if max round has been reached.
    """
    input = [from_line("django")]
    with pytest.raises(RuntimeError, match="after 0 rounds of resolving"):
        resolver(input).resolve(max_rounds=0)


def test_iter_dependencies(resolver, from_line):
    """
    Dependencies should be pinned or editable.
    """
    ireq = from_line("django>=1.8")
    res = resolver([])

    with pytest.raises(
        TypeError, match="Expected pinned or editable requirement, got django>=1.8"
    ):
        next(res._iter_dependencies(ireq))


def test_iter_dependencies_results(resolver, from_line):
    res = resolver([])
    ireq = from_line("aiohttp==3.6.2")
    assert next(res._iter_dependencies(ireq)).comes_from == ireq


def test_iter_dependencies_ignores_constraints(resolver, from_line):
    res = resolver([])
    ireq = from_line("aiohttp==3.6.2", constraint=True)
    with pytest.raises(StopIteration):
        next(res._iter_dependencies(ireq))


def test_combine_install_requirements(from_line):
    celery30 = from_line("celery>3.0", comes_from="-r requirements.in")
    celery31 = from_line("celery==3.1.1", comes_from=from_line("fake-package"))
    celery32 = from_line("celery<3.2")

    combined = combine_install_requirements([celery30, celery31])
    assert combined.comes_from == celery31.comes_from  # shortest string
    assert set(combined._source_ireqs) == {celery30, celery31}
    assert str(combined.req.specifier) == "==3.1.1,>3.0"

    combined_all = combine_install_requirements([celery32, combined])
    assert combined_all.comes_from is None
    assert set(combined_all._source_ireqs) == {celery30, celery31, celery32}
    assert str(combined_all.req.specifier) == "<3.2,==3.1.1,>3.0"


def test_compile_failure_shows_provenance(resolver, from_line):
    """
    Provenance of conflicting dependencies should be printed on failure.
    """
    requirements = [
        from_line("fake-piptools-test-with-pinned-deps==0.1"),
        from_line("celery>3.2"),
    ]

    with pytest.raises(NoCandidateFound) as err:
        resolver(requirements).resolve()
    lines = str(err.value).splitlines()
    assert lines[-2].strip() == "celery>3.2"
    assert (
        lines[-1].strip()
        == "celery==3.1.18 (from fake-piptools-test-with-pinned-deps==0.1)"
    )
