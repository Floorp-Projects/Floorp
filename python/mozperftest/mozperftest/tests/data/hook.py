def doit(env):
    return "OK"


def on_exception(env, layer, exc):
    # swallow the error and abort the run
    return False
