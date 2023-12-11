# Make flake8, ruff and autodoc happy
if "self" not in globals():
    self = None

# This code is loaded via `mach python --exec-file`, so it runs in the scope of
# the `mach python` command.
old = self._mach_context.post_dispatch_handler


def handler(context, handler, instance, result, start_time, end_time, depth, args):
    global old
    # Round off sub-second precision.
    old(context, handler, instance, result, int(start_time), end_time, depth, args)


self._mach_context.post_dispatch_handler = handler
