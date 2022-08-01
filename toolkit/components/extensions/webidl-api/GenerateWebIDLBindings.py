# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import difflib
import json
import logging
import os
import subprocess
import sys
import tempfile

try:
    import buildconfig
    import jinja2
    import jsonschema
    import mozpack.path as mozpath
except ModuleNotFoundError as e:
    print(
        "This script should be executed using `mach python %s`\n" % __file__,
        file=sys.stderr,
    )
    raise e

WEBIDL_DIR = mozpath.join("dom", "webidl")
WEBIDL_DIR_FULLPATH = mozpath.join(buildconfig.topsrcdir, WEBIDL_DIR)

CPP_DIR = mozpath.join("toolkit", "components", "extensions", "webidl-api")
CPP_DIR_FULLPATH = mozpath.join(buildconfig.topsrcdir, CPP_DIR)

# Absolute path to the base dir for this script.
BASE_DIR = CPP_DIR_FULLPATH

# TODO(Bug 1724785): a patch to introduce the doc page linked below is attached to
# this bug and meant to ideally land along with this patch.
DOCS_NEXT_STEPS = """
The following documentation page provides more in depth details of the next steps:

https://firefox-source-docs.mozilla.org/toolkit/components/extensions/webextensions/wiring_up_new_webidl_bindings.html
"""

# Load the configuration file.
glbl = {}
with open(mozpath.join(BASE_DIR, "ExtensionWebIDL.conf")) as f:
    exec(f.read(), glbl)

# Special mapping between the JSON schema type and the related WebIDL type.
WEBEXT_TYPES_MAPPING = glbl["WEBEXT_TYPES_MAPPING"]

# Special mapping for the `WebExtensionStub` to be used for API methods that
# require special handling.
WEBEXT_STUBS_MAPPING = glbl["WEBEXT_STUBS_MAPPING"]

# Schema entries that should be hidden in workers.
WEBEXT_WORKER_HIDDEN_SET = glbl["WEBEXT_WORKER_HIDDEN_SET"]

# Set of the webidl type names to be threated as primitive types.
WEBIDL_PRIMITIVE_TYPES = glbl["WEBIDL_PRIMITIVE_TYPES"]

# Mapping table for the directory where the JSON schema are going to be loaded from,
# the 'toolkit' ones are potentially available on both desktop and mobile builds
# (if not specified otherwise through the WEBEXT_ANDROID_EXCLUDED list), whereas the
# 'browser' and 'mobile' ones are only available on desktop and mobile builds
# respectively.
#
# `load_and_parse_JSONSchema` will iterate over this map and will call `Schemas`
# load_schemas method passing the path to the directory with the schema data and the
# related key from this map as the `schema_group` associated with all the schema data
# being loaded.
#
# Schema data loaded from different groups may potentially overlap, and the resulting
# generated webidl may contain preprocessing macro to conditionally include different
# webidl signatures on different builds (in particular for the Desktop vs. Android
# differences).
WEBEXT_SCHEMADIRS_MAPPING = glbl["WEBEXT_SCHEMADIRS_MAPPING"]

# List of toolkit-level WebExtensions API namespaces that are not included in
# android builds.
WEBEXT_ANDROID_EXCLUDED = glbl["WEBEXT_ANDROID_EXCLUDED"]

# Define a custom jsonschema validation class
WebExtAPIValidator = jsonschema.validators.extend(
    jsonschema.validators.Draft4Validator,
)
# Hack: inject any as a valid simple types.
WebExtAPIValidator.META_SCHEMA["definitions"]["simpleTypes"]["enum"].append("any")


def run_diff(diff_cmd, left_name, left_text, right_name, right_text):
    """
    Creates two temporary files and run the given `diff_cmd` to generate a diff
    between the two temporary files (used to generate diffs related to the JSON
    Schema files for desktop and mobile builds)
    """

    diff_output = ""

    # Generate the diff using difflib if diff_cmd isn't set.
    if diff_cmd is None:
        diff_generator = difflib.unified_diff(
            left_text.splitlines(keepends=True),
            right_text.splitlines(keepends=True),
            fromfile=left_name,
            tofile=right_name,
        )
        diff_output = "".join(diff_generator)
    else:
        # Optionally allow to generate the diff using an external diff tool
        # (e.g. choosing `icdiff` through `--diff-command icdiff` would generate
        # colored side-by-side diffs).
        with tempfile.NamedTemporaryFile("w+t", prefix="%s-" % left_name) as left_file:
            with tempfile.NamedTemporaryFile(
                "w+t", prefix="%s-" % right_name
            ) as right_file:
                left_file.write(left_text)
                left_file.flush()
                right_file.write(right_text)
                right_file.flush()
                diff_output = subprocess.run(
                    [diff_cmd, "-u", left_file.name, right_file.name],
                    capture_output=True,
                ).stdout.decode("utf-8")

    if len(diff_output) == 0:
        return "Diff empty: both files have the exact same content."

    return diff_output


def read_json(json_file_path):
    """
    Helper function used to read the WebExtensions API schema JSON files
    by ignoring the license comment on the top of some of those files.
    Same helper as the one available in Schemas.jsm:
    https://searchfox.org/mozilla-central/rev/3434a9df60373a997263107e6f124fb164ddebf2/toolkit/components/extensions/Schemas.jsm#70
    """
    with open(json_file_path) as json_file:
        txt = json_file.read()
        # Chrome JSON files include a license comment that we need to
        # strip off for this to be valid JSON. As a hack, we just
        # look for the first '[' character, which signals the start
        # of the JSON content.
        return json.loads(txt[txt.index("[") :])


def write_with_overwrite_confirm(
    relpath,
    abspath,
    newcontent,
    diff_prefix,
    diff_command=None,
    overwrite_existing=False,
):
    is_overwriting = False
    no_changes = False

    # Make sure generated files do have a newline at the end of the file.
    if newcontent[-1] != "\n":
        newcontent = newcontent + "\n"

    if os.path.exists(abspath):
        with open(abspath, "r") as existingfile:
            existingcontent = existingfile.read()
        if existingcontent == newcontent:
            no_changes = True
        elif not overwrite_existing:
            print("Found existing %s.\n" % relpath, file=sys.stderr)
            print(
                "(Run again with --overwrite-existing to allow overwriting it automatically)",
                file=sys.stderr,
            )
            data = ""
            while data not in ["Y", "N", "D"]:
                data = input(
                    "\nOverwrite %s? (Y = Yes / N = No / D = Diff)\n" % relpath
                ).upper()
                if data == "N":
                    print(
                        "Aborted saving updated content to file %s" % relpath,
                        file=sys.stderr,
                    )
                    return False
                elif data == "D":
                    print(
                        run_diff(
                            diff_command,
                            "%s--existing" % diff_prefix,
                            "".join(open(abspath, "r").readlines()),
                            "%s--updated" % diff_prefix,
                            newcontent,
                        )
                    )
                    data = ""  # Ask confirmation again after printing diff.
                elif data == "Y":
                    is_overwriting = True
                    break
        else:
            is_overwriting = True

    if is_overwriting:
        print("Overwriting %s.\n" % relpath, file=sys.stderr)

    if no_changes:
        print("No changes for the existing %s.\n" % relpath, file=sys.stderr)
    else:
        with open(abspath, "w") as dest_file:
            dest_file.write(newcontent)
        print("Wrote new content in file %s" % relpath)

    # Return true if there were changes written on disk
    return not no_changes


