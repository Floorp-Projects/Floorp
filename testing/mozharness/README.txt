# Mozharness

## Docs
* https://developer.mozilla.org/en-US/docs/Mozharness_FAQ
* https://wiki.mozilla.org/ReleaseEngineering/Mozharness
* http://moz-releng-mozharness.readthedocs.org/en/latest/mozharness.mozilla.html
* http://moz-releng-docs.readthedocs.org/en/latest/software.html#mozharness

## Submitting changes
Like any Gecko change, please create a patch or submit to Mozreview and
open a Bugzilla ticket under the Mozharness component:
https://bugzilla.mozilla.org/enter_bug.cgi?product=Release%20Engineering&component=Mozharness

This bug will get triaged by Release Engineering

## Run unit tests
To run the unit tests of mozharness the `tox` package needs to be installed:

```
pip install tox
```

There are various ways to run the unit tests. Just make sure you are within the `$gecko_repo/testing/mozharness` directory before running one of the commands below:

```
tox                            # run all unit tests
tox -- -x                      # run all unit tests but stop after first failure
tox -- test/test_base_log.py   # only run the base log unit test
```

Happy contributing! =)

