import pytest
from tests.bidi import get_viewport_dimensions
from tests.bidi.browsing_context.capture_screenshot import (
    get_element_coordinates,
    get_physical_element_dimensions,
    get_reference_screenshot,
)
from tests.support.image import png_dimensions
from webdriver.bidi.modules.browsing_context import BoxOptions, ElementOptions
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio


async def test_params_clip_element_with_document_origin(
    bidi_session, top_context, inline, compare_png_bidi
):
    element_styles = "background-color: black; width: 50px; height:50px;"

    # Render an element inside of viewport for the reference.
    reference_data = await get_reference_screenshot(
        bidi_session,
        inline,
        top_context["context"],
        f"""<div style="{element_styles}"></div>""",
    )

    viewport_dimensions = await get_viewport_dimensions(bidi_session, top_context)

    # Render the same element outside of viewport.
    url = inline(
        f"""<div style="{element_styles} margin-top: {viewport_dimensions["height"]}px"></div>"""
    )
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=url, wait="complete"
    )
    element = await bidi_session.script.evaluate(
        await_promise=False,
        expression="document.querySelector('div')",
        target=ContextTarget(top_context["context"]),
    )
    expected_size = await get_physical_element_dimensions(
        bidi_session, top_context, element
    )

    data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"],
        clip=ElementOptions(element=element),
        origin="document",
    )

    assert png_dimensions(data) == expected_size

    comparison = await compare_png_bidi(reference_data, data)
    assert comparison.equal()


async def test_params_clip_box_with_document_origin(
    bidi_session, top_context, inline, compare_png_bidi
):
    element_styles = "background-color: black; width: 50px; height:50px;"

    # Render an element inside of viewport for the reference.
    reference_data = await get_reference_screenshot(
        bidi_session,
        inline,
        top_context["context"],
        f"""<div style="{element_styles}"></div>""",
    )

    viewport_dimensions = await get_viewport_dimensions(bidi_session, top_context)

    # Render the same element outside of viewport.
    url = inline(
        f"""<div style="{element_styles} margin-top: {viewport_dimensions["height"]}px"></div>"""
    )
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=url, wait="complete"
    )
    element = await bidi_session.script.call_function(
        await_promise=False,
        function_declaration="""() => document.querySelector('div')""",
        target=ContextTarget(top_context["context"]),
    )
    element_coordinates = await get_element_coordinates(
        bidi_session, top_context, element
    )
    element_dimensions = await get_physical_element_dimensions(
        bidi_session, top_context, element
    )
    data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"],
        clip=BoxOptions(
            x=element_coordinates[0],
            y=element_coordinates[1],
            width=element_dimensions[0],
            height=element_dimensions[1],
        ),
        origin="document",
    )
    assert png_dimensions(data) == element_dimensions

    comparison = await compare_png_bidi(reference_data, data)
    assert comparison.equal()
