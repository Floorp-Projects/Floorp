# coding: utf-8
"""
:class:`asynctest.TestCase` decorator which controls checks performed after
tests.

This module is separated from :mod:`asynctest.case` to avoid circular imports
in modules registering new checks.

To implement new checks:

    * its name must be added in the ``DEFAULTS`` dict,

    * a static method of the same name must be added to the :class:`_fail_on`
      class,

    * an optional static method named ``before_[name of the check]`` can be
      added to :class:`_fail_on` to implement some set-up before the test runs.

A check may be only available on some platforms, activated by a conditional
import. In this case, ``DEFAULT`` and :class:`_fail_on` can be updated in the
module. There is an example in the :mod:`asynctest.selector` module.
"""
from asyncio import TimerHandle


_FAIL_ON_ATTR = "_asynctest_fail_on"


#: Default value of the arguments of @fail_on, the name of the argument matches
#: the name of the static method performing the check in the :class:`_fail_on`.
#: The value is True when the check is enabled by default, False otherwise.
DEFAULTS = {
    "unused_loop": False,
    "active_handles": False,
}


class _fail_on:
    def __init__(self, checks=None):
        self.checks = checks or {}
        self._computed_checks = None

    def __call__(self, func):
        checker = getattr(func, _FAIL_ON_ATTR, None)
        if checker:
            checker = checker.copy()
            checker.update(self.checks)
        else:
            checker = self.copy()

        setattr(func, _FAIL_ON_ATTR, checker)
        return func

    def update(self, checks, override=True):
        if override:
            self.checks.update(checks)
        else:
            for check, value in checks.items():
                self.checks.setdefault(check, value)

    def copy(self):
        return _fail_on(self.checks.copy())

    def get_checks(self, case):
        # cache the result so it's consistent across calls to get_checks()
        if self._computed_checks is None:
            checks = DEFAULTS.copy()

            try:
                checks.update(getattr(case, _FAIL_ON_ATTR, None).checks)
            except AttributeError:
                pass

            checks.update(self.checks)
            self._computed_checks = checks

        return self._computed_checks

    def before_test(self, case):
        checks = self.get_checks(case)
        for check in filter(checks.get, checks):
            try:
                getattr(self, "before_test_" + check)(case)
            except (AttributeError, TypeError):
                pass

    def check_test(self, case):
        checks = self.get_checks(case)
        for check in filter(checks.get, checks):
            getattr(self, check)(case)

    # checks

    @staticmethod
    def unused_loop(case):
        if not case.loop._asynctest_ran:
            case.fail("Loop did not run during the test")

    @staticmethod
    def _is_live_timer_handle(handle):
        return isinstance(handle, TimerHandle) and not handle._cancelled

    @classmethod
    def _live_timer_handles(cls, loop):
        return filter(cls._is_live_timer_handle, loop._scheduled)

    @classmethod
    def active_handles(cls, case):
        handles = tuple(cls._live_timer_handles(case.loop))
        if handles:
            case.fail("Loop contained unfinished work {!r}".format(handles))


def fail_on(**kwargs):
    """
    Enable checks on the loop state after a test ran to help testers to
    identify common mistakes.
    """
    # documented in asynctest.case.rst
    for kwarg in kwargs:
        if kwarg not in DEFAULTS:
            raise TypeError("fail_on() got an unexpected keyword argument "
                            "'{}'".format(kwarg))

    return _fail_on(kwargs)


def _fail_on_all(flag, func):
    checker = _fail_on(dict((arg, flag) for arg in DEFAULTS))
    return checker if func is None else checker(func)


def strict(func=None):
    """
    Activate strict checking of the state of the loop after a test ran.
    """
    # documented in asynctest.case.rst
    return _fail_on_all(True, func)


def lenient(func=None):
    """
    Deactivate all checks after a test ran.
    """
    # documented in asynctest.case.rst
    return _fail_on_all(False, func)
