# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mach.decorators import Command, CommandArgument

TODO_FILL_DESCRIPTION = "TODO: Fill in this description. You _can_ use **markdown**."
GENERATED_BY_DESCRIPTION = (
    "This event was generated to correspond to the Legacy Telemetry event {}."
)
DESCRIPTION_INDENT = "      "
LIST_INDENT = "      - "
BUG_URL_TEMPLATE = "https://bugzil.la/{}"

GLEAN_EVENT_TEMPLATE = """
{category}:
  {name}:
    type: event
    description: >
{multiline_description}
    bugs: {bugs_alias}{bugs_list}
    data_reviews: {data_alias}{bugs_list}
    notification_emails:
{emails_list}
    expires: {expiry}
    extra_keys: {extra_alias}{extra_keys}
    telemetry_mirror: {legacy_enum}
""".strip(
    "\n"
)

EXTRA_KEY_DESCRIPTION_INDENT = DESCRIPTION_INDENT + "    "
VALUE_EXTRA_DESCRIPTION = "The `value` of the event. Mirrors to the Legacy Telemetry event's `value` parameter."
EXTRA_KEY_TEMPLATE = """
      {name}:
        description: >
{description}
        type: string
""".strip(
    "\n"
)


@Command(
    "gifft",
    category="misc",
    description="Generate a Glean metric definition for a given Legacy Telemetry probe. Only supports Events.",
)
@CommandArgument(
    "telemetry_probe_name", help="Telemetry probe name (e.g. readermode.view)"
)
def mach_gifft(command_context, telemetry_probe_name):
    from os import path

    telemetrydir = path.join(
        command_context.topsrcdir, "toolkit", "components", "telemetry"
    )

    import itertools
    import sys

    sys.path.append(path.join(telemetrydir, "build_scripts"))
    import textwrap

    from mozparsers import parse_events

    events = parse_events.load_events(path.join(telemetrydir, "Events.yaml"), True)
    for e in events:
        if e.category + "." + e.name == telemetry_probe_name:
            # There are four levels of identification in Legacy Telemetry event
            # definitions: category, name/family, method, and object.
            # If not present, the `method` property will supply the name/family.
            # GIFFT will mirror to a specific identified C++ enum, which means
            # we need to generate Glean events for every combination of method
            # and object.
            category = e.category
            bugs_alias = data_alias = extra_alias = ""
            for m, o in itertools.product(e.methods, e.objects):
                legacy_name = category + "." + m + "#" + o
                name = m + "_" + o
                description = e._definition.get("description", TODO_FILL_DESCRIPTION)
                multiline_description = textwrap.fill(
                    description,
                    width=80 - len(DESCRIPTION_INDENT),
                    initial_indent=DESCRIPTION_INDENT,
                    subsequent_indent=DESCRIPTION_INDENT,
                )
                multiline_description += "\n"
                multiline_description += textwrap.fill(
                    GENERATED_BY_DESCRIPTION.format(legacy_name),
                    width=80 - len(DESCRIPTION_INDENT),
                    initial_indent=DESCRIPTION_INDENT,
                    subsequent_indent=DESCRIPTION_INDENT,
                )

                if bugs_alias:
                    bugs_list = ""
                else:
                    bugs_alias = f"{category}_{m}_bugs"
                    data_alias = f"{category}_{m}_data_reviews"
                    bugs_list = "\n" + textwrap.indent(
                        "\n".join(
                            map(
                                lambda b: BUG_URL_TEMPLATE.format(b),
                                e._definition.get("bug_numbers", []),
                            )
                        ),
                        LIST_INDENT,
                    )
                emails_list = textwrap.indent(
                    "\n".join(e._definition.get("notification_emails", [])), LIST_INDENT
                )

                # expiry_version is a string like `"123.0a1"` or `"never"`,
                # but Glean wants a number like `123` or `never`.
                expiry = e.expiry_version.strip('"').split(".")[0]

                if extra_alias:
                    extra_keys = ""
                else:
                    extra_alias = f"{category}_{m}_extra"
                    multiline_extra_description = textwrap.fill(
                        VALUE_EXTRA_DESCRIPTION,
                        width=80 - len(EXTRA_KEY_DESCRIPTION_INDENT),
                        initial_indent=EXTRA_KEY_DESCRIPTION_INDENT,
                        subsequent_indent=EXTRA_KEY_DESCRIPTION_INDENT,
                    )
                    extra_keys = "\n" + EXTRA_KEY_TEMPLATE.format(
                        name="value", description=multiline_extra_description
                    )
                    for key_name, key_description in e._definition.get(
                        "extra_keys", {}
                    ).items():
                        extra_keys += "\n"
                        extra_keys += EXTRA_KEY_TEMPLATE.format(
                            name=key_name,
                            description=textwrap.indent(
                                key_description, EXTRA_KEY_DESCRIPTION_INDENT
                            ),
                        )

                legacy_enum = (
                    parse_events.convert_to_cpp_identifier(category, ".") + "_"
                )
                legacy_enum += parse_events.convert_to_cpp_identifier(m, "_") + "_"
                legacy_enum += parse_events.convert_to_cpp_identifier(o, "_")

                print(
                    GLEAN_EVENT_TEMPLATE.format(
                        category=category,
                        name=name,
                        multiline_description=multiline_description,
                        bugs_alias=(
                            f"&{bugs_alias}" if bugs_list else f"*{bugs_alias}"
                        ),
                        bugs_list=bugs_list,
                        data_alias=(
                            f"&{data_alias}" if bugs_list else f"*{data_alias}"
                        ),
                        emails_list=emails_list,
                        expiry=expiry,
                        extra_alias=(
                            f"&{extra_alias}" if extra_keys else f"*{extra_alias}"
                        ),
                        extra_keys=extra_keys,
                        legacy_enum=legacy_enum,
                    )
                )
                print()  # We want a newline between event definitions.
