from webdriver.bidi.modules.script import ContextTarget

from ... import get_viewport_dimensions, remote_mapping_to_dict
from .. import get_object_from_context

async def assert_pointer_events(
    bidi_session, context, expected_events, target, pointer_type
):
    events = await get_object_from_context(
        bidi_session, context["context"], "window.recordedEvents"
    )

    assert len(events) == len(expected_events)
    event_types = [e["type"] for e in events]
    assert expected_events == event_types

    for e in events:
        assert e["target"] == target
        assert e["pointerType"] == pointer_type


async def get_inview_center_bidi(bidi_session, context, element):
    elem_rect = await get_element_rect(bidi_session, context=context, element=element)
    viewport_rect = await get_viewport_dimensions(bidi_session, context=context)

    x = {
        "left": max(0, min(elem_rect["x"], elem_rect["x"] + elem_rect["width"])),
        "right": min(
            viewport_rect["width"],
            max(elem_rect["x"], elem_rect["x"] + elem_rect["width"]),
        ),
    }

    y = {
        "top": max(0, min(elem_rect["y"], elem_rect["y"] + elem_rect["height"])),
        "bottom": min(
            viewport_rect["height"],
            max(elem_rect["y"], elem_rect["y"] + elem_rect["height"]),
        ),
    }

    return {
        "x": (x["left"] + x["right"]) / 2,
        "y": (y["top"] + y["bottom"]) / 2,
    }


async def get_element_rect(bidi_session, context, element):
    result = await bidi_session.script.call_function(
        function_declaration="""
el => el.getBoundingClientRect().toJSON()
""",
        arguments=[element],
        target=ContextTarget(context["context"]),
        await_promise=False,
    )

    return remote_mapping_to_dict(result["value"])


async def record_pointer_events(bidi_session, context, container, selector):
    # Record basic mouse / pointer events on the element matching the given
    # selector in the container.
    # The serialized element will be returned
    target = await bidi_session.script.call_function(
        function_declaration=f"""container => {{
            const target = container.querySelector("{selector}");
            window.recordedEvents = [];
            function onPointerEvent(event) {{
                window.recordedEvents.push({{
                    "type": event.type,
                    "pointerType": event.pointerType,
                    "target": event.target.id
                }});
            }}
            target.addEventListener("pointerdown", onPointerEvent);
            target.addEventListener("pointerup", onPointerEvent);
            return target;
        }}
        """,
        arguments=[container],
        target=ContextTarget(context["context"]),
        await_promise=False,
    )

    return target