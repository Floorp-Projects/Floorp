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

def init(debugger):
    debugger.HandleCommand("type summary add nsAString_internal -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsACString_internal -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type synthetic add -x \"nsTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add -x \"nsAutoTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add -x \"FallibleTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add -x \"AutoFallibleTArray<\" -l lldbutils.general.TArraySyntheticChildrenProvider")
