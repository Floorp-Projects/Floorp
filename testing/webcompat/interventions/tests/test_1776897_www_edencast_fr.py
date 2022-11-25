import pytest
from helpers import Css, assert_not_element, await_dom_ready, find_element

# The page will asyncronously write out an <audio> element on success, but
# will not do anything easily detectable otherwise.
#
# The problem is that the podPressShowHidePlayer function thinks to write
# out a Flash embed unless WebKit is in the UA string, but ends up doing
# nothing at all. However, it calls a different function for html5 vs SWF,
# and we can detect which one it calls to confirm it calls the html5 one.

URL = "https://www.edencast.fr/zoomcastlost-in-blindness/"

AUDIO_CSS = Css("audio#podpresshtml5_1")

SCRIPT = """
        var done = arguments[0];

        if (!window?.podPressShowHidePlayer) {
          done("none");
        }

        window.podPressenprintHTML5audio = function() {
          done("html5");
        };

        window.podpress_audioplayer_swfobject = {
          embedSWF: function() {
            done("swf");
          },
        };

        const d = document.createElement("div");
        d.id = "podPressPlayerSpace_test";
        document.documentElement.appendChild(d);

        const p = document.createElement("div");
        p.id = "podPressPlayerSpace_test_PlayLink";
        document.documentElement.appendChild(p);

        podPressShowHidePlayer("test", "https//x/test.mp3", 100, 100, "force");
    """


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    await_dom_ready(session)
    assert "html5" == session.execute_async_script(SCRIPT)
    assert find_element(session, AUDIO_CSS)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    await_dom_ready(session)
    assert "swf" == session.execute_async_script(SCRIPT)
    assert_not_element(session, AUDIO_CSS)
