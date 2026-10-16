# SPDX-License-Identifier: MPL-2.0
"""Bounded decoders and opaque projections; no transport, environment or execution."""

import hashlib
import json
import re
from typing import cast

from .agent_types import (
    Action, ContractError, Observation, RedirectEvent, ReplayContext, SourceBindings,
)
from tools.final_web_panel_http import RedirectCase

POLICY_ID = "web-panel-ua-on-off-on-v1"
ORACLE_ID = "final-web-panel-http-exact-three-hop-v1"
MAX_STEPS = 32
MAX_RECORD_BYTES = 32768  # Includes the mandatory final LF.
MAX_WIRE_BYTES = 524288  # Each of the two separate channels.
MAX_PAGE_BYTES = 65536
MAX_OUTPUT_BYTES = 32768
ACTION_IDS = frozenset((
    "observe", "web-panel.mobile-on", "web-panel.mobile-off",
    "web-panel.navigate-two-hop",
))
REQUIRED_ACTIONS = (
    "web-panel.mobile-on", "web-panel.navigate-two-hop",
    "web-panel.mobile-off", "web-panel.navigate-two-hop",
    "web-panel.mobile-on", "web-panel.navigate-two-hop",
)


def require(condition: bool, code: str) -> None:
    if not condition:
        raise ContractError(code)


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True, allow_nan=False).encode("ascii")


def _string(value: object, limit: int = 8192) -> str:
    require(type(value) is str, "STRING_TYPE")
    text = cast(str, value)
    try:
        require(len(text.encode("utf-8")) <= limit, "STRING_BOUND")
    except UnicodeError:
        raise ContractError("STRING_UTF8") from None
    return text


def _int(value: object) -> int:
    require(type(value) is int, "INTEGER_TYPE")
    number = cast(int, value)
    require(abs(number) <= 1000000000, "INTEGER_BOUND")
    return number


def _object(value: object, fields: set[str]) -> dict[str, object]:
    require(type(value) is dict, "OBJECT_TYPE")
    obj = cast(dict[str, object], value)
    require(set(obj) == fields, "OBJECT_FIELDS")
    return obj


def _pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        require(key not in result, "DUPLICATE_KEY")
        result[key] = value
    return result


def _number(text: str) -> int:
    require(len(text.lstrip("-")) <= 10, "INTEGER_BOUND")
    return _int(int(text))


def _no_float(_text: str) -> object:
    raise ContractError("NUMBER_FORMAT")


def _bounded_value(value: object, depth: int = 0) -> None:
    require(depth <= 10, "JSON_DEPTH")
    if type(value) is str:
        _string(value)
    elif type(value) is list:
        items = cast(list[object], value)
        require(len(items) <= MAX_STEPS, "ARRAY_BOUND")
        for item in items:
            _bounded_value(item, depth + 1)
    elif type(value) is dict:
        obj = cast(dict[str, object], value)
        require(len(obj) <= 16, "OBJECT_BOUND")
        for key, item in obj.items():
            _string(key, 80)
            _bounded_value(item, depth + 1)


def decode_record(raw: bytes) -> object:
    require(type(raw) is bytes, "WIRE_TYPE")
    require(0 < len(raw) <= MAX_RECORD_BYTES, "RECORD_BOUND")
    require(raw.endswith(b"\n") and b"\n" not in raw[:-1], "RECORD_FRAMING")
    try:
        value = json.loads(raw[:-1].decode("utf-8"), object_pairs_hook=_pairs,
                           parse_int=_number, parse_float=_no_float,
                           parse_constant=_no_float)
        _bounded_value(value)
        return value
    except ContractError:
        raise
    except (ValueError, UnicodeError, RecursionError):
        raise ContractError("JSON_INVALID") from None


def wire_lines(raw: bytes) -> list[bytes]:
    require(type(raw) is bytes, "WIRE_TYPE")
    require(0 < len(raw) <= MAX_WIRE_BYTES, "WIRE_BOUND")
    # Split only on LF, preserving a partial final record for sequential rejection.
    pieces = raw.split(b"\n", MAX_STEPS)
    return [piece + b"\n" for piece in pieces[:-1]] + (
        [pieces[-1]] if pieces[-1] else [])


