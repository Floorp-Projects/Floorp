from __future__ import absolute_import
import pytest

import mozunit
from mozleak import lsan


@pytest.mark.parametrize(("input_", "expected"),
                         [("alloc_system::platform::_$LT$impl$u20$core..alloc.."
                           "GlobalAlloc$u20$for$u20$alloc_system..System$GT$::"
                           "alloc::h5a1f0db41e296502",
                           "alloc_system::platform::_$LT$impl$u20$core..alloc.."
                           "GlobalAlloc$u20$for$u20$alloc_system..System$GT$::alloc"),
                          ("alloc_system::platform::_$LT$impl$u20$core..alloc.."
                           "GlobalAlloc$u20$for$u20$alloc_system..System$GT$::alloc",
                           "alloc_system::platform::_$LT$impl$u20$core..alloc.."
                           "GlobalAlloc$u20$for$u20$alloc_system..System$GT$::alloc")])
def test_clean(input_, expected):
    leaks = lsan.LSANLeaks(None)
    assert leaks._cleanFrame(input_) == expected


if __name__ == '__main__':
    mozunit.main()