class DefaultDict(dict):
    def __init__(self, createDefault):
        self._createDefault = createDefault

    def getOrCreate(self, key):
        if key not in self:
            self[key] = self._createDefault(key)
        return self[key]


class WebIDLHelpers:
    """
    A collection of helpers used to generate the WebIDL definitions for the
    API entries loaded from the collected JSON schema files.
    """

    @classmethod
    def expect_instance(cls, obj, expected_class):
        """
        Raise a TypeError if `obj` is not an instance of `Class`.
        """

        if not isinstance(obj, expected_class):
            raise TypeError(
                "Unexpected object type, expected %s: %s" % (expected_class, obj)
            )

    @classmethod
    def namespace_to_webidl_definition(cls, api_ns, schema_group):
        """
        Generate the WebIDL definition for the given APINamespace instance.
        """

        # TODO: schema_group is currently unused in this method.
        template = api_ns.root.jinja_env.get_template("ExtensionAPI.webidl.in")
        return template.render(cls.to_template_props(api_ns))

    @classmethod
    def to_webidl_definition_name(cls, text):
        """
        Convert a namespace name into its related webidl definition name.
        """

        # Join namespace parts, with capitalized first letters.
        name = "Extension"
        for part in text.split("."):
            name += part[0].upper() + part[1:]
        return name

    @classmethod
    def to_template_props(cls, api_ns):
        """
        Convert an APINamespace object its the set of properties that are
        expected by the webidl template.
        """

        cls.expect_instance(api_ns, APINamespace)

        webidl_description_comment = (
            '// WebIDL definition for the "%s" WebExtensions API' % api_ns.name
        )
        webidl_name = cls.to_webidl_definition_name(api_ns.name)

        # TODO: some API should not be exposed to service workers (e.g. runtime.getViews),
        # refer to a config file to detect this kind of exceptions/special cases.
        #
        # TODO: once we want to expose the WebIDL bindings to extension windows
        # and not just service workers we will need to add "Window" to the
        # webidl_exposed_attr and only expose APIs with allowed_context "devtools_only"
        # on Windows.
        #
        # e.g.
        # if "devtools_only" in api_ns.allowed_contexts:
        #     webidl_exposed_attr = ", ".join(["Window"])
        # else:
        #    webidl_exposed_attr = ", ".join(["ServiceWorker", "Window"])
        if "devtools_only" in api_ns.allowed_contexts:
            raise Exception("Not yet supported: devtools_only allowed_contexts")

        if "content_only" in api_ns.allowed_contexts:
            raise Exception("Not yet supported: content_only allowed_contexts")

        webidl_exposed_attr = ", ".join(["ServiceWorker"])

        webidl_definition_body = cls.to_webidl_definition_body(api_ns)
        return {
            "api_namespace": api_ns.name,
            "webidl_description_comment": webidl_description_comment,
            "webidl_name": webidl_name,
            "webidl_exposed_attr": webidl_exposed_attr,
            "webidl_definition_body": webidl_definition_body,
        }

    @classmethod
    def to_webidl_definition_body(cls, api_ns):
        """
        Generate the body of an API namespace webidl definition.
        """

        cls.expect_instance(api_ns, APINamespace)

        body = []

        # TODO: once we are going to expose the webidl bindings to
        # content scripts we should generate a separate definition
        # for the content_only parts of the API namespaces and make
        # them part of a separate `ExtensionContent<APINamespace>`
        # webidl interface (e.g. `ExtensionContentUserScripts` would
        # contain only the part of the userScripts API namespace that
        # should be available to the content scripts globals.
        def should_include(api_entry):
            if isinstance(
                api_entry, APIFunction
            ) and WebIDLHelpers.webext_method_hidden_in_worker(api_entry):
                return False
            if api_entry.is_mv2_only:
                return False
            return "content_only" not in api_entry.get_allowed_contexts()

        webidl_functions = [
            cls.to_webidl_method(v)
            for v in api_ns.functions.values()
            if should_include(v)
        ]
        if len(webidl_functions) > 0:
            body = body + ["\n  // API methods.\n", "\n\n".join(webidl_functions)]

        webidl_events = [
            cls.to_webidl_event_property(v)
            for v in api_ns.events.values()
            if should_include(v)
        ]
        if len(webidl_events) > 0:
            body = body + ["\n  // API events.\n", "\n\n".join(webidl_events)]

        webidl_props = [
            cls.to_webidl_property(v)
            for v in api_ns.properties.values()
            if should_include(v)
        ]
        if len(webidl_props) > 0:
            body = body + ["\n  // API properties.\n", "\n\n".join(webidl_props)]

        webidl_child_ns = [
            cls.to_webidl_namespace_property(v)
            for v in api_ns.get_child_namespaces()
            if should_include(v)
        ]
        if len(webidl_child_ns) > 0:
            body = body + [
                "\n  // API child namespaces.\n",
                "\n\n".join(webidl_child_ns),
            ]

        return "\n".join(body)

    @classmethod
    def to_webidl_namespace_property(cls, api_ns):
        """
        Generate the webidl fragment for a child APINamespace property (an
        API namespace included in a parent API namespace, e.g. `devtools.panels`
        is a child namespace for `devtools` and `privacy.network` is a child
        namespace for `privacy`).
        """

        cls.expect_instance(api_ns, APINamespace)

        # TODO: at the moment this method is not yet checking if an entry should
        # be wrapped into build time macros in the generated webidl definitions
        # (as done for methods and event properties).
        #
        # We may look into it if there is any property that needs this
        # (at the moment it seems that we may defer it)

        prop_name = api_ns.name[api_ns.name.find(".") + 1 :]
        prop_type = WebIDLHelpers.to_webidl_definition_name(api_ns.name)
        attrs = [
            "Replaceable",
            "SameObject",
            'BinaryName="Get%s"' % prop_type,
            'Func="mozilla::extensions::%s::IsAllowed' % prop_type,
        ]

        lines = [
            "  [%s]" % ", ".join(attrs),
            "  readonly attribute %s %s;" % (prop_type, prop_name),
        ]

        return "\n".join(lines)

    @classmethod
    def to_webidl_definition(cls, api_entry, schema_group):
        """
        Convert a API namespace or entry class instance into its webidl
        definition.
        """

        if isinstance(api_entry, APINamespace):
            return cls.namespace_to_webidl_definition(api_entry, schema_group)
        if isinstance(api_entry, APIFunction):
            return cls.to_webidl_method(api_entry, schema_group)
        if isinstance(api_entry, APIProperty):
            return cls.to_webidl_property(api_entry, schema_group)
        if isinstance(api_entry, APIEvent):
            return cls.to_webidl_event_property(api_entry, schema_group)
        if isinstance(api_entry, APIType):
            # return None for APIType instances, which are currently not being
            # turned into webidl definitions.
            return None

        raise Exception("Unknown api_entry type: %s" % api_entry)

    @classmethod
    def to_webidl_property(cls, api_property, schema_group=None):
        """
        Returns the WebIDL fragment for the given APIProperty entry to be included
        in the body of a WebExtension API namespace webidl definition.
        """

        cls.expect_instance(api_property, APIProperty)

        # TODO: at the moment this method is not yet checking if an entry should
        # be wrapped into build time macros in the generated webidl definitions
        # (as done for methods and event properties).
        #
        # We may look into it if there is any property that needs this
        # (at the moment it seems that we may defer it)

        attrs = ["Replaceable"]

        schema_data = api_property.get_schema_data(schema_group)
        proptype = cls.webidl_type_from_mapping(
            schema_data, "%s property type" % api_property.api_path_string
        )

        lines = [
            "  [%s]" % ", ".join(attrs),
            "  readonly attribute %s %s;" % (proptype, api_property.name),
        ]

        return "\n".join(lines)

    @classmethod
    def to_webidl_event_property(cls, api_event, schema_group=None):
        """
        Returns the WebIDL fragment for the given APIEvent entry to be included
        in the body of a WebExtension API namespace webidl definition.
        """

        cls.expect_instance(api_event, APIEvent)

        def generate_webidl(group):
            # Empty if the event doesn't exist in the given schema_group.
            if group and group not in api_event.schema_groups:
                return ""
            attrs = ["Replaceable", "SameObject"]
            return "\n".join(
                [
                    "  [%s]" % ", ".join(attrs),
                    "  readonly attribute ExtensionEventManager %s;" % api_event.name,
                ]
            )

        if schema_group is not None:
            return generate_webidl(schema_group)

        return cls.maybe_wrap_in_buildtime_macros(api_event, generate_webidl)

    @classmethod
    def to_webidl_method(cls, api_fun, schema_group=None):
        """
        Returns the WebIDL definition for the given APIFunction entry to be included
        in the body of a WebExtension API namespace webidl definition.
        """

        cls.expect_instance(api_fun, APIFunction)

        def generate_webidl(group):
            attrs = ["Throws"]
            stub_attr = cls.webext_method_stub(api_fun, group)
            if stub_attr:
                attrs = attrs + [stub_attr]
            retval_type = cls.webidl_method_retval_type(api_fun, group)
            lines = []
            for fn_params in api_fun.iter_multiple_webidl_signatures_params(group):
                params = ", ".join(cls.webidl_method_params(api_fun, group, fn_params))
                lines.extend(
                    [
                        "  [%s]" % ", ".join(attrs),
                        "  %s %s(%s);" % (retval_type, api_fun.name, params),
                    ]
                )
            return "\n".join(lines)

        if schema_group is not None:
            return generate_webidl(schema_group)

        return cls.maybe_wrap_in_buildtime_macros(api_fun, generate_webidl)

    @classmethod
    def maybe_wrap_in_buildtime_macros(cls, api_entry, generate_webidl_fn):
        """
        Wrap the generated webidl content into buildtime macros if there are
        differences between Android and Desktop JSON schema that turns into
        different webidl definitions.
        """

        browser_webidl = None
        mobile_webidl = None

        if api_entry.in_browser:
            browser_webidl = generate_webidl_fn("browser")
        elif api_entry.in_toolkit:
            browser_webidl = generate_webidl_fn("toolkit")

        if api_entry.in_mobile:
            mobile_webidl = generate_webidl_fn("mobile")

        # Generate a method signature surrounded by `#if defined(ANDROID)` macros
        # to conditionally exclude APIs that are not meant to be available in
        # Android builds.
        if api_entry.in_browser and not api_entry.in_mobile:
            return "#if !defined(ANDROID)\n%s\n#endif" % browser_webidl

        # NOTE: at the moment none of the API seems to be exposed on mobile but
        # not on desktop.
        if api_entry.in_mobile and not api_entry.in_browser:
            return "#if defined(ANDROID)\n%s\n#endif" % mobile_webidl

        # NOTE: at the moment none of the API seems to be available in both
        # mobile and desktop builds and have different webidl signature
        # (at least until not all method param types are converted into non-any
        # webidl type signatures)
        if browser_webidl != mobile_webidl and mobile_webidl is not None:
            return "#if defined(ANDROID)\n%s\n#else\n%s\n#endif" % (
                mobile_webidl,
                browser_webidl,
            )

        return browser_webidl

    @classmethod
    def webext_method_hidden_in_worker(cls, api_fun, schema_group=None):
        """
        Determine if a method should be hidden in the generated webidl
        for a worker global.
        """
        cls.expect_instance(api_fun, APIFunction)
        api_path = ".".join([*api_fun.path])
        return api_path in WEBEXT_WORKER_HIDDEN_SET

    @classmethod
    def webext_method_stub(cls, api_fun, schema_group=None):
        """
        Returns the WebExtensionStub WebIDL extended attribute for the given APIFunction.
        """

        cls.expect_instance(api_fun, APIFunction)

        stub = "WebExtensionStub"

        api_path = ".".join([*api_fun.path])

        if api_path in WEBEXT_STUBS_MAPPING:
            logging.debug("Looking for %s in WEBEXT_STUBS_MAPPING", api_path)
            # if the stub config for a given api_path is a boolean, then do not stub the
            # method if it is set to False and use the default one if set to true.
            if isinstance(WEBEXT_STUBS_MAPPING[api_path], bool):
                if not WEBEXT_STUBS_MAPPING[api_path]:
                    return ""
                else:
                    return "%s" % stub
            return '%s="%s"' % (stub, WEBEXT_STUBS_MAPPING[api_path])

        schema_data = api_fun.get_schema_data(schema_group)

        is_ambiguous = False
        if "allowAmbiguousOptionalArguments" in schema_data:
            is_ambiguous = True

        if api_fun.is_async():
            if is_ambiguous:
                # Specialized stub for async methods with ambiguous args.
                return '%s="AsyncAmbiguous"' % stub
            return '%s="Async"' % stub

        if "returns" in schema_data:
            # If the method requires special handling just add it to
            # the WEBEXT_STUBS_MAPPING table.
            return stub

        return '%s="NoReturn"' % stub

    @classmethod
    def webidl_method_retval_type(cls, api_fun, schema_group=None):
        """
        Return the webidl return value type for the given `APIFunction` entry.

        If the JSON schema for the method is not marked as asynchronous and
        there is a `returns` schema property, the return type will be defined
        from it (See WebIDLHelpers.webidl_type_from_mapping for more info about
        the type mapping).
        """

        cls.expect_instance(api_fun, APIFunction)

        if api_fun.is_async(schema_group):
            # webidl signature for the Async methods will return any, then
            # the implementation will return a Promise if no callback was passed
            # to the method and undefined if the optional chrome compatible callback
            # was passed as a parameter.
            return "any"

        schema_data = api_fun.get_schema_data(schema_group)
        if "returns" in schema_data:
            return cls.webidl_type_from_mapping(
                schema_data["returns"], "%s return value" % api_fun.api_path_string
            )

        return "void"

    @classmethod
    def webidl_method_params(cls, api_fun, schema_group=None, params_schema_data=None):
        """
        Return the webidl method parameters for the given `APIFunction` entry.

        If the schema for the function includes `allowAmbiguousOptionalArguments`
        then the methods paramers are going to be the variadic arguments of type
        `any` (e.g. `void myMethod(any... args);`).

        If params_schema_data is None, then the parameters will be resolved internally
        from the schema data.
        """

        cls.expect_instance(api_fun, APIFunction)

        params = []

        schema_data = api_fun.get_schema_data(schema_group)

        # Use a variadic positional argument if the methods allows
        # ambiguous optional arguments.
        #
        # The ambiguous mapping is currently used for:
        #
        # - API methods that have an allowAmbiguousOptionalArguments
        #   property in their JSONSchema definition
        #   (e.g. browser.runtime.sendMessage)
        #
        # - API methods for which the currently autogenerated
        #   methods are not all distinguishable from a WebIDL
        #   parser perspective
        #   (e.g. scripting.getRegisteredContentScripts and
        #   scripting.unregisterContentScripts, where
        #   `any filter, optional Function` and `optional Function`
        #   are not distinguishable when called with a single
        #   parameter set to an undefined value).
        if api_fun.has_ambiguous_stub_mapping(schema_group):
            return ["any... args"]

        if params_schema_data is None:
            if "parameters" in schema_data:
                params_schema_data = schema_data["parameters"]
            else:
                params_schema_data = []

        for param in params_schema_data:
            is_optional = "optional" in param and param["optional"]

            if (
                api_fun.is_async(schema_group)
                and schema_data["async"] == param["name"]
                and schema_data["parameters"][-1] == param
            ):
                # the last async callback parameter is validated and added later
                # in this method.
                continue

            ptype = cls.webidl_type_from_mapping(
                param,
                "%s method parameter %s" % (api_fun.api_path_string, param["name"]),
            )

            if (
                ptype != "any"
                and not cls.webidl_type_is_primitive(ptype)
                and is_optional
            ):
                if ptype != "Function":
                    raise TypeError(
                        "unexpected optional type. "
                        "Only Function is expected to be marked as optional"
                    )
                ptype = "optional %s" % ptype

            params.append("%s %s" % (ptype, param["name"]))

        if api_fun.is_async(schema_group):
            # Add the chrome-compatible callback as an additional optional parameter
            # when the method is async.
            #
            # The parameter name will be "callback" (default) or the custom one set in
            # the schema data (`get_sync_callback_name` also validates the consistency
            # of the schema data for the callback parameter and throws if the expected
            # parameter is missing).
            params.append(
                "optional Function %s" % api_fun.get_async_callback_name(schema_group)
            )

        return params

    @classmethod
    def webidl_type_is_primitive(cls, webidl_type):
        return webidl_type in WEBIDL_PRIMITIVE_TYPES

    @classmethod
    def webidl_type_from_mapping(cls, schema_data, where_info):
        """
        Return the WebIDL type related to the given `schema_data`.

        The JSON schema type is going to be derived from:
        - `type` and `isInstanceOf` properties
        - or `$ref` property

        and then converted into the related WebIDL type using the
        `WEBEXT_TYPES_MAPPING` table.

        The caller needs also specify where the type mapping
        where meant to be used in form of an arbitrary string
        passed through the `where_info` parameter, which is
        only used to log a more detailed debug message for types
        there couldn't be resolved from the schema data.

        Returns `any` if no special mapping has been found.
        """

        if "type" in schema_data:
            if (
                "isInstanceOf" in schema_data
                and schema_data["isInstanceOf"] in WEBEXT_TYPES_MAPPING
            ):
                schema_type = schema_data["isInstanceOf"]
            else:
                schema_type = schema_data["type"]
        elif "$ref" in schema_data:
            schema_type = schema_data["$ref"]
        else:
            logging.info(
                "%s %s. %s: %s",
                "Falling back to webidl type 'any' for",
                where_info,
                "Unable to get a schema_type from schema data",
                json.dumps(schema_data, indent=True),
            )
            return "any"

        if schema_type in WEBEXT_TYPES_MAPPING:
            return WEBEXT_TYPES_MAPPING[schema_type]

        logging.warning(
            "%s %s. %s: %s",
            "Falling back to webidl type 'any' for",
            where_info,
            "No type mapping found in WEBEXT_TYPES_MAPPING for schema_type",
            schema_type,
        )

        return "any"


