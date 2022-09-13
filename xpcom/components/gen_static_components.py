import json
import os
import re
import struct
from collections import defaultdict

from uuid import UUID

from mozbuild.util import FileAvoidWrite
from perfecthash import PerfectHash
import buildconfig


NO_CONTRACT_ID = 0xFFFFFFFF

PHF_SIZE = 512

# In tests, we might not have a (complete) buildconfig.
ENDIAN = (
    "<" if buildconfig.substs.get("TARGET_ENDIANNESS", "little") == "little" else ">"
)


# Represents a UUID in the format used internally by Gecko, and supports
# serializing it in that format to both C++ source and raw byte arrays.
class UUIDRepr(object):
    def __init__(self, uuid):
        self.uuid = uuid

        fields = uuid.fields

        self.a = fields[0]
        self.b = fields[1]
        self.c = fields[2]

        d = list(fields[3:5])
        for i in range(0, 6):
            d.append(fields[5] >> (8 * (5 - i)) & 0xFF)

        self.d = tuple(d)

    def __str__(self):
        return str(self.uuid)

    @property
    def bytes(self):
        return struct.pack(ENDIAN + "IHHBBBBBBBB", self.a, self.b, self.c, *self.d)

    def to_cxx(self):
        rest = ", ".join("0x%02x" % b for b in self.d)

        return "{ 0x%x, 0x%x, 0x%x, { %s } }" % (self.a, self.b, self.c, rest)


# Corresponds to the Module::ProcessSelector enum in Module.h. The actual
# values don't matter, since the code generator emits symbolic constants for
# these values, but we use the same values as the enum constants for clarity.
class ProcessSelector:
    ANY_PROCESS = 0x0
    MAIN_PROCESS_ONLY = 0x1
    CONTENT_PROCESS_ONLY = 0x2
    ALLOW_IN_GPU_PROCESS = 0x4
    ALLOW_IN_VR_PROCESS = 0x8
    ALLOW_IN_SOCKET_PROCESS = 0x10
    ALLOW_IN_RDD_PROCESS = 0x20
    ALLOW_IN_UTILITY_PROCESS = 0x30
    ALLOW_IN_GPU_AND_MAIN_PROCESS = ALLOW_IN_GPU_PROCESS | MAIN_PROCESS_ONLY
    ALLOW_IN_GPU_AND_SOCKET_PROCESS = ALLOW_IN_GPU_PROCESS | ALLOW_IN_SOCKET_PROCESS
    ALLOW_IN_GPU_AND_VR_PROCESS = ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS
    ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS = (
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS | ALLOW_IN_SOCKET_PROCESS
    )
    ALLOW_IN_RDD_AND_SOCKET_PROCESS = ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS
    ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS = (
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS
    )
    ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS = (
        ALLOW_IN_GPU_PROCESS
        | ALLOW_IN_RDD_PROCESS
        | ALLOW_IN_SOCKET_PROCESS
        | ALLOW_IN_UTILITY_PROCESS
    )
    ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS = (
        ALLOW_IN_GPU_PROCESS
        | ALLOW_IN_RDD_PROCESS
        | ALLOW_IN_VR_PROCESS
        | ALLOW_IN_SOCKET_PROCESS
    )
    ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS = (
        ALLOW_IN_GPU_PROCESS
        | ALLOW_IN_RDD_PROCESS
        | ALLOW_IN_VR_PROCESS
        | ALLOW_IN_SOCKET_PROCESS
        | ALLOW_IN_UTILITY_PROCESS
    )


