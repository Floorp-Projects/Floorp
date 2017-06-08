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

class RegionSyntheticChildrenProvider:

    def __init__(self, valobj, internal_dict, rect_type = "nsRect"):
        self.rect_type = rect_type
        self.valobj = valobj
        self.pixman_region = self.valobj.GetChildMemberWithName("mImpl")
        self.pixman_data = self.pixman_region.GetChildMemberWithName("data")
        self.pixman_extents = self.pixman_region.GetChildMemberWithName("extents")
        self.num_rects = self.pixman_region_num_rects()
        self.box_type = self.pixman_extents.GetType()
        self.box_type_size = self.box_type.GetByteSize()
        self.box_list_base_ptr = self.pixman_data.GetValueAsUnsigned(0) + self.pixman_data.GetType().GetPointeeType().GetByteSize()

    def pixman_region_num_rects(self):
        if self.pixman_data.GetValueAsUnsigned(0):
            return self.pixman_data.Dereference().GetChildMemberWithName("numRects").GetValueAsUnsigned(0)
        return 1

    def num_children(self):
        return 2 + self.num_rects

    def get_child_index(self, name):
        if name == "numRects":
            return 0
        if name == "bounds":
            return 1
        return None

    def convert_pixman_box_to_rect(self, valobj, name):
        x1 = valobj.GetChildMemberWithName("x1").GetValueAsSigned()
        x2 = valobj.GetChildMemberWithName("x2").GetValueAsSigned()
        y1 = valobj.GetChildMemberWithName("y1").GetValueAsSigned()
        y2 = valobj.GetChildMemberWithName("y2").GetValueAsSigned()
        return valobj.CreateValueFromExpression(name,
            '%s(%d, %d, %d, %d)' % (self.rect_type, x1, y1, x2 - x1, y2 - y1))

    def get_child_at_index(self, index):
        if index == 0:
            return self.pixman_data.CreateValueFromExpression('numRects', '(uint32_t)%d' % self.num_rects)
        if index == 1:
            return self.convert_pixman_box_to_rect(self.pixman_extents, 'bounds')

        rect_index = index - 2
        if rect_index >= self.num_rects:
            return None
        if self.num_rects == 1:
            return self.convert_pixman_box_to_rect(self.pixman_extents, 'bounds')
        box_address = self.box_list_base_ptr + rect_index * self.box_type_size
        box = self.pixman_data.CreateValueFromAddress('', box_address, self.box_type)
        return self.convert_pixman_box_to_rect(box, "[%d]" % rect_index)

class IntRegionSyntheticChildrenProvider:
    def __init__(self, valobj, internal_dict):
        wrapped_region = valobj.GetChildMemberWithName("mImpl")
        self.wrapped_provider = RegionSyntheticChildrenProvider(wrapped_region, internal_dict, "nsIntRect")

    def num_children(self):
        return self.wrapped_provider.num_children()

    def get_child_index(self, name):
        return self.wrapped_provider.get_child_index(name)

    def get_child_at_index(self, index):
        return self.wrapped_provider.get_child_at_index(index)

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

def summarize_region(valobj, internal_dict):
    # This function makes use of the synthetic children generated for ns(Int)Regions.
    bounds = valobj.GetChildMemberWithName("bounds")
    bounds_summary = summarize_rect(bounds, internal_dict)
    num_rects = valobj.GetChildMemberWithName("numRects").GetValueAsUnsigned(0)
    if num_rects <= 1:
        if rect_is_empty(bounds):
            return "empty"
        if num_rects == 1:
            return "one rect: " + bounds_summary
    return str(num_rects) + " rects, bounds: " + bounds_summary

def init(debugger):
    debugger.HandleCommand("type summary add nscolor -v -F lldbutils.gfx.summarize_nscolor")
    debugger.HandleCommand("type summary add nsRect -v -F lldbutils.gfx.summarize_rect")
    debugger.HandleCommand("type summary add nsIntRect -v -F lldbutils.gfx.summarize_rect")
    debugger.HandleCommand("type summary add gfxRect -v -F lldbutils.gfx.summarize_rect")
    debugger.HandleCommand("type synthetic add nsRegion -l lldbutils.gfx.RegionSyntheticChildrenProvider")
    debugger.HandleCommand("type synthetic add nsIntRegion -l lldbutils.gfx.IntRegionSyntheticChildrenProvider")
    debugger.HandleCommand("type summary add nsRegion -v -F lldbutils.gfx.summarize_region")
    debugger.HandleCommand("type summary add nsIntRegion -v -F lldbutils.gfx.summarize_region")