class APIEntry:
    """
    Base class for the classes that represents the JSON schema data.
    """

    def __init__(self, parent, name, ns_path):
        self.parent = parent
        self.root = parent.root
        self.name = name
        self.path = [*ns_path, name]

        self.schema_data_list = []
        self.schema_data_by_group = DefaultDict(lambda _: [])

    def add_schema(self, schema_data, schema_group):
        """
        Add schema data loaded from a specific group of schema files.

        Each entry may have more than one schema_data coming from a different group
        of schema files, but only one entry per schema group is currently expected
        and a TypeError is going to raised if this assumption is violated.

        NOTE: entries part of the 'manifest' are expected to have more than one schema_data
        coming from the same group of schema files, but it doesn't represent any actual
        API namespace and so we can ignore them for the purpose of generating the WebIDL
        definitions.
        """

        self.schema_data_by_group.getOrCreate(schema_group).append(schema_data)

        # If the new schema_data is deep equal to an existing one
        # don't bother adding it even if it was in a different schema_group.
        if schema_data not in self.schema_data_list:
            self.schema_data_list.append(schema_data)

        in_manifest_namespace = self.api_path_string.startswith("manifest.")

        # Raise an error if we do have multiple schema entries for the same
        # schema group, but skip it for the "manifest" namespace because it.
        # is expected for it to have multiple schema data entries for the
        # same type and at the moment we don't even use that namespace to
        # generate and webidl definitions.
        if (
            not in_manifest_namespace
            and len(self.schema_data_by_group[schema_group]) > 1
        ):
            raise TypeError(
                'Unxpected multiple schema data for API property "%s" in schema group %s'
                % (self.api_path_string, schema_group)
            )

    def get_allowed_contexts(self, schema_group=None):
        """
        Return the allowed contexts for this API entry, or the default contexts from its
        parent entry otherwise.
        """

        if schema_group is not None:
            if schema_group not in self.schema_data_by_group:
                return []
            if "allowedContexts" in self.schema_data_by_group[schema_group]:
                return self.schema_data_by_group[schema_group]["allowedContexts"]
        else:
            if "allowedContexts" in self.schema_data_list[0]:
                return self.schema_data_list[0]["allowedContexts"]

        if self.parent:
            return self.parent.default_contexts

        return []

    @property
    def schema_groups(self):
        """List of the schema groups that have schema data for this entry."""
        return [*self.schema_data_by_group.keys()]

    @property
    def in_toolkit(self):
        """Whether the API entry is defined by toolkit schemas."""
        return "toolkit" in self.schema_groups

    @property
    def in_browser(self):
        """Whether the API entry is defined by browser schemas."""
        return "browser" in self.schema_groups

    @property
    def in_mobile(self):
        """Whether the API entry is defined by mobile schemas."""
        return "mobile" in self.schema_groups

    @property
    def is_mv2_only(self):
        # Each API entry should not have multiple max_manifest_version property
        # conflicting with each other (even if there is schema data coming from multiple
        # JSONSchema files, eg. when a base toolkit schema definition is extended by additional
        # schema data on Desktop or Mobile), and so here we just iterate over all the schema
        # data related to this entry and look for the first max_manifest_version property value
        # we can find if any.
        for entry in self.schema_data_list:
            if "max_manifest_version" in entry and entry["max_manifest_version"] < 3:
                return True
        return False

    def dump_platform_diff(self, diff_cmd, only_if_webidl_differ):
        """
        Dump a diff of the JSON schema data coming from browser and mobile,
        if the API did have schema data loaded from both these group of schema files.
        """
        if len(self.schema_groups) <= 1:
            return

        # We don't expect any schema data from "toolkit" that we expect to also have
        # duplicated (and potentially different) schema data in the other groups
        # of schema data ("browser" and "mobile).
        #
        # For the API that are shared but slightly different in the Desktop and Android
        # builds we expect the schema data to only be placed in the related group of schema
        # ("browser" and "mobile").
        #
        # We throw a TypeError here to detect if that assumption is violated while we are
        # collecting the platform diffs, while keeping the logic for the generated diff
        # below simple with the guarantee that we wouldn't get to it if that assumption
        # is violated.
        if "toolkit" in self.schema_groups:
            raise TypeError(
                "Unexpected diff between toolkit and browser/mobile schema: %s"
                % self.api_path_string
            )

        # Compare the webidl signature generated for mobile vs desktop,
        # generate different signature surrounded by macro if they differ
        # or only include one if the generated webidl signature would still
        # be the same.
        browser_schema_data = self.schema_data_by_group["browser"][0]
        mobile_schema_data = self.schema_data_by_group["mobile"][0]

        if only_if_webidl_differ:
            browser_webidl = WebIDLHelpers.to_webidl_definition(self, "browser")
            mobile_webidl = WebIDLHelpers.to_webidl_definition(self, "mobile")

            if browser_webidl == mobile_webidl:
                return

        json_diff = run_diff(
            diff_cmd,
            "%s-browser" % self.api_path_string,
            json.dumps(browser_schema_data, indent=True),
            "%s-mobile" % self.api_path_string,
            json.dumps(mobile_schema_data, indent=True),
        )

        if len(json_diff.strip()) == 0:
            return

        # Print a diff of the browser vs. mobile JSON schema.
        print("\n\n## API schema desktop vs. mobile for %s\n\n" % self.api_path_string)
        print("```\n%s\n```" % json_diff)

    def get_schema_data(self, schema_group=None):
        """
        Get schema data loaded for this entry (optionally from a specific group
        of schema files).
        """
        if schema_group is None:
            return self.schema_data_list[0]
        return self.schema_data_by_group[schema_group][0]

    @property
    def api_path_string(self):
        """Convert the path list into the full namespace string."""
        return ".".join(self.path)