# Maps ProcessSelector constants to the name of the corresponding
# Module::ProcessSelector enum value.
PROCESSES = {
    ProcessSelector.ANY_PROCESS: "ANY_PROCESS",
    ProcessSelector.MAIN_PROCESS_ONLY: "MAIN_PROCESS_ONLY",
    ProcessSelector.CONTENT_PROCESS_ONLY: "CONTENT_PROCESS_ONLY",
    ProcessSelector.ALLOW_IN_GPU_PROCESS: "ALLOW_IN_GPU_PROCESS",
    ProcessSelector.ALLOW_IN_VR_PROCESS: "ALLOW_IN_VR_PROCESS",
    ProcessSelector.ALLOW_IN_SOCKET_PROCESS: "ALLOW_IN_SOCKET_PROCESS",
    ProcessSelector.ALLOW_IN_RDD_PROCESS: "ALLOW_IN_RDD_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_AND_MAIN_PROCESS: "ALLOW_IN_GPU_AND_MAIN_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_AND_SOCKET_PROCESS: "ALLOW_IN_GPU_AND_SOCKET_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_AND_VR_PROCESS: "ALLOW_IN_GPU_AND_VR_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS: "ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS",
    ProcessSelector.ALLOW_IN_RDD_AND_SOCKET_PROCESS: "ALLOW_IN_RDD_AND_SOCKET_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS: "ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS",
    ProcessSelector.ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS: "ALLOW_IN_GPU_RDD_SOCKET_AND_UTILITY_PROCESS",  # NOQA: E501
    ProcessSelector.ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS: "ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS",  # NOQA: E501
    ProcessSelector.ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS: "ALLOW_IN_GPU_RDD_VR_SOCKET_AND_UTILITY_PROCESS",  # NOQA: E501
}


# Emits the C++ symbolic constant corresponding to a ProcessSelector constant.
def lower_processes(processes):
    return "Module::ProcessSelector::%s" % PROCESSES[processes]


# Emits the C++ symbolic constant for a ModuleEntry's ModuleID enum entry.
def lower_module_id(module):
    return "ModuleID::%s" % module.name


# Corresponds to the Module::BackgroundTasksSelector enum in Module.h. The
# actual values don't matter, since the code generator emits symbolic constants
# for these values, but we use the same values as the enum constants for
# clarity.
class BackgroundTasksSelector:
    NO_TASKS = 0x0
    ALL_TASKS = 0xFFFF


# Maps BackgroundTasksSelector constants to the name of the corresponding
# Module::BackgroundTasksSelector enum value.
BACKGROUNDTASKS = {
    BackgroundTasksSelector.ALL_TASKS: "ALL_TASKS",
    BackgroundTasksSelector.NO_TASKS: "NO_TASKS",
}


# Emits the C++ symbolic constant corresponding to a BackgroundTasks constant.
def lower_backgroundtasks(backgroundtasks):
    return "Module::BackgroundTasksSelector::%s" % BACKGROUNDTASKS[backgroundtasks]


# Represents a static string table, indexed by offset. This allows us to
# reference strings from static data structures without requiring runtime
# relocations.
class StringTable(object):
    def __init__(self):
        self.entries = {}
        self.entry_list = []
        self.size = 0

        self._serialized = False

    # Returns the index of the given string in the `entry_list` array. If
    # no entry for the string exists, it first creates one.
    def get_idx(self, string):
        idx = self.entries.get(string, None)
        if idx is not None:
            return idx

        assert not self._serialized

        assert len(string) == len(string.encode("utf-8"))

        idx = self.size
        self.size += len(string) + 1

        self.entries[string] = idx
        self.entry_list.append(string)
        return idx

    # Returns the C++ code representing string data of this string table, as a
    # single string literal. This must only be called after the last call to
    # `get_idx()` or `entry_to_cxx()` for this instance.
    def to_cxx(self):
        self._serialized = True

        lines = []

        idx = 0
        for entry in self.entry_list:
            str_ = entry.replace("\\", "\\\\").replace('"', r"\"").replace("\n", r"\n")

            lines.append('    /* 0x%x */ "%s\\0"\n' % (idx, str_))

            idx += len(entry) + 1

        return "".join(lines)

    # Returns a `StringEntry` struct initializer for the string table entry
    # corresponding to the given string. If no matching entry exists, it is
    # first created.
    def entry_to_cxx(self, string):
        idx = self.get_idx(string)
        return "{ 0x%x } /* %s */" % (idx, pretty_string(string))


strings = StringTable()

interfaces = []


