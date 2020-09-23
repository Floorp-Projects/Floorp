_GLOBAL = False


def before_iterations(kw):
    global _GLOBAL
    _GLOBAL = True


def before_runs(env, **kw):
    if not _GLOBAL:
        raise Exception("oops")
