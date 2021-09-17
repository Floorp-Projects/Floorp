import contextlib


def set_context(session, context):
    session.send_session_command("POST", "moz/context", {"context": context})


@contextlib.contextmanager
def using_context(session, context):
    orig_context = session.send_session_command("GET", "moz/context")
    needs_change = context != orig_context

    if needs_change:
        set_context(session, context)

    try:
        yield
    finally:
        if needs_change:
            set_context(session, orig_context)