# Represents a C++ namespace, containing a set of classes and potentially
# sub-namespaces. This is used to generate pre-declarations for incomplete
# types referenced in XPCOM manifests.
class Namespace(object):
    def __init__(self, name=None):
        self.name = name
        self.classes = set()
        self.namespaces = {}

    # Returns a Namespace object for the sub-namespace with the given name.
    def sub(self, name):
        assert name not in self.classes

        if name not in self.namespaces:
            self.namespaces[name] = Namespace(name)
        return self.namespaces[name]

    # Generates C++ code to pre-declare all classes in this namespace and all
    # of its sub-namespaces.
    def to_cxx(self):
        res = ""
        if self.name:
            res += "namespace %s {\n" % self.name

        for clas in sorted(self.classes):
            res += "class %s;\n" % clas

        for ns in sorted(self.namespaces.keys()):
            res += self.namespaces[ns].to_cxx()

        if self.name:
            res += "}  // namespace %s\n" % self.name

        return res


# Represents a component defined in an XPCOM manifest's `Classes` array.
class ModuleEntry(object):
    next_anon_id = 0

    def __init__(self, data, init_idx):
        self.cid = UUIDRepr(UUID(data["cid"]))
        self.contract_ids = data.get("contract_ids", [])
        self.type = data.get("type", "nsISupports")
        self.categories = data.get("categories", {})
        self.processes = data.get("processes", 0)
        self.headers = data.get("headers", [])

        self.js_name = data.get("js_name", None)
        self.interfaces = data.get("interfaces", [])

        if len(self.interfaces) > 255:
            raise Exception(
                "JS service %s may not have more than 255 " "interfaces" % self.js_name
            )

        self.interfaces_offset = len(interfaces)
        for iface in self.interfaces:
            interfaces.append(iface)

        # If the manifest declares Init or Unload functions, this contains its
        # index, as understood by the `CallInitFunc()` function.
        #
        # If it contains any value other than `None`, a corresponding
        # `CallInitFunc(init_idx)` call will be genrated before calling this
        # module's constructor.
        self.init_idx = init_idx

        self.constructor = data.get("constructor", None)
        self.legacy_constructor = data.get("legacy_constructor", None)
        self.init_method = data.get("init_method", [])

        self.jsm = data.get("jsm", None)
        self.esModule = data.get("esModule", None)

        self.external = data.get(
            "external", not (self.headers or self.legacy_constructor)
        )
        self.singleton = data.get("singleton", False)
        self.overridable = data.get("overridable", False)

        if "name" in data:
            self.anonymous = False
            self.name = data["name"]
        else:
            self.anonymous = True
            self.name = "Anonymous%03d" % ModuleEntry.next_anon_id
            ModuleEntry.next_anon_id += 1

        def error(str_):
            raise Exception(
                "Error defining component %s (%s): %s"
                % (str(self.cid), ", ".join(map(repr, self.contract_ids)), str_)
            )

        if self.jsm:
            if not self.constructor:
                error("JavaScript components must specify a constructor")

            for prop in ("init_method", "legacy_constructor", "headers"):
                if getattr(self, prop):
                    error(
                        "JavaScript components may not specify a '%s' "
                        "property" % prop
                    )
        elif self.esModule:
            if not self.constructor:
                error("JavaScript components must specify a constructor")

            for prop in ("init_method", "legacy_constructor", "headers"):
                if getattr(self, prop):
                    error(
                        "JavaScript components may not specify a '%s' "
                        "property" % prop
                    )
        elif self.external:
            if self.constructor or self.legacy_constructor:
                error(
                    "Externally-constructed components may not specify "
                    "'constructor' or 'legacy_constructor' properties"
                )
            if self.init_method:
                error(
                    "Externally-constructed components may not specify "
                    "'init_method' properties"
                )
            if self.type == "nsISupports":
                error(
                    "Externally-constructed components must specify a type "
                    "other than nsISupports"
                )

        if self.constructor and self.legacy_constructor:
            error(
                "The 'constructor' and 'legacy_constructor' properties "
                "are mutually exclusive"
            )

        if self.overridable and not self.contract_ids:
            error("Overridable components must specify at least one contract " "ID")

    @property
    def contract_id(self):
        return self.contract_ids[0]

    # Generates the C++ code for a StaticModule struct initializer
    # representing this component.
    def to_cxx(self):
        contract_id = (
            strings.entry_to_cxx(self.contract_id)
            if self.overridable
            else "{ 0x%x }" % NO_CONTRACT_ID
        )

        return """
        /* {name} */ {{
          /* {{{cid_string}}} */
          {cid},
          {contract_id},
          {processes},
        }}""".format(
            name=self.name,
            cid=self.cid.to_cxx(),
            cid_string=str(self.cid),
            contract_id=contract_id,
            processes=lower_processes(self.processes),
        )

    # Generates the C++ code for a JSServiceEntry representing this module.
    def lower_js_service(self):
        return """
        {{
          {js_name},
          ModuleID::{name},
          {{ {iface_offset} }},
          {iface_count}
        }}""".format(
            js_name=strings.entry_to_cxx(self.js_name),
            name=self.name,
            iface_offset=self.interfaces_offset,
            iface_count=len(self.interfaces),
        )

    # Generates the C++ code necessary to construct an instance of this
    # component.
    #
    # This code lives in a function with the following arguments:
    #
    #  - aIID: The `const nsIID&` interface ID that the resulting instance
    #          will be queried to.
    #
    #  - aResult: The `void**` pointer in which to store the result.
    #
    # And which returns an `nsresult` indicating success or failure.
    def lower_constructor(self):
        res = ""

        if self.init_idx is not None:
            res += "      MOZ_TRY(CallInitFunc(%d));\n" % self.init_idx

        if self.legacy_constructor:
            res += (
                "      return /* legacy */ %s(aIID, aResult);\n"
                % self.legacy_constructor
            )
            return res

        if self.jsm:
            res += (
                "      nsCOMPtr<nsISupports> inst;\n"
                "      MOZ_TRY(ConstructJSMComponent(nsLiteralCString(%s),\n"
                "                                    %s,\n"
                "                                    getter_AddRefs(inst)));"
                "\n" % (json.dumps(self.jsm), json.dumps(self.constructor))
            )
        elif self.esModule:
            res += (
                "      nsCOMPtr<nsISupports> inst;\n"
                "      MOZ_TRY(ConstructESModuleComponent(nsLiteralCString(%s),\n"
                "                                         %s,\n"
                "                                         getter_AddRefs(inst)));"
                "\n" % (json.dumps(self.esModule), json.dumps(self.constructor))
            )
        elif self.external:
            res += (
                "      nsCOMPtr<nsISupports> inst = "
                "mozCreateComponent<%s>();\n" % self.type
            )
            # The custom constructor may return null, so check before calling
            # any methods.
            res += "      NS_ENSURE_TRUE(inst, NS_ERROR_FAILURE);\n"
        else:
            res += "      RefPtr<%s> inst = " % self.type

            if not self.constructor:
                res += "new %s();\n" % self.type
            else:
                res += "%s();\n" % self.constructor
                # The `new` operator is infallible, so we don't need to worry
                # about it returning null, but custom constructors may, so
                # check before calling any methods.
                res += "      NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);\n"

                # Check that the constructor function returns an appropriate
                # `already_AddRefed` value for our declared type.
                res += """
      using T =
          RemoveAlreadyAddRefed<decltype(%(constructor)s())>::Type;
      static_assert(
          std::is_same_v<already_AddRefed<T>, decltype(%(constructor)s())>,
          "Singleton constructor must return already_AddRefed");
      static_assert(
          std::is_base_of<%(type)s, T>::value,
          "Singleton constructor must return correct already_AddRefed");

""" % {
                    "type": self.type,
                    "constructor": self.constructor,
                }

            if self.init_method:
                res += "      MOZ_TRY(inst->%s());\n" % self.init_method

        res += "      return inst->QueryInterface(aIID, aResult);\n"

        return res

    # Generates the C++ code for the `mozilla::components::<name>` entry
    # corresponding to this component. This may not be called for modules
    # without an explicit `name` (in which cases, `self.anonymous` will be
    # true).
    def lower_getters(self):
        assert not self.anonymous

        substs = {
            "name": self.name,
            "id": "::mozilla::xpcom::ModuleID::%s" % self.name,
        }

        res = (
            """
namespace %(name)s {
static inline const nsID& CID() {
  return ::mozilla::xpcom::Components::GetCID(%(id)s);
}

static inline ::mozilla::xpcom::GetServiceHelper Service(nsresult* aRv = nullptr) {
  return {%(id)s, aRv};
}
"""
            % substs
        )

        if not self.singleton:
            res += (
                """
static inline ::mozilla::xpcom::CreateInstanceHelper Create(nsresult* aRv = nullptr) {
  return {%(id)s, aRv};
}
"""
                % substs
            )

        res += (
            """\
}  // namespace %(name)s
"""
            % substs
        )

        return res

    # Generates the rust code for the `xpcom::components::<name>` entry
    # corresponding to this component. This may not be called for modules
    # without an explicit `name` (in which cases, `self.anonymous` will be
    # true).
    def lower_getters_rust(self):
        assert not self.anonymous

        substs = {
            "name": self.name,
            "id": "super::ModuleID::%s" % self.name,
        }

        res = (
            """
#[allow(non_snake_case)]
pub mod %(name)s {
    /// Get the singleton service instance for this component.
    pub fn service<T: crate::XpCom>() -> Result<crate::RefPtr<T>, nserror::nsresult> {
        let mut ga = crate::GetterAddrefs::<T>::new();
        let rv = unsafe { super::Gecko_GetServiceByModuleID(%(id)s, &T::IID, ga.void_ptr()) };
        if rv.failed() {
            return Err(rv);
        }
        ga.refptr().ok_or(nserror::NS_ERROR_NO_INTERFACE)
    }
"""
            % substs
        )

        if not self.singleton:
            res += (
                """
    /// Create a new instance of this component.
    pub fn create<T: crate::XpCom>() -> Result<crate::RefPtr<T>, nserror::nsresult> {
        let mut ga = crate::GetterAddrefs::<T>::new();
        let rv = unsafe { super::Gecko_CreateInstanceByModuleID(%(id)s, &T::IID, ga.void_ptr()) };
        if rv.failed() {
            return Err(rv);
        }
        ga.refptr().ok_or(nserror::NS_ERROR_NO_INTERFACE)
    }
"""
                % substs
            )

        res += """\
}
"""

        return res