class APIType(APIEntry):
    """Class to represent an API type"""


class APIProperty(APIEntry):
    """Class to represent an API property"""


class APIEvent(APIEntry):
    """Class to represent an API Event"""


class APIFunction(APIEntry):
    """Class to represent an API function"""

    def is_async(self, schema_group=None):
        """
        Returns True is the APIFunction is marked as asynchronous in its schema data.
        """
        schema_data = self.get_schema_data(schema_group)
        return "async" in schema_data

    def is_optional_param(self, param):
        return "optional" in param and param["optional"]

    def is_callback_param(self, param, schema_group=None):
        return self.is_async(schema_group) and (
            param["name"] == self.get_async_callback_name(schema_group)
        )

    def iter_multiple_webidl_signatures_params(self, schema_group=None):
        """
        Lazily generate the parameters set to use in the multiple webidl definitions
        that should be generated by this method, due to a set of optional parameters
        followed by a mandatory one.

        NOTE: the caller SHOULD NOT mutate (or save for later use) the list of parameters
        yielded by this generator function (because the parameters list and parameters
        are not deep cloned and reused internally between yielded values).
        """
        schema_data = self.get_schema_data(schema_group)
        parameters = schema_data["parameters"].copy()
        yield parameters

        if not self.has_multiple_webidl_signatures(schema_group):
            return

        def get_next_idx(p):
            return parameters.index(p) + 1

        def get_next_rest(p):
            return parameters[get_next_idx(p) : :]

        def is_optional(p):
            return self.is_optional_param(p)

        def is_mandatory(p):
            return not is_optional(p)

        rest = parameters
        while not all(is_mandatory(param) for param in rest):
            param = next(filter(is_optional, rest))
            rest = get_next_rest(param)
            if self.is_callback_param(param, schema_group):
                return

            parameters.remove(param)
            yield parameters

    def has_ambiguous_stub_mapping(self, schema_group):
        # Determine if the API should be using the AsyncAmbiguous
        # stub method per its JSONSchema data.
        schema_data = self.get_schema_data(schema_group)
        is_ambiguous = False
        if "allowAmbiguousOptionalArguments" in schema_data:
            is_ambiguous = True

        if not is_ambiguous:
            # Determine if the API should be using the AsyncAmbiguous
            # stub method per configuration set from ExtensionWebIDL.conf.
            api_path = ".".join([*self.path])
            if api_path in WEBEXT_STUBS_MAPPING:
                return WEBEXT_STUBS_MAPPING[api_path] == "AsyncAmbiguous"

        return is_ambiguous

    def has_multiple_webidl_signatures(self, schema_group=None):
        """
        Determine if the API method in the JSONSchema needs to be turned in
        multiple function signatures in the WebIDL definitions (e.g. `alarms.create`,
        needs two separate WebIDL definitions accepting 1 and 2 parameters to match the
        expected behaviors).
        """

        if self.has_ambiguous_stub_mapping(schema_group):
            # The few methods that are marked as ambiguous (only runtime.sendMessage,
            # besides the ones in the `test` API) are currently generated as
            # a single webidl method with a variadic parameter.
            return False

        schema_data = self.get_schema_data(schema_group)
        params = schema_data["parameters"] or []

        return not all(not self.is_optional_param(param) for param in params)

    def get_async_callback_name(self, schema_group):
        """
        Get the async callback name, or raise a TypeError if inconsistencies are detected
        in the schema data related to the expected callback parameter.
        """
        # For an async method we expect the "async" keyword to be either
        # set to `true` or to a callback name, in which case we expect
        # to have a callback parameter with the same name as the last
        # of the function schema parameters:
        schema_data = self.get_schema_data(schema_group)
        if "async" not in schema_data or schema_data["async"] is False:
            raise TypeError("%s schema is not an async function" % self.api_path_string)

        if isinstance(schema_data["async"], str):
            cb_name = schema_data["async"]
            if "parameters" not in schema_data or not schema_data["parameters"]:
                raise TypeError(
                    "%s is missing a parameter definition for async callback %s"
                    % (self.api_path_string, cb_name)
                )

            last_param = schema_data["parameters"][-1]
            if last_param["name"] != cb_name or last_param["type"] != "function":
                raise TypeError(
                    "%s is missing a parameter definition for async callback %s"
                    % (self.api_path_string, cb_name)
                )
            return cb_name

        # default callback name on `"async": true` in the schema data.
        return "callback"


