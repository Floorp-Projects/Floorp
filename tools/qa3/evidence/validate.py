# SPDX-License-Identifier: MPL-2.0
"""EvidenceManifest v0 validator (frozen ADR 8061068 sections 1.1-1.5).

The JSON Schema file next to this module is the contract. This module adds the
checks a JSON Schema cannot express on its own:

* unknown ``schemaVersion`` rejection (checked before anything else),
* the state-transition table from ``state-transitions.json``,
* ``cancel_requested`` agreement between the TaskStatus body and the Envelope,
* old-epoch report rejection (central/Scheduler role only),
* ID issuer namespaces, entity kinds, relationship endpoints and operation order,
* required limitation entries for the explicit ``UNVERIFIED``/absent values.

Error codes are shared with ``validate.ts`` so the Scheduler and the Runner can
reject the same fixtures with the same reason.
"""

from __future__ import annotations

import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCHEMA_PATH = HERE / "evidence-manifest.schema.json"
TRANSITIONS_PATH = HERE / "state-transitions.json"

SCHEMA_VERSION = 1
UNVERIFIED = "UNVERIFIED"

ISSUER_NAMESPACE = {
    "scheduler": "scheduler:",
    "executor": "executor:",
    "host": "host:",
    "host-authorization": "host-authorization:",
}

ENTITY_KINDS = (
    "task",
    "job",
    "run",
    "attempt",
    "session",
    "workspace",
    "approval",
)

CORE_RELATIONSHIPS = (
    "task-has-job",
    "job-has-run",
    "run-has-attempt",
    "run-uses-session",
    "run-in-workspace",
)

# Limitation substrings that must accompany an explicit absence in the record.
LIMITATION_MARKERS = {
    "dsh-pin": "DSH",
    "commit": "commit",
    "arch": "arch",
    "ownership": "ownership",
    "visual": "visual",
    "timestamp": "timestamp",
}

_schema_cache = None
_transitions_cache = None


def read_json(path):
    """Read JSON and reject duplicate object keys (QA3 convention)."""

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON field {key!r} in {path}")
            result[key] = value
        return result

    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique)


def load_schema():
    global _schema_cache
    if _schema_cache is None:
        _schema_cache = read_json(SCHEMA_PATH)
    return _schema_cache


def load_transitions():
    global _transitions_cache
    if _transitions_cache is None:
        data = read_json(TRANSITIONS_PATH)
        if data.get("schemaVersion") != 1:
            raise ValueError("unsupported state-transition table schema")
        _transitions_cache = data
    return _transitions_cache


def _error(code, path, message):
    return {"code": code, "path": path or "/", "message": message}


def _resolve_ref(ref, root):
    if not ref.startswith("#/"):
        raise ValueError(f"unsupported reference {ref}")
    node = root
    for raw in ref[2:].split("/"):
        token = raw.replace("~1", "/").replace("~0", "~")
        if not isinstance(node, dict) or token not in node:
            raise ValueError(f"unresolvable reference {ref}")
        node = node[token]
    return node


def _type_matches(value, expected):
    if expected == "object":
        return isinstance(value, dict)
    if expected == "array":
        return isinstance(value, list)
    if expected == "string":
        return isinstance(value, str)
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "null":
        return value is None
    return False


def _type_ok(value, expected):
    if isinstance(expected, list):
        return any(_type_matches(value, item) for item in expected)
    return _type_matches(value, expected)


def _suberrors(instance, schema, root, path):
    errors = []
    _check(instance, schema, root, path, errors)
    return errors


