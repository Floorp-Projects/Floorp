# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


def summarize_nscolor(valobj, internal_dict):
    colors = {
        "#800000": "maroon",
        "#ff0000": "red",
        "#ffa500": "orange",
        "#ffff00": "yellow",
        "#808000": "olive",
        "#800080": "purple",
        "#ff00ff": "fuchsia",
        "#ffffff": "white",
        "#00ff00": "lime",
        "#008000": "green",
        "#000080": "navy",
        "#0000ff": "blue",
        "#00ffff": "aqua",
        "#008080": "teal",
        "#000000": "black",
        "#c0c0c0": "silver",
        "#808080": "gray",
    }
    value = valobj.GetValueAsUnsigned(0)
    if value == 0:
        return "transparent"
    if value & 0xFF000000 != 0xFF000000:
        return "rgba(%d, %d, %d, %f)" % (
            value & 0xFF,
            (value >> 8) & 0xFF,
            (value >> 16) & 0xFF,
            ((value >> 24) & 0xFF) / 255.0,
        )
    color = "#%02x%02x%02x" % (value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)
    if color in colors:
        return colors[color]
    return color


def summarize_rect(valobj, internal_dict):
    x = valobj.GetChildMemberWithName("x").GetValue()
    y = valobj.GetChildMemberWithName("y").GetValue()
    width = valobj.GetChildMemberWithName("width").GetValue()
    height = valobj.GetChildMemberWithName("height").GetValue()
    return "%s, %s, %s, %s" % (x, y, width, height)


def rect_is_empty(valobj):
    width = valobj.GetChildMemberWithName("width").GetValueAsSigned()
    height = valobj.GetChildMemberWithName("height").GetValueAsSigned()
    return width <= 0 or height <= 0


def init(debugger):
    debugger.HandleCommand(
        "type summary add nscolor -v -F lldbutils.gfx.summarize_nscolor"
    )
    debugger.HandleCommand("type summary add nsRect -v -F lldbutils.gfx.summarize_rect")
    debugger.HandleCommand(
        "type summary add nsIntRect -v -F lldbutils.gfx.summarize_rect"
    )
    debugger.HandleCommand(
        "type summary add gfxRect -v -F lldbutils.gfx.summarize_rect"
    )