class APINamespace:
    """Class to represent an API namespace"""

    def __init__(self, root, name, ns_path):
        self.root = root
        self.name = name
        if name:
            self.path = [*ns_path, name]
        else:
            self.path = [*ns_path]

        # All the schema data collected for this namespace across all the
        # json schema files loaded, grouped by the schem_group they are being
        # loaded from ('toolkit', 'desktop', mobile').
        self.schema_data_by_group = DefaultDict(lambda _: [])

        # class properties populated by parse_schemas.

        self.max_manifest_version = None
        self.permissions = set()
        self.allowed_contexts = set()
        self.default_contexts = set()

        self.types = DefaultDict(lambda type_id: APIType(self, type_id, self.path))
        self.properties = DefaultDict(
            lambda prop_id: APIProperty(self, prop_id, self.path)
        )
        self.functions = DefaultDict(
            lambda fn_name: APIFunction(self, fn_name, self.path)
        )
        self.events = DefaultDict(
            lambda event_name: APIEvent(self, event_name, self.path)
        )

    def get_allowed_contexts(self):
        """
        Return the allowed contexts for this API namespace
        """
        return self.allowed_contexts

    @property
    def schema_groups(self):
        """List of the schema groups that have schema data for this entry."""
        return [*self.schema_data_by_group.keys()]

    @property
    def in_toolkit(self):
        """Whether the API entry is defined by toolkit schemas."""
        return "toolkit" in self.schema_groups

    @property
    def in_browser(self):
        """Whether the API entry is defined by browser schemas."""
        return "browser" in self.schema_groups

    @property
    def in_mobile(self):
        """Whether the API entry is defined by mobile schemas."""
        if self.name in WEBEXT_ANDROID_EXCLUDED:
            return False
        return "mobile" in self.schema_groups

    @property
    def is_mv2_only(self):
        return self.max_manifest_version == 2

    @property
    def api_path_string(self):
        """Convert the path list into the full namespace string."""
        return ".".join(self.path)

    def add_schema(self, schema_data, schema_group):
        """Add schema data loaded from a specific group of schema files."""
        self.schema_data_by_group.getOrCreate(schema_group).append(schema_data)

    def parse_schemas(self):
        """Parse all the schema data collected (from all schema groups)."""
        for schema_group, schema_data in self.schema_data_by_group.items():
            self._parse_schema_data(schema_data, schema_group)

    def _parse_schema_data(self, schema_data, schema_group):
        for data in schema_data:
            # TODO: we should actually don't merge together permissions and
            # allowedContext/defaultContext, because in some cases the schema files
            # are split in two when only part of the API is available to the
            # content scripts.

            # load permissions, allowed_contexts and default_contexts
            if "permissions" in data:
                self.permissions.update(data["permissions"])
            if "allowedContexts" in data:
                self.allowed_contexts.update(data["allowedContexts"])
            if "defaultContexts" in data:
                self.default_contexts.update(data["defaultContexts"])
            if "max_manifest_version" in data:
                if (
                    self.max_manifest_version is not None
                    and self.max_manifest_version != data["max_manifest_version"]
                ):
                    raise TypeError(
                        "Error loading schema data - overwriting existing max_manifest_version"
                        " value\n\tPrevious max_manifest_version set: %s\n\tschema_group: %s"
                        "\n\tschema_data: %s"
                        % (self.max_manifest_version, schema_group, schema_data)
                    )
                self.max_manifest_version = data["max_manifest_version"]

            api_path = self.api_path_string

            # load types
            if "types" in data:
                for type_data in data["types"]:
                    type_id = None
                    if "id" in type_data:
                        type_id = type_data["id"]
                    elif "$extend" in type_data:
                        type_id = type_data["$extend"]
                    elif "unsupported" in type_data:
                        # No need to raise an error for an unsupported type
                        # it will ignored below before adding it to the map
                        # of the namespace types.
                        pass
                    else:
                        # Supported entries without an "id" or "$extend"
                        # property are unexpected, log a warning and
                        # fail explicitly if that happens to be the case.
                        logging.critical(
                            "Error loading schema data type from '%s %s': %s",
                            schema_group,
                            api_path,
                            json.dumps(type_data, indent=True),
                        )
                        raise TypeError(
                            "Error loading schema type data defined in '%s %s'"
                            % (schema_group, api_path),
                        )

                    if "unsupported" in type_data:
                        # Skip unsupported type.
                        logging.debug(
                            "Skipping unsupported type '%s'",
                            "%s %s.%s" % (schema_group, api_path, type_id),
                        )
                        continue

                    assert type_id
                    type_entry = self.types.getOrCreate(type_id)
                    type_entry.add_schema(type_data, schema_group)

            # load properties
            if "properties" in data:
                for prop_id, prop_data in data["properties"].items():
                    # Skip unsupported type.
                    if "unsupported" in prop_data:
                        logging.debug(
                            "Skipping unsupported property '%s'",
                            "%s %s.%s" % (schema_group, api_path, prop_id),
                        )
                        continue
                    prop_entry = self.properties.getOrCreate(prop_id)
                    prop_entry.add_schema(prop_data, schema_group)

            # load functions
            if "functions" in data:
                for func_data in data["functions"]:
                    func_name = func_data["name"]
                    # Skip unsupported function.
                    if "unsupported" in func_data:
                        logging.debug(
                            "Skipping unsupported function '%s'",
                            "%s %s.%s" % (schema_group, api_path, func_name),
                        )
                        continue
                    func_entry = self.functions.getOrCreate(func_name)
                    func_entry.add_schema(func_data, schema_group)

            # load events
            if "events" in data:
                for event_data in data["events"]:
                    event_name = event_data["name"]
                    # Skip unsupported function.
                    if "unsupported" in event_data:
                        logging.debug(
                            "Skipping unsupported event: '%s'",
                            "%s %s.%s" % (schema_group, api_path, event_name),
                        )
                        continue
                    event_entry = self.events.getOrCreate(event_name)
                    event_entry.add_schema(event_data, schema_group)

    def get_child_namespace_names(self):
        """Returns the list of child namespaces for the current namespace"""

        # some API namespaces may contains other namespaces
        # e.g. 'devtools' does contain 'devtools.inspectedWindow',
        # 'devtools.panels' etc.
        return [
            ns
            for ns in self.root.get_all_namespace_names()
            if ns.startswith(self.name + ".")
        ]

    def get_child_namespaces(self):
        """Returns all the APINamespace instances for the child namespaces"""
        return [
            self.root.get_namespace(name) for name in self.get_child_namespace_names()
        ]

    def get_boilerplate_cpp_header(self):
        template = self.root.jinja_env.get_template("ExtensionAPI.h.in")
        webidl_props = WebIDLHelpers.to_template_props(self)
        return template.render(
            {"webidl_name": webidl_props["webidl_name"], "api_namespace": self.name}
        )

    def get_boilerplate_cpp(self):
        template = self.root.jinja_env.get_template("ExtensionAPI.cpp.in")
        webidl_props = WebIDLHelpers.to_template_props(self)
        return template.render(
            {"webidl_name": webidl_props["webidl_name"], "api_namespace": self.name}
        )

    def dump(self, schema_group=None):
        """
        Used by the --dump-namespaces-info flag to dump some info
        for a given namespace based on all the schema files loaded.
        """

        def get_entry_names_by_group(values):
            res = {"both": [], "mobile": [], "browser": []}
            for item in values:
                if item.in_toolkit or (item.in_browser and item.in_mobile):
                    res["both"].append(item.name)
                elif item.in_browser and not item.in_mobile:
                    res["browser"].append(item.name)
                elif item.in_mobile and not item.in_desktop:
                    res["mobile"].append(item.name)
            return res

        def dump_names_by_group(values):
            entries_map = get_entry_names_by_group(values)
            print("  both: %s" % entries_map["both"])
            print("  only on desktop: %s" % entries_map["browser"])
            print("  only on mobile: %s" % entries_map["mobile"])

        if schema_group is not None and [schema_group] != self.schema_groups:
            return

        print("\n## %s\n" % self.name)

        print("schema groups: ", self.schema_groups)
        print("max manifest version: ", self.max_manifest_version)
        print("permissions: ", self.permissions)
        print("allowed contexts: ", self.allowed_contexts)
        print("default contexts: ", self.default_contexts)

        print("functions:")
        dump_names_by_group(self.functions.values())
        fn_multi_signatures = list(
            filter(
                lambda fn: fn.has_multiple_webidl_signatures(), self.functions.values()
            )
        )
        if len(fn_multi_signatures) > 0:
            print("functions with multiple WebIDL type signatures:")
            for fn in fn_multi_signatures:
                print("  -", fn.name)
                for params in fn.iter_multiple_webidl_signatures_params():
                    print("    -", params)

        print("events:")
        dump_names_by_group(self.events.values())
        print("properties:")
        dump_names_by_group(self.properties.values())
        print("types:")
        dump_names_by_group(self.types.values())

        print("child namespaces:")
        dump_names_by_group(self.get_child_namespaces())


