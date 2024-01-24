from typing import Any, Mapping

from ._module import BidiModule, command


class Browser(BidiModule):
    @command
    def close(self) -> Mapping[str, Any]:
        return {}

    @command
    def create_user_context(self) -> Mapping[str, Any]:
        return {}

    @create_user_context.result
    def _create_user_context(self, result: Mapping[str, Any]) -> Any:
        assert result["userContext"] is not None
        assert isinstance(result["userContext"], str)

        return result["userContext"]
