# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


def frametree(debugger, command, result, dict):
    """Dumps the frame tree containing the given nsIFrame*."""
    debugger.HandleCommand("expr (" + command + ")->DumpFrameTree()")


def frametree_pixels(debugger, command, result, dict):
    """Dumps the frame tree containing the given nsIFrame* in CSS pixels."""
    debugger.HandleCommand("expr (" + command + ")->DumpFrameTreeInCSSPixels()")


def frametreelimited(debugger, command, result, dict):
    """Dumps the subtree of a frame tree rooted at the given nsIFrame*."""
    debugger.HandleCommand("expr (" + command + ")->DumpFrameTreeLimited()")


def frametreelimited_pixels(debugger, command, result, dict):
    """Dumps the subtree of a frame tree rooted at the given nsIFrame*
    in CSS pixels."""
    debugger.HandleCommand("expr (" + command + ")->DumpFrameTreeLimitedInCSSPixels()")


def pstate(debugger, command, result, dict):
    """Displays a frame's state bits symbolically."""
    debugger.HandleCommand("expr mozilla::PrintFrameState(" + command + ")")


def summarize_nscoord(valobj, internal_dict):
    MAX = 0x3FFFFFFF
    value = valobj.GetValueAsSigned(0)
    if value == MAX:
        return "NS_UNCONSTRAINEDSIZE"
    if value == -MAX:
        return "NS_INTRINSIC_ISIZE_UNKNOWN"
    if value == MAX - (1000000 * 60):
        return "INFINITE_ISIZE_COORD"

    return "%.2gpx" % (value / 60.0)


def _find_child_field(valobj, name, depth=0):
    child = valobj.GetChildMemberWithName(name)
    if child.IsValid():
        return child
    if depth > 4:
        return None
    return _find_child_field(valobj.GetChildAtIndex(0), name, depth + 1)


def summarize_nspoint(valobj, internal_dict):
    return "(%s, %s)" % (
        summarize_nscoord(_find_child_field(valobj, "x"), {}),
        summarize_nscoord(_find_child_field(valobj, "y"), {}),
    )


def summarize_nssize(valobj, internal_dict):
    return "(%s, %s)" % (
        summarize_nscoord(_find_child_field(valobj, "width"), {}),
        summarize_nscoord(_find_child_field(valobj, "height"), {}),
    )


def summarize_nsrect(valobj, internal_dict):
    return "%s x %s" % (
        summarize_nspoint(valobj, {}),
        summarize_nssize(valobj, {}),
    )


def summarize_writing_mode(valobj, internal_dict):
    wm = valobj.GetChildMemberWithName("mWritingMode")

    # Values from: servo/components/style/logical_geometry.rs:29
    VERTICAL = 1 << 0
    # INLINE_REVERSED = 1 << 1
    VERTICAL_LR = 1 << 2
    LINE_INVERTED = 1 << 3
    RTL = 1 << 4
    VERTICAL_SIDEWAYS = 1 << 5
    TEXT_SIDEWAYS = 1 << 6
    # UPRIGHT = 1 << 7

    bits = wm.GetChildMemberWithName("_0").GetValueAsUnsigned()

    result = ""
    if bits & VERTICAL:
        result += "vertical-"
        if bits & VERTICAL_LR:
            result += "lr"
        else:
            result += "rl"
        if bits & (VERTICAL_SIDEWAYS | TEXT_SIDEWAYS):
            result += "-sideways"
        if bits & LINE_INVERTED:
            result += "inverted"
    else:
        result += "horizontal"
    if not (bits & RTL):
        result += "-ltr"
    else:
        result += "-rtl"

    return result


def init(debugger):
    debugger.HandleCommand("command script add -f lldbutils.layout.frametree frametree")
    debugger.HandleCommand(
        "command script add -f lldbutils.layout.frametree_pixels frametree_pixels"
    )
    debugger.HandleCommand(
        "command script add -f lldbutils.layout.frametreelimited frametreelimited"
    )
    debugger.HandleCommand(
        "command script add -f lldbutils.layout.frametreelimited_pixels frametreelimited_pixels"
    )
    debugger.HandleCommand("command alias ft frametree")
    debugger.HandleCommand("command alias ftp frametree_pixels")
    debugger.HandleCommand("command alias ftl frametreelimited")
    debugger.HandleCommand("command alias ftlp frametreelimited_pixels")
    debugger.HandleCommand("command script add -f lldbutils.layout.pstate pstate")

    debugger.HandleCommand(
        "type summary add nscoord -v -F lldbutils.layout.summarize_nscoord"
    )
    debugger.HandleCommand(
        "type summary add nsPoint -v -F lldbutils.layout.summarize_nspoint"
    )
    debugger.HandleCommand(
        "type summary add nsSize -v -F lldbutils.layout.summarize_nssize"
    )
    debugger.HandleCommand(
        "type summary add nsRect -v -F lldbutils.layout.summarize_nsrect"
    )
    debugger.HandleCommand(
        "type summary add mozilla::WritingMode -v -F lldbutils.layout.summarize_writing_mode"
    )