# Returns a quoted string literal representing the given raw string, with
# certain special characters replaced so that it can be used in a C++-style
# (/* ... */) comment.
def pretty_string(string):
    return json.dumps(string).replace("*/", r"*\/").replace("/*", r"/\*")


# Represents a static contract ID entry, corresponding to a C++ ContractEntry
# struct, mapping a contract ID to a static module entry.
class ContractEntry(object):
    def __init__(self, contract, module):
        self.contract = contract
        self.module = module

    def to_cxx(self):
        return """
        {{
          {contract},
          {module_id},
        }}""".format(
            contract=strings.entry_to_cxx(self.contract),
            module_id=lower_module_id(self.module),
        )


# Generates the C++ code for the StaticCategoryEntry and StaticCategory
# structs for all category entries declared in XPCOM manifests.
def gen_categories(substs, categories):
    cats = []
    ents = []

    count = 0
    for category, entries in sorted(categories.items()):

        def k(entry):
            return tuple(entry[0]["name"]) + entry[1:]

        entries.sort(key=k)

        cats.append(
            "  { %s,\n"
            "    %d, %d },\n" % (strings.entry_to_cxx(category), count, len(entries))
        )
        count += len(entries)

        ents.append("  /* %s */\n" % pretty_string(category))
        for entry, value, processes in entries:
            name = entry["name"]
            backgroundtasks = entry.get(
                "backgroundtasks", BackgroundTasksSelector.NO_TASKS
            )

            ents.append(
                "  { %s,\n"
                "    %s,\n"
                "    %s,\n"
                "    %s },\n"
                % (
                    strings.entry_to_cxx(name),
                    strings.entry_to_cxx(value),
                    lower_backgroundtasks(backgroundtasks),
                    lower_processes(processes),
                )
            )
        ents.append("\n")
    ents.pop()

    substs["category_count"] = len(cats)
    substs["categories"] = "".join(cats)
    substs["category_entries"] = "".join(ents)