class Schemas:
    """Helper class used to load and parse all the schema files"""

    def __init__(self):
        self.json_schemas = dict()
        self.api_namespaces = DefaultDict(lambda name: APINamespace(self, name, []))
        self.jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(BASE_DIR),
        )

    def load_schemas(self, schema_dir_path, schema_group):
        """
        Helper function used to read all WebExtensions API schema JSON files
        from a given directory.
        """
        for file_name in os.listdir(schema_dir_path):
            if file_name.endswith(".json"):
                full_path = os.path.join(schema_dir_path, file_name)
                rel_path = os.path.relpath(full_path, buildconfig.topsrcdir)

                logging.debug("Loading schema file %s", rel_path)

                schema_data = read_json(full_path)
                self.json_schemas[full_path] = schema_data

                for schema_data_entry in schema_data:
                    name = schema_data_entry["namespace"]
                    # Validate the schema while loading them.
                    WebExtAPIValidator.check_schema(schema_data_entry)

                    api_ns = self.api_namespaces.getOrCreate(name)
                    api_ns.add_schema(schema_data_entry, schema_group)
                    self.api_namespaces[name] = api_ns

    def get_all_namespace_names(self):
        """
        Return an array of all namespace names
        """
        return [*self.api_namespaces.keys()]

    def parse_schemas(self):
        """
        Helper function used to parse all the collected API schemas.
        """
        for api_ns in self.api_namespaces.values():
            api_ns.parse_schemas()

    def get_namespace(self, name):
        """
        Return a APINamespace instance for the given api name.
        """
        return self.api_namespaces[name]

    def dump_namespaces(self):
        """
        Dump all namespaces collected to stdout.
        """
        print(self.get_all_namespace_names())

    def dump(self):
        """
        Dump all collected schema to stdout.
        """
        print(json.dumps(self.json_schemas, indent=True))


