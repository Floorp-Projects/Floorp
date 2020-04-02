#!/usr/bin/env python
import mozunit

from mozperftest.system import pick_system
from mozperftest.tests.support import get_running_env


def test_proxy():
    mach_cmd, metadata = get_running_env()

    # XXX this will run for real, we need to mock HTTP calls
    with pick_system("script", mach_cmd) as proxy:
        proxy(metadata)


if __name__ == "__main__":
    mozunit.main()