# Generates the C++ code for all Init and Unload functions declared in XPCOM
# manifests. These form the bodies of the `CallInitFunc()` and `CallUnload`
# functions in StaticComponents.cpp.
def gen_module_funcs(substs, funcs):
    inits = []
    unloads = []

    template = """\
    case %d:
      %s
      break;
"""

    for i, (init, unload) in enumerate(funcs):
        init_code = "%s();" % init if init else "/* empty */"
        inits.append(template % (i, init_code))

        if unload:
            unloads.append(
                """\
  if (CalledInit(%d)) {
    %s();
  }
"""
                % (i, unload)
            )

    substs["init_funcs"] = "".join(inits)
    substs["unload_funcs"] = "".join(unloads)
    substs["init_count"] = len(funcs)


def gen_interfaces(ifaces):
    res = []
    for iface in ifaces:
        res.append("  nsXPTInterface::%s,\n" % iface)
    return "".join(res)


# Generates class pre-declarations for any types referenced in `Classes` array
# entries which do not have corresponding `headers` entries to fully declare
# their types.
def gen_decls(types):
    root_ns = Namespace()

    for type_ in sorted(types):
        parts = type_.split("::")

        ns = root_ns
        for part in parts[:-1]:
            ns = ns.sub(part)
        ns.classes.add(parts[-1])

    return root_ns.to_cxx()


