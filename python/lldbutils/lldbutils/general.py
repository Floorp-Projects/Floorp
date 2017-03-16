import lldb
from lldbutils import utils

def summarize_string(valobj, internal_dict):
    data = valobj.GetChildMemberWithName("mData")
    length = valobj.GetChildMemberWithName("mLength").GetValueAsUnsigned(0)
    return utils.format_string(data, length)

class TArraySyntheticChildrenProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.header = self.valobj.GetChildMemberWithName("mHdr")
        self.element_type = self.valobj.GetType().GetTemplateArgumentType(0)
        self.element_size = self.element_type.GetByteSize()
        header_size = self.header.GetType().GetPointeeType().GetByteSize()
        self.element_base_addr = self.header.GetValueAsUnsigned(0) + header_size

    def num_children(self):
        return self.header.Dereference().GetChildMemberWithName("mLength").GetValueAsUnsigned(0)

    def get_child_index(self, name):
        try:
            index = int(name)
            if index >= self.num_children():
                return None
        except:
            pass
        return None

    def get_child_at_index(self, index):
        if index >= self.num_children():
            return None
        addr = self.element_base_addr + index * self.element_size
        return self.valobj.CreateValueFromAddress("[%d]" % index, addr, self.element_type)

def prefcnt(debugger, command, result, dict):
    """Displays the refcount of an object."""
    # We handled regular nsISupports-like refcounted objects and cycle collected
    # objects.
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    thread = process.GetSelectedThread()
    frame = thread.GetSelectedFrame()
    obj = frame.EvaluateExpression(command)
    if obj.GetError().Fail():
        print "could not evaluate expression"
        return
    obj = utils.dereference(obj)
    field = obj.GetChildMemberWithName("mRefCnt")
    if field.GetError().Fail():
        field = obj.GetChildMemberWithName("refCnt")
    if field.GetError().Fail():
        print "not a refcounted object"
        return
    refcnt_type = field.GetType().GetCanonicalType().GetName()
    if refcnt_type == "nsAutoRefCnt":
        print field.GetChildMemberWithName("mValue").GetValueAsUnsigned(0)
    elif refcnt_type == "nsCycleCollectingAutoRefCnt":
        print field.GetChildMemberWithName("mRefCntAndFlags").GetValueAsUnsigned(0) >> 2
    elif refcnt_type == "mozilla::ThreadSafeAutoRefCnt":
        print field.GetChildMemberWithName("mValue").GetChildMemberWithName("mValue").GetValueAsUnsigned(0)
    elif refcnt_type == "int":  # non-atomic mozilla::RefCounted object
        print field.GetValueAsUnsigned(0)
    elif refcnt_type == "mozilla::Atomic<int>":  # atomic mozilla::RefCounted object
        print field.GetChildMemberWithName("mValue").GetValueAsUnsigned(0)
    else:
        print "unknown mRefCnt type " + refcnt_type

# Used to work around http://llvm.org/bugs/show_bug.cgi?id=22211
def callfunc(debugger, command, result, dict):
    """Calls a function for which debugger information is unavailable by getting its address from the symbol table.
       The function is assumed to return void."""

    if '(' not in command:
        print 'Usage: callfunc your_function(args)'
        return

    command_parts = command.split('(')
    funcname = command_parts[0].strip()
    args = command_parts[1]

    target = debugger.GetSelectedTarget()
    symbols = target.FindFunctions(funcname).symbols
    if not symbols:
        print 'Could not find a function symbol for a function called "%s"' % funcname
        return

    sym = symbols[0]
    arg_types = '()'
    if sym.name and sym.name.startswith(funcname + '('):
        arg_types = sym.name[len(funcname):]
    debugger.HandleCommand('print ((void(*)%s)0x%0x)(%s' % (arg_types, sym.addr.GetLoadAddress(target), args))

def init(debugger):
    debugger.HandleCommand("type summary add nsAString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsACString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsFixedString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsFixedCString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsAutoString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsAutoCString -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type synthetic add -x \"nsTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add -x \"AutoTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add -x \"FallibleTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("command script add -f lldbutils.general.prefcnt -f lldbutils.general.prefcnt prefcnt")
    debugger.HandleCommand("command script add -f lldbutils.general.callfunc -f lldbutils.general.callfunc callfunc")