def parse_command_and_args():
    parser = argparse.ArgumentParser()

    # global cli flags shared by all sub-commands.
    parser.add_argument("--verbose", "-v", action="count", default=0)
    parser.add_argument(
        "--diff-command",
        type=str,
        metavar="DIFFCMD",
        help="select the diff command used to generate diffs (defaults to 'diff')",
    )

    parser.add_argument(
        "--generate-cpp-boilerplate",
        action="store_true",
        help="'generate' command flag to be used to generate cpp boilerplate"
        + " for the given NAMESPACE",
    )
    parser.add_argument(
        "--overwrite-existing",
        action="store_true",
        help="'generate' command flag to be used to allow the script to"
        + " overwrite existing files (API webidl and cpp boilerplate files)",
    )

    parser.add_argument(
        "api_namespaces",
        type=str,
        metavar="NAMESPACE",
        nargs="+",
        help="WebExtensions API namespaces to generate webidl and cpp boilerplates for",
    )

    return parser.parse_args()


def load_and_parse_JSONSchema():
    """Load and parse all JSONSchema data"""

    # Initialize Schemas and load all the JSON schema from the directories
    # listed in WEBEXT_SCHEMADIRS_MAPPING.
    schemas = Schemas()
    for schema_group, schema_dir_components in WEBEXT_SCHEMADIRS_MAPPING.items():
        schema_dir = mozpath.join(buildconfig.topsrcdir, *schema_dir_components)
        schemas.load_schemas(schema_dir, schema_group)

    # Parse all the schema loaded (which also run some validation based on the
    # expectations of the code that generates the webidl definitions).
    schemas.parse_schemas()

    return schemas


