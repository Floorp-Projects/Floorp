# Mozharness

### Submitting changes
Like any Gecko change, please create a patch or submit to Mozreview and
open a Bugzilla ticket under the Mozharness component:
https://bugzilla.mozilla.org/enter_bug.cgi?product=Release%20Engineering&component=Mozharness

This bug will get triaged by Release Engineering

### Docs
* https://developer.mozilla.org/en-US/docs/Mozharness_FAQ
* https://wiki.mozilla.org/ReleaseEngineering/Mozharness
* http://moz-releng-mozharness.readthedocs.org/en/latest/mozharness.mozilla.html
* http://moz-releng-docs.readthedocs.org/en/latest/software.html#mozharness

### To run mozharness unit tests
```
pip install tox
tox  # from within the $gecko_repo/testing/mozharness directory
```

Happy contributing! =)

