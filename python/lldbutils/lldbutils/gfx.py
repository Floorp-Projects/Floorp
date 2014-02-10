import lldb

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
        "#808080": "gray"
    }
    value = valobj.GetValueAsUnsigned(0)
    if value == 0:
        return "transparent"
    if value & 0xff000000 != 0xff000000:
        return "rgba(%d, %d, %d, %f)" % (value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, ((value >> 24) & 0xff) / 255.0)
    color = "#%02x%02x%02x" % (value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff)
    if color in colors:
        return colors[color]
    return color

def init(debugger):
    debugger.HandleCommand("type summary add nscolor -v -F lldbutils.gfx.summarize_nscolor")