# Run the 'generate' subcommand which does:
#
# - generates the webidl file for the new API
# - generate boilerplate for the C++ files that implements the new webidl definition
# - provides details about the rest of steps needed to fully wire up the WebExtensions API
#   in the `browser` and `chrome` globals defined through WebIDL.
#
# This command is the entry point for the main feature provided by this scripts.
def run_generate_command(args, schemas):
    show_next_steps = False

    for api_ns_str in args.api_namespaces:
        webidl_name = WebIDLHelpers.to_webidl_definition_name(api_ns_str)

        # Generate webidl definition.
        webidl_relpath = mozpath.join(WEBIDL_DIR, "%s.webidl" % webidl_name)
        webidl_abspath = mozpath.join(WEBIDL_DIR_FULLPATH, "%s.webidl" % webidl_name)
        print(
            "\nGenerating webidl definition for '%s' => %s"
            % (api_ns_str, webidl_relpath)
        )
        api_ns = schemas.get_namespace(api_ns_str)

        did_wrote_webidl_changes = write_with_overwrite_confirm(
            relpath=webidl_relpath,
            abspath=webidl_abspath,
            newcontent=WebIDLHelpers.to_webidl_definition(api_ns, None),
            diff_prefix="%s.webidl" % webidl_name,
            diff_command=args.diff_command,
            overwrite_existing=args.overwrite_existing,
        )

        if did_wrote_webidl_changes:
            show_next_steps = True

        cpp_abspath = mozpath.join(CPP_DIR_FULLPATH, "%s.cpp" % webidl_name)
        cpp_header_abspath = mozpath.join(CPP_DIR_FULLPATH, "%s.h" % webidl_name)

        cpp_files_exist = os.path.exists(cpp_abspath) and os.path.exists(
            cpp_header_abspath
        )

        # Generate c++ boilerplate files if forced by the cli flag or
        # if the cpp files do not exist yet.
        if args.generate_cpp_boilerplate or not cpp_files_exist:
            print(
                "\nGenerating C++ boilerplate for '%s' => %s.h/.cpp"
                % (api_ns_str, webidl_name)
            )

            cpp_relpath = mozpath.join(CPP_DIR, "%s.cpp" % webidl_name)
            cpp_header_relpath = mozpath.join(CPP_DIR, "%s.h" % webidl_name)

            write_with_overwrite_confirm(
                relpath=cpp_header_relpath,
                abspath=cpp_header_abspath,
                newcontent=api_ns.get_boilerplate_cpp_header(),
                diff_prefix="%s.h" % webidl_name,
                diff_command=args.diff_command,
                overwrite_existing=False,
            )
            write_with_overwrite_confirm(
                relpath=cpp_relpath,
                abspath=cpp_abspath,
                newcontent=api_ns.get_boilerplate_cpp(),
                diff_prefix="%s.cpp" % webidl_name,
                diff_command=args.diff_command,
                overwrite_existing=False,
            )

    if show_next_steps:
        separator = "-" * 20
        print(
            "\n%s\n\n"
            "NEXT STEPS\n"
            "==========\n\n"
            "It is not done yet!!!\n"
            "%s" % (separator, DOCS_NEXT_STEPS)
        )


def set_logging_level(verbose):
    """Set the logging level (defaults to WARNING), and increased to
    INFO or DEBUG based on the verbose counter flag value"""
    # Increase logging level based on the args.verbose counter flag value.
    # (Default logging level should include warnings).
    if verbose == 0:
        logging_level = "WARNING"
    elif verbose >= 2:
        logging_level = "DEBUG"
    else:
        logging_level = "INFO"
    logging.getLogger().setLevel(logging_level)
    logging.info("Logging level set to %s", logging_level)


def main():
    """Entry point function for this script"""

    args = parse_command_and_args()
    set_logging_level(args.verbose)
    schemas = load_and_parse_JSONSchema()
    run_generate_command(args, schemas)


if __name__ == "__main__":
    main()
