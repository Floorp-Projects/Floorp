# General

* Contributing to distro identification currently doesn't have any specific standards and rather depends on the specific implementation.
* A 100% coverage is expected for each PR unless explicitly authorized by the reviewer.
* Please try to maintain maximum code-health (via landscape.io).

# Contributing distro specific tests

Distro's tests are implemented via a standardized framework under `tests/test_distro.py`

For each distribution, tests should be added in the relevant class according to which distribution file(s) exists on it, so, for example, tests should be added under `TestOSRelease` where `/etc/os-release` is available.

The tests must be self-contained, meaning that the release files for the distribution should be maintained in the repository under `tests/resources/distros/distribution_name+distribution_version`.

A tests method would like somewhat like this:

```python
def test_centos7_os_release(self):
    desired_outcome = {
        'id': 'centos',
        'name': 'CentOS Linux',
        'pretty_name': 'CentOS Linux 7 (Core)',
        'version': '7',
        'pretty_version': '7 (Core)',
        'best_version': '7',
        'like': 'rhel fedora',
        'codename': 'Core'
    }
    self._test_outcome(desired_outcome)
```

The framework will automatically try to pick up the relevant file according to the method's name (`centos7` meaning the folder should be named `centos7` as well) and compare the `desired_outcome` with the parsed files found under the test dir.

The exception to the rule is under the `TestDistroRelease` test class which should look somewhat like this:

```python
def test_centos5_dist_release(self):
    desired_outcome = {
        'id': 'centos',
        'name': 'CentOS',
        'pretty_name': 'CentOS 5.11 (Final)',
        'version': '5.11',
        'pretty_version': '5.11 (Final)',
        'best_version': '5.11',
        'codename': 'Final',
        'major_version': '5',
        'minor_version': '11'
    }
    self._test_outcome(desired_outcome, 'centos', '5')
```

Where the name of the method is not indicative of the lookup folder but rather tha two last arguments in `_test_outcome`.

A test case is mandatory under `TestOverall` for a PR to be complete.