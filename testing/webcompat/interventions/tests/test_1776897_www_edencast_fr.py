import pytest

# The page will asyncronously write out an <audio> element on success, but
# will not do anything easily detectable otherwise.
#
# The problem is that the podPressShowHidePlayer function thinks to write
# out a Flash embed unless WebKit is in the UA string, but ends up doing
# nothing at all. However, it calls a different function for html5 vs SWF,
# and we can detect which one it calls to confirm it calls the html5 one.

URL = "https://www.edencast.fr/zoomcastlost-in-blindness/"

AUDIO_CSS = "audio#podpresshtml5_1"

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


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    await client.dom_ready()
    assert "html5" == client.execute_async_script(SCRIPT)
    assert client.find_css(AUDIO_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    await client.dom_ready()
    assert "swf" == client.execute_async_script(SCRIPT)
    assert not client.find_css(AUDIO_CSS)