# Generates the `switch` body for the `CreateInstanceImpl()` function, with a
# `case` for each value in ModuleID to construct an instance of the
# corresponding component.
def gen_constructors(entries):
    constructors = []
    for entry in entries:
        constructors.append(
            """\
    case {id}: {{
{constructor}\
    }}
""".format(
                id=lower_module_id(entry), constructor=entry.lower_constructor()
            )
        )

    return "".join(constructors)


# Generates the getter code for each named component entry in the
# `mozilla::components::` namespace.
def gen_getters(entries):
    entries = list(entries)
    entries.sort(key=lambda e: e.name)

    return "".join(entry.lower_getters() for entry in entries if not entry.anonymous)


# Generates the rust getter code for each named component entry in the
# `xpcom::components::` module.
def gen_getters_rust(entries):
    entries = list(entries)
    entries.sort(key=lambda e: e.name)

    return "".join(
        entry.lower_getters_rust() for entry in entries if not entry.anonymous
    )


def gen_includes(substs, all_headers):
    headers = set()
    absolute_headers = set()

    for header in all_headers:
        if header.startswith("/"):
            absolute_headers.add(header)
        else:
            headers.add(header)

    includes = ['#include "%s"' % header for header in sorted(headers)]
    substs["includes"] = "\n".join(includes) + "\n"

    relative_includes = [
        '#include "../..%s"' % header for header in sorted(absolute_headers)
    ]
    substs["relative_includes"] = "\n".join(relative_includes) + "\n"


def to_category_list(val):
    # Entries can be bare strings (like `"m-browser"`), lists of bare strings,
    # or dictionaries (like `{"name": "m-browser", "backgroundtasks":
    # BackgroundTasksSelector.ALL_TASKS}`), somewhat recursively.

    def ensure_dict(v):
        # Turn `v` into `{"name": v}` if it's not already a dict.
        if isinstance(v, dict):
            return v
        return {"name": v}

    if isinstance(val, (list, tuple)):
        return tuple(ensure_dict(v) for v in val)

    if isinstance(val, dict):
        # Explode `{"name": ["x", "y"], "backgroundtasks": ...}` into
        # `[{"name": "x", "backgroundtasks": ...}, {"name": "y", "backgroundtasks": ...}]`.
        names = val.pop("name")

        vals = []
        for entry in to_category_list(names):
            d = dict(val)
            d["name"] = entry["name"]
            vals.append(d)

        return tuple(vals)

    return (ensure_dict(val),)


