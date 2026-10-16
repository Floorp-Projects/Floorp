# SPDX-License-Identifier: MPL-2.0
"""Immutable values for the closed, source-only Web Panel replay protocol."""

from dataclasses import dataclass


class ContractError(ValueError):
    """Only fixed codes cross the untrusted input boundary."""


@dataclass(frozen=True)
class SourceBindings:
    floorp: str
    runtime: str


@dataclass(frozen=True)
class ReplayContext:
    run_id: str
    origin: str
    desktop_ua: str
    mobile_ua: str
    sources: SourceBindings


@dataclass(frozen=True)
class Action:
    run_id: str
    step: int
    action_id: str


@dataclass(frozen=True)
class RedirectEvent:
    nonce: str
    case_id: str
    hop: int
    url: str
    host: str
    method: str
    status: int
    location: str | None
    user_agent: str


@dataclass(frozen=True)
class Observation:
    run_id: str
    step: int
    action_id: str
    mobile_enabled: bool
    redirect: tuple[RedirectEvent, ...]
    normal_tab_uas: tuple[str, ...]
    other_panel_uas: tuple[str, ...]
