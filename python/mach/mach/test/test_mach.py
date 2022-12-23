# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozunit import main


def test_set_isatty_environ(monkeypatch, get_mach):
    # Make sure the 'MACH_STDOUT_ISATTY' variable gets set.
    monkeypatch.delenv("MACH_STDOUT_ISATTY", raising=False)
    monkeypatch.setattr(os, "isatty", lambda fd: True)

    m = get_mach()
    orig_run = m._run
    env_is_set = []

    def wrap_run(*args, **kwargs):
        env_is_set.append("MACH_STDOUT_ISATTY" in os.environ)
        return orig_run(*args, **kwargs)

    monkeypatch.setattr(m, "_run", wrap_run)

    ret = m.run([])
    assert ret == 0
    assert env_is_set[0]


if __name__ == "__main__":
    main()
