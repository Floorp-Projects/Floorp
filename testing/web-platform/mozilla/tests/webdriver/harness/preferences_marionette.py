from support.fixtures import get_pref


def test_recommended_preferences(session):
    has_recommended_prefs = get_pref(session, "remote.prefs.recommended.applied")
    assert has_recommended_prefs is True
