from tests.support.asserts import assert_success
from tests.support.inline import inline


def click(session, element):
    return session.transport.send(
        "POST", "/session/{session_id}/element/{element_id}/click".format(
            session_id=session.session_id,
            element_id=element.id))


def test_element_disappears_during_click(session):
    """
    When an element in the event bubbling order disappears (its CSS
    display style is set to "none") during a click, Gecko and Blink
    exhibit different behaviour.  Whilst Chrome fires a "click"
    DOM event on <body>, Firefox does not.

    A WebDriver implementation may choose to wait for this event to let
    the event loops spin enough times to let click events propagate,
    so this is a corner case test that Firefox does not hang indefinitely.
    """
    session.url = inline("""
        <style>
        #over,
        #under {
          position: absolute;
          top: 8px;
          left: 8px;
          width: 100px;
          height: 100px;
        }

        #over {
          background: blue;
          opacity: .5;
        }
        #under {
          background: yellow;
        }

        #log {
          margin-top: 120px;
        }
        </style>

        <body id="body">
          <div id=under></div>
          <div id=over></div>

           <div id=log></div>
        </body>

        <script>
        let under = document.querySelector("#under");
        let over = document.querySelector("#over");
        let body = document.querySelector("body");
        let log = document.querySelector("#log");

        function logEvent({type, target, currentTarget}) {
          log.innerHTML += "<p></p>";
          log.lastElementChild.textContent = `${type} in ${target.id} (handled by ${currentTarget.id})`;
        }

        for (let ev of ["click", "mousedown", "mouseup"]) {
          under.addEventListener(ev, logEvent);
          over.addEventListener(ev, logEvent);
          body.addEventListener(ev, logEvent);
        }

        over.addEventListener("mousedown", () => over.style.display = "none");
        </script>
        """)
    over = session.find.css("#over", all=False)

    # should not time out
    response = click(session, over)
    assert_success(response)