def _check(instance, schema, root, path, errors):
    if not isinstance(schema, dict):
        errors.append(_error("schema-violation", path, "unsupported schema node"))
        return

    if "$ref" in schema:
        _check(instance, _resolve_ref(schema["$ref"], root), root, path, errors)
        return

    if "type" in schema and not _type_ok(instance, schema["type"]):
        expected = schema["type"]
        errors.append(
            _error(
                "schema-violation",
                path,
                f"expected type {expected!r}, got {type(instance).__name__}",
            )
        )
        return

    if "const" in schema and instance != schema["const"]:
        errors.append(
            _error("schema-violation", path, f"must equal {schema['const']!r}")
        )

    if "enum" in schema and instance not in schema["enum"]:
        errors.append(
            _error("schema-violation", path, f"must be one of {schema['enum']!r}")
        )

    if isinstance(instance, str):
        if "minLength" in schema and len(instance) < schema["minLength"]:
            errors.append(
                _error(
                    "schema-violation",
                    path,
                    f"shorter than {schema['minLength']} characters",
                )
            )
        if "maxLength" in schema and len(instance) > schema["maxLength"]:
            errors.append(
                _error(
                    "schema-violation",
                    path,
                    f"longer than {schema['maxLength']} characters",
                )
            )
        if "pattern" in schema and not re.search(schema["pattern"], instance):
            errors.append(
                _error("schema-violation", path, f"does not match {schema['pattern']!r}")
            )

    if isinstance(instance, bool):
        pass
    elif isinstance(instance, (int, float)):
        if "minimum" in schema and instance < schema["minimum"]:
            errors.append(
                _error("schema-violation", path, f"below minimum {schema['minimum']}")
            )
        if "maximum" in schema and instance > schema["maximum"]:
            errors.append(
                _error("schema-violation", path, f"above maximum {schema['maximum']}")
            )

    if isinstance(instance, dict):
        for key in schema.get("required", []):
            if key not in instance:
                errors.append(
                    _error(
                        "missing-required-evidence",
                        f"{path}/{key}",
                        f"required field {key!r} is missing",
                    )
                )
        properties = schema.get("properties", {})
        for key, value in instance.items():
            if key in properties:
                _check(value, properties[key], root, f"{path}/{key}", errors)
            elif schema.get("additionalProperties") is False:
                errors.append(
                    _error("schema-violation", f"{path}/{key}", "unexpected field")
                )

    if isinstance(instance, list):
        if "minItems" in schema and len(instance) < schema["minItems"]:
            errors.append(
                _error(
                    "schema-violation",
                    path,
                    f"needs at least {schema['minItems']} items",
                )
            )
        if "maxItems" in schema and len(instance) > schema["maxItems"]:
            errors.append(
                _error(
                    "schema-violation",
                    path,
                    f"allows at most {schema['maxItems']} items",
                )
            )
        if "items" in schema:
            for index, item in enumerate(instance):
                _check(item, schema["items"], root, f"{path}/{index}", errors)

    for sub in schema.get("allOf", []):
        _check(instance, sub, root, path, errors)

    if "anyOf" in schema:
        branches = [_suberrors(instance, sub, root, path) for sub in schema["anyOf"]]
        if not any(len(branch) == 0 for branch in branches):
            errors.append(_error("schema-violation", path, "no anyOf branch matched"))

    if "oneOf" in schema:
        branches = [_suberrors(instance, sub, root, path) for sub in schema["oneOf"]]
        matched = [index for index, branch in enumerate(branches) if not branch]
        if len(matched) != 1:
            errors.append(
                _error(
                    "schema-violation",
                    path,
                    f"oneOf matched {len(matched)} branches, expected exactly 1",
                )
            )


def _limitation_requirements(manifest):
    requirements = []
    runtime = manifest.get("runtime") or {}
    toolchain = runtime.get("toolchain") or {}
    dsh = toolchain.get("dsh") or {}
    if dsh.get("pin") == UNVERIFIED:
        requirements.append("dsh-pin")
    os_block = runtime.get("os") or {}
    if os_block.get("arch") == UNVERIFIED:
        requirements.append("arch")
    build = runtime.get("build") or {}
    floorp = toolchain.get("floorp") or {}
    if build.get("commit") is None or floorp.get("commit") is None:
        requirements.append("commit")
    if any(
        isinstance(entity, dict) and entity.get("owner") == UNVERIFIED
        for entity in manifest.get("envelope", {}).get("entities", [])
    ):
        requirements.append("ownership")
    if manifest.get("visualJudge") is None:
        requirements.append("visual")
    for attempt in manifest.get("attempts") or []:
        if attempt.get("startedAt") is None or attempt.get("endedAt") is None:
            requirements.append("timestamp")
            break
    return requirements


def _check_limitations(manifest, errors):
    limitations = [
        item.lower() for item in manifest.get("limitations", []) if isinstance(item, str)
    ]
    for requirement in _limitation_requirements(manifest):
        marker = LIMITATION_MARKERS[requirement]
        if not any(marker.lower() in item for item in limitations):
            errors.append(
                _error(
                    "missing-limitation",
                    "/limitations",
                    f"a limitation mentioning {marker!r} is required by this record",
                )
            )


