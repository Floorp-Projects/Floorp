# This code is loaded via `mach python --exec-file`, so it runs in the scope of
# the `mach python` command.
from __future__ import absolute_import
old = self._mach_context.post_dispatch_handler  # noqa: F821


def handler(context, handler, instance, result,
            start_time, end_time, depth, args):
    global old
    # Round off sub-second precision.
    old(context, handler, instance, result,
        int(start_time), end_time, depth, args)


self._mach_context.post_dispatch_handler = handler  # noqa: F821
