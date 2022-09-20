import pytest
from typing import Any, List, Mapping

from webdriver.bidi.modules.script import ContextTarget


@pytest.fixture
def call_function(bidi_session, top_context):
    async def call_function(
        function_declaration: str,
        arguments: List[Mapping[str, Any]],
        context: str = top_context["context"],
        sandbox: str = None,
    ) -> Mapping[str, Any]:
        if sandbox is None:
            target = ContextTarget(top_context["context"])
        else:
            target = ContextTarget(top_context["context"], sandbox)

        result = await bidi_session.script.call_function(
            function_declaration=function_declaration,
            arguments=arguments,
            await_promise=False,
            target=target,
        )
        return result

    return call_function


@pytest.fixture
async def default_realm(bidi_session, top_context):
    realms = await bidi_session.script.get_realms(context=top_context["context"])
    return realms[0]["realm"]
