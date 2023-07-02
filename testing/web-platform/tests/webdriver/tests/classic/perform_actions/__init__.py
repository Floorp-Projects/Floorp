def perform_actions(session, actions):
    return session.transport.send(
        "POST",
        "/session/{session_id}/actions".format(session_id=session.session_id),
        {"actions": actions})