def gen_substs(manifests):
    module_funcs = []

    headers = set()

    modules = []
    categories = defaultdict(list)

    for manifest in manifests:
        headers |= set(manifest.get("Headers", []))

        init_idx = None
        init = manifest.get("InitFunc")
        unload = manifest.get("UnloadFunc")
        if init or unload:
            init_idx = len(module_funcs)
            module_funcs.append((init, unload))

        for clas in manifest["Classes"]:
            modules.append(ModuleEntry(clas, init_idx))

        for category, entries in manifest.get("Categories", {}).items():
            for key, entry in entries.items():
                if isinstance(entry, tuple):
                    value, process = entry
                else:
                    value, process = entry, 0
                categories[category].append(({"name": key}, value, process))

    cids = set()
    contracts = []
    contract_map = {}
    js_services = {}

    jsms = set()
    esModules = set()

    types = set()

    for mod in modules:
        headers |= set(mod.headers)

        for contract_id in mod.contract_ids:
            if contract_id in contract_map:
                raise Exception("Duplicate contract ID: %s" % contract_id)

            entry = ContractEntry(contract_id, mod)
            contracts.append(entry)
            contract_map[contract_id] = entry

        for category, entries in mod.categories.items():
            for entry in to_category_list(entries):
                categories[category].append((entry, mod.contract_id, mod.processes))

        if mod.type and not mod.headers:
            types.add(mod.type)

        if mod.jsm:
            jsms.add(mod.jsm)

        if mod.esModule:
            esModules.add(mod.esModule)

        if mod.js_name:
            if mod.js_name in js_services:
                raise Exception("Duplicate JS service name: %s" % mod.js_name)
            js_services[mod.js_name] = mod

        if str(mod.cid) in cids:
            raise Exception("Duplicate cid: %s" % str(mod.cid))
        cids.add(str(mod.cid))

    cid_phf = PerfectHash(modules, PHF_SIZE, key=lambda module: module.cid.bytes)

    contract_phf = PerfectHash(contracts, PHF_SIZE, key=lambda entry: entry.contract)

    js_services_phf = PerfectHash(
        list(js_services.values()), PHF_SIZE, key=lambda entry: entry.js_name
    )

    js_services_json = {}
    for entry in js_services.values():
        for iface in entry.interfaces:
            js_services_json[iface] = entry.js_name

    substs = {}

    gen_categories(substs, categories)

    substs["module_ids"] = "".join("  %s,\n" % entry.name for entry in cid_phf.entries)

    substs["module_count"] = len(modules)
    substs["contract_count"] = len(contracts)

    gen_module_funcs(substs, module_funcs)

    gen_includes(substs, headers)

    substs["component_jsms"] = (
        "\n".join(" %s," % strings.entry_to_cxx(jsm) for jsm in sorted(jsms)) + "\n"
    )
    substs["component_esmodules"] = (
        "\n".join(
            " %s," % strings.entry_to_cxx(esModule) for esModule in sorted(esModules)
        )
        + "\n"
    )

    substs["interfaces"] = gen_interfaces(interfaces)

    substs["decls"] = gen_decls(types)

    substs["constructors"] = gen_constructors(cid_phf.entries)

    substs["component_getters"] = gen_getters(cid_phf.entries)

    substs["component_getters_rust"] = gen_getters_rust(cid_phf.entries)

    substs["module_cid_table"] = cid_phf.cxx_codegen(
        name="ModuleByCID",
        entry_type="StaticModule",
        entries_name="gStaticModules",
        lower_entry=lambda entry: entry.to_cxx(),
        return_type="const StaticModule*",
        return_entry=(
            "return entry.CID().Equals(aKey) && entry.Active()" " ? &entry : nullptr;"
        ),
        key_type="const nsID&",
        key_bytes="reinterpret_cast<const char*>(&aKey)",
        key_length="sizeof(nsID)",
    )

    substs["module_contract_id_table"] = contract_phf.cxx_codegen(
        name="LookupContractID",
        entry_type="ContractEntry",
        entries_name="gContractEntries",
        lower_entry=lambda entry: entry.to_cxx(),
        return_type="const ContractEntry*",
        return_entry="return entry.Matches(aKey) ? &entry : nullptr;",
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
    )

    substs["js_services_table"] = js_services_phf.cxx_codegen(
        name="LookupJSService",
        entry_type="JSServiceEntry",
        entries_name="gJSServices",
        lower_entry=lambda entry: entry.lower_js_service(),
        return_type="const JSServiceEntry*",
        return_entry="return entry.Name() == aKey ? &entry : nullptr;",
        key_type="const nsACString&",
        key_bytes="aKey.BeginReading()",
        key_length="aKey.Length()",
    )

    substs["js_services_json"] = json.dumps(js_services_json, sort_keys=True, indent=4)

    # Do this only after everything else has been emitted so we're sure the
    # string table is complete.
    substs["strings"] = strings.to_cxx()
    return substs