def _envelope(obj: dict[str, object], kind: str) -> tuple[str, int, str]:
    require(_int(obj["schemaVersion"]) == 1 and obj["kind"] == kind, "ENVELOPE")
    run_id = _string(obj["runId"], 64)
    require(re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", run_id) is not None, "RUN_ID")
    step = _int(obj["step"])
    require(1 <= step <= MAX_STEPS, "STEP_BOUND")
    action_id = _string(obj["actionId"], 80)
    require(action_id in ACTION_IDS, "ACTION_ID")
    return run_id, step, action_id


ENVELOPE_FIELDS = {"schemaVersion", "kind", "runId", "step", "actionId"}


def decode_action(raw: bytes) -> Action:
    obj = _object(decode_record(raw), ENVELOPE_FIELDS)
    return Action(*_envelope(obj, "action"))


def _ua_list(value: object) -> tuple[str, ...]:
    require(type(value) is list, "UA_LIST_TYPE")
    items = cast(list[object], value)
    require(len(items) <= 1, "UA_LIST_BOUND")
    return tuple(_string(item, 4096) for item in items)


def _event(value: object) -> RedirectEvent:
    obj = _object(value, {"nonce", "caseId", "hop", "url", "host", "method",
                          "status", "location", "userAgent"})
    location = obj["location"]
    require(location is None or type(location) is str, "LOCATION_TYPE")
    return RedirectEvent(
        _string(obj["nonce"], 32), _string(obj["caseId"], 80), _int(obj["hop"]),
        _string(obj["url"], 512), _string(obj["host"], 80), _string(obj["method"], 8),
        _int(obj["status"]), None if location is None else _string(location, 512),
        _string(obj["userAgent"], 4096),
    )


def decode_observation(raw: bytes) -> Observation:
    obj = _object(decode_record(raw), ENVELOPE_FIELDS | {
        "mobileEnabled", "redirect", "normalTabUas", "otherPanelUas"})
    envelope = _envelope(obj, "observation")
    require(type(obj["mobileEnabled"]) is bool, "MOBILE_TYPE")
    require(type(obj["redirect"]) is list, "REDIRECT_TYPE")
    events = cast(list[object], obj["redirect"])
    require(len(events) <= 3, "REDIRECT_BOUND")
    return Observation(*envelope, cast(bool, obj["mobileEnabled"]),
                       tuple(_event(item) for item in events),
                       _ua_list(obj["normalTabUas"]), _ua_list(obj["otherPanelUas"]))


def validate_context(context: ReplayContext) -> None:
    require(type(context) is ReplayContext and type(context.sources) is SourceBindings,
            "CONTEXT_TYPE")
    run = _string(context.run_id, 64)
    require(re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", run) is not None, "RUN_ID")
    for source in (context.sources.floorp, context.sources.runtime):
        require(re.fullmatch(r"[0-9a-f]{40}", _string(source, 40)) is not None,
                "SOURCE_BINDING")
    for ua in (context.desktop_ua, context.mobile_ua):
        require(0 < len(_string(ua, 4096)) and not any(ord(c) < 32 or ord(c) == 127
                                                      for c in ua), "UA_CONTEXT")
    require(context.desktop_ua != context.mobile_ua, "UA_NOT_DISCRIMINATING")
    try:
        RedirectCase("0" * 32, "web-panel-ua.redirect.mobile-on",
                     _string(context.origin, 40), context.mobile_ua)
    except (RuntimeError, ValueError):
        raise ContractError("ORIGIN_CONTEXT") from None


def context_digest(context: ReplayContext) -> str:
    validate_context(context)
    return digest(canonical({
        "policyId": POLICY_ID, "oracleId": ORACLE_ID, "runId": context.run_id,
        "origin": context.origin, "desktopUa": context.desktop_ua,
        "mobileUa": context.mobile_ua, "floorpSource": context.sources.floorp,
        "runtimeSource": context.sources.runtime, "initialMobileEnabled": False,
    }))


def redirect_case(context: ReplayContext, step: int, mobile: bool) -> RedirectCase:
    validate_context(context)
    require(type(step) is int and 1 <= step <= MAX_STEPS and type(mobile) is bool,
            "CASE_CONTEXT")
    nonce = digest(canonical([context_digest(context), step]))[:32]
    return RedirectCase(nonce, "web-panel-ua.redirect.mobile-" + ("on" if mobile else "off"),
                        context.origin, context.mobile_ua if mobile else context.desktop_ua)


def project_untrusted_page(raw: bytes) -> dict[str, object]:
    """Opaque data only. This is not an authenticated controller-channel boundary."""
    require(type(raw) is bytes and len(raw) <= MAX_PAGE_BYTES, "PAGE_BOUND")
    return {"kind": "untrusted-page-summary", "byteCount": len(raw),
            "sha256": digest(raw), "nonActionable": True}