def _semantic_checks(manifest, current_epoch, errors):
    envelope = manifest["envelope"]
    transitions = load_transitions()
    table = transitions["transitions"]
    state = envelope["taskStatus"]["state"]

    if current_epoch is not None and envelope["epoch"] < current_epoch:
        errors.append(
            _error(
                "stale-epoch",
                "/envelope/epoch",
                f"report epoch {envelope['epoch']} is older than current epoch "
                f"{current_epoch}",
            )
        )

    step = envelope["stateTransition"]
    if step["to"] != state:
        errors.append(
            _error(
                "invalid-transition",
                "/envelope/stateTransition",
                f"stateTransition.to {step['to']!r} does not equal taskStatus.state "
                f"{state!r}",
            )
        )
    allowed = table.get(step["from"])
    if allowed is None:
        errors.append(
            _error(
                "invalid-transition",
                "/envelope/stateTransition/from",
                f"unknown state {step['from']!r}",
            )
        )
    elif step["to"] not in allowed:
        errors.append(
            _error(
                "invalid-transition",
                "/envelope/stateTransition",
                f"{step['from']!r} -> {step['to']!r} is not in the state-transition "
                "table",
            )
        )

    body_cancel = envelope["taskStatus"]["cancelRequested"]
    envelope_cancel = envelope["cancelRequested"]
    cancel_state = transitions.get("cancelRequestedState", "cancel_requested")
    if body_cancel != envelope_cancel:
        errors.append(
            _error(
                "cancel-state-mismatch",
                "/envelope/cancelRequested",
                "Envelope cancelRequested disagrees with taskStatus.cancelRequested",
            )
        )
    if (state == cancel_state) != bool(body_cancel):
        errors.append(
            _error(
                "cancel-state-mismatch",
                "/envelope/taskStatus",
                f"cancel_requested meaning must hold for state {state!r} with "
                f"cancelRequested={body_cancel!r}",
            )
        )

    entities = envelope["entities"]
    by_id = {}
    for index, entity in enumerate(entities):
        pointer = f"/envelope/entities/{index}"
        namespace = ISSUER_NAMESPACE.get(entity["issuer"])
        if namespace is None:
            errors.append(
                _error(
                    "invalid-id-namespace",
                    f"{pointer}/issuer",
                    f"unknown issuer {entity['issuer']!r}",
                )
            )
            continue
        if not entity["id"].startswith(namespace):
            errors.append(
                _error(
                    "invalid-id-namespace",
                    f"{pointer}/id",
                    f"id {entity['id']!r} is not in the {namespace!r} namespace of "
                    f"issuer {entity['issuer']!r}",
                )
            )
        if entity["id"] in by_id:
            errors.append(
                _error(
                    "duplicate-entity-id",
                    f"{pointer}/id",
                    f"duplicate entity id {entity['id']!r}",
                )
            )
        by_id[entity["id"]] = entity

    present_kinds = {entity["kind"] for entity in entities}
    for kind in ENTITY_KINDS:
        if kind not in present_kinds:
            errors.append(
                _error(
                    "missing-entity",
                    "/envelope/entities",
                    f"entity kind {kind!r} is required by the Envelope",
                )
            )

    relationship_types = set()
    for index, rel in enumerate(envelope["relationships"]):
        pointer = f"/envelope/relationships/{index}"
        relationship_types.add(rel["type"])
        for side in ("from", "to"):
            if rel[side] not in by_id:
                errors.append(
                    _error(
                        "unknown-entity-reference",
                        f"{pointer}/{side}",
                        f"unknown entity id {rel[side]!r}",
                    )
                )
    for required in CORE_RELATIONSHIPS:
        if required not in relationship_types:
            errors.append(
                _error(
                    "missing-relationship",
                    "/envelope/relationships",
                    f"relationship {required!r} is required by the Envelope",
                )
            )

    for index, attempt in enumerate(manifest["attempts"]):
        if attempt["run"] not in by_id:
            errors.append(
                _error(
                    "unknown-entity-reference",
                    f"/attempts/{index}/run",
                    f"unknown run id {attempt['run']!r}",
                )
            )

    orders = [operation["order"] for operation in manifest["operations"]]
    if sorted(orders) != list(range(1, len(orders) + 1)):
        errors.append(
            _error(
                "invalid-operation-order",
                "/operations",
                f"operation order must be unique and 1..{len(orders)}, got {orders!r}",
            )
        )

    _check_limitations(manifest, errors)


def validate_manifest(value, current_epoch=None, role="runner"):
    """Validate one EvidenceManifest document.

    ``role="scheduler"`` mirrors central intake: the current epoch must be known,
    otherwise the report is rejected with ``missing-current-epoch``. The Runner
    validates without a central epoch (ADR 1.2: an old Runner's external writes
    are not stopped by central rejection).
    """

    if not isinstance(value, dict):
        return [_error("schema-violation", "/", "manifest must be an object")]

    if "schemaVersion" not in value:
        return [
            _error(
                "missing-required-evidence",
                "/schemaVersion",
                "required field 'schemaVersion' is missing",
            )
        ]
    if type(value["schemaVersion"]) is not int or value["schemaVersion"] != SCHEMA_VERSION:
        return [
            _error(
                "unsupported-schema-version",
                "/schemaVersion",
                f"unsupported schemaVersion {value['schemaVersion']!r}; "
                f"expected {SCHEMA_VERSION}",
            )
        ]

    if role == "scheduler" and current_epoch is None:
        return [
            _error(
                "missing-current-epoch",
                "/envelope/epoch",
                "central intake requires the current epoch",
            )
        ]

    errors = []
    _check(value, load_schema(), load_schema(), "", errors)
    if errors:
        return errors

    _semantic_checks(value, current_epoch, errors)
    return errors


def validate_report_as_scheduler(value, current_epoch):
    """Central (Scheduler) intake: same rules plus the current epoch requirement."""

    return validate_manifest(value, current_epoch=current_epoch, role="scheduler")


def is_valid(value, current_epoch=None, role="runner"):
    return not validate_manifest(value, current_epoch=current_epoch, role=role)
