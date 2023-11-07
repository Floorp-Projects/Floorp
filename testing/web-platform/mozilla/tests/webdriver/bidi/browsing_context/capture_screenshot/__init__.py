from math import floor

from tests.bidi import get_device_pixel_ratio, remote_mapping_to_dict
from webdriver.bidi.modules.script import ContextTarget


async def get_document_dimensions(bidi_session, context: str):
    expression = """
        ({
          height: document.documentElement.scrollHeight,
          width: document.documentElement.scrollWidth,
        });
    """
    result = await bidi_session.script.evaluate(
        expression=expression,
        target=ContextTarget(context["context"]),
        await_promise=False,
    )

    return remote_mapping_to_dict(result["value"])


async def get_physical_document_dimensions(bidi_session, context):
    """Get the physical dimensions of the context's document.
    :param bidi_session: BiDiSession
    :param context: Browsing context ID
    :returns: Tuple of (int, int) containing document width, document height.
    """
    document = await get_document_dimensions(bidi_session, context)
    dpr = await get_device_pixel_ratio(bidi_session, context)
    return (floor(document["width"] * dpr), floor(document["height"] * dpr))
