def on_exception(env, layer, exc):
    # swallow the error and resume
    return True
