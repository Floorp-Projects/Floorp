import lldb
from lldbutils import utils

def summarize_text_fragment(valobj, internal_dict):
    content_union = valobj.GetChildAtIndex(0)
    state_union = valobj.GetChildAtIndex(1).GetChildMemberWithName("mState")
    length = state_union.GetChildMemberWithName("mLength").GetValueAsUnsigned(0)
    if state_union.GetChildMemberWithName("mIs2b").GetValueAsUnsigned(0):
        field = "m2b"
    else:
        field = "m1b"
    ptr = content_union.GetChildMemberWithName(field)
    return utils.format_string(ptr, length)

def init(debugger):
    debugger.HandleCommand("type summary add nsTextFragment -F lldbutils.content.summarize_text_fragment")
