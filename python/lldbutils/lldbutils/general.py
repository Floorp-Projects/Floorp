import lldb
from lldbutils import utils

def summarize_string(valobj, internal_dict):
    data = valobj.GetChildMemberWithName("mData")
    length = valobj.GetChildMemberWithName("mLength").GetValueAsUnsigned(0)
    return utils.format_string(data, length)

def init(debugger):
    debugger.HandleCommand("type summary add nsAString_internal -F lldbutils.general.summarize_string")
    debugger.HandleCommand("type summary add nsACString_internal -F lldbutils.general.summarize_string")