# Returns true if the given build config substitution is defined and truthy.
def defined(subst):
    return bool(buildconfig.substs.get(subst))


def read_manifest(filename):
    glbl = {
        "buildconfig": buildconfig,
        "defined": defined,
        "ProcessSelector": ProcessSelector,
        "BackgroundTasksSelector": BackgroundTasksSelector,
    }
    code = compile(open(filename).read(), filename, "exec")
    exec(code, glbl)
    return glbl


def main(fd, conf_file, template_file):
    def open_output(filename):
        return FileAvoidWrite(os.path.join(os.path.dirname(fd.name), filename))

    conf = json.load(open(conf_file, "r"))

    deps = set()

    manifests = []
    for filename in conf["manifests"]:
        deps.add(filename)
        manifest = read_manifest(filename)
        manifests.append(manifest)
        manifest.setdefault("Priority", 50)
        manifest["__filename__"] = filename

    manifests.sort(key=lambda man: (man["Priority"], man["__filename__"]))

    substs = gen_substs(manifests)

    def replacer(match):
        return substs[match.group(1)]

    with open_output("StaticComponents.cpp") as fh:
        with open(template_file, "r") as tfh:
            template = tfh.read()

        fh.write(re.sub(r"//# @([a-zA-Z_]+)@\n", replacer, template))

    with open_output("StaticComponentData.h") as fh:
        fh.write(
            """\
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StaticComponentData_h
#define StaticComponentData_h

#include <stddef.h>

namespace mozilla {
namespace xpcom {

static constexpr size_t kStaticModuleCount = %(module_count)d;

static constexpr size_t kContractCount = %(contract_count)d;

static constexpr size_t kStaticCategoryCount = %(category_count)d;

static constexpr size_t kModuleInitCount = %(init_count)d;

}  // namespace xpcom
}  // namespace mozilla

#endif
"""
            % substs
        )

    with open_output("components.rs") as fh:
        fh.write(
            """\
/// Unique IDs for each statically-registered module.
#[repr(u16)]
pub enum ModuleID {
%(module_ids)s
}

%(component_getters_rust)s
"""
            % substs
        )

    fd.write(
        """\
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Components_h
#define mozilla_Components_h

#include "nsCOMPtr.h"

struct nsID;

#define NS_IMPL_COMPONENT_FACTORY(iface) \\
  template <>                            \\
  already_AddRefed<nsISupports> mozCreateComponent<iface>()

template <typename T>
already_AddRefed<nsISupports> mozCreateComponent();

namespace mozilla {
namespace xpcom {

enum class ModuleID : uint16_t {
%(module_ids)s
};

// May be added as a friend function to allow constructing services via
// private constructors and init methods.
nsresult CreateInstanceImpl(ModuleID aID, const nsIID& aIID, void** aResult);

class MOZ_STACK_CLASS StaticModuleHelper : public nsCOMPtr_helper {
 public:
  StaticModuleHelper(ModuleID aId, nsresult* aErrorPtr)
      : mId(aId), mErrorPtr(aErrorPtr) {}

 protected:
  nsresult SetResult(nsresult aRv) const {
    if (mErrorPtr) {
      *mErrorPtr = aRv;
    }
    return aRv;
  }

  ModuleID mId;
  nsresult* mErrorPtr;
};

class MOZ_STACK_CLASS GetServiceHelper final : public StaticModuleHelper {
 public:
  using StaticModuleHelper::StaticModuleHelper;

  nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                  void** aResult) const override;
};

class MOZ_STACK_CLASS CreateInstanceHelper final : public StaticModuleHelper {
 public:
  using StaticModuleHelper::StaticModuleHelper;

  nsresult NS_FASTCALL operator()(const nsIID& aIID,
                                  void** aResult) const override;
};

class Components final {
 public:
  static const nsID& GetCID(ModuleID aID);
};

}  // namespace xpcom

namespace components {
%(component_getters)s
}  // namespace components

}  // namespace mozilla

#endif
"""
        % substs
    )

    with open_output("services.json") as fh:
        fh.write(substs["js_services_json"])

    return deps
