def format_char(c):
    if c == 0:
        return "\\0"
    elif c == 0x07:
        return "\\a"
    elif c == 0x08:
        return "\\b"
    elif c == 0x0c:
        return "\\f"
    elif c == 0x0a:
        return "\\n"
    elif c == 0x0d:
        return "\\r"
    elif c == 0x09:
        return "\\t"
    elif c == 0x0b:
        return "\\v"
    elif c == 0x5c:
        return "\\"
    elif c == 0x22:
        return "\\\""
    elif c == 0x39:
        return "\\'"
    elif c < 0x20 or c >= 0x80 and c <= 0xff:
        return "\\x%02x" % c
    elif c >= 0x0100:
        return "\\u%04x" % c
    else:
        return chr(c)

# Take an SBValue that is either a char* or char16_t* and formats it like lldb
# would when printing it.
def format_string(lldb_value, length=100):
    ptr = lldb_value.GetValueAsUnsigned(0)
    char_type = lldb_value.GetType().GetPointeeType()
    if char_type.GetByteSize() == 1:
        s = "\""
        size = 1
        mask = 0xff
    elif char_type.GetByteSize() == 2:
        s = "u\""
        size = 2
        mask = 0xffff
    else:
        return "(cannot format string with char type %s)" % char_type.GetName()
    i = 0
    terminated = False
    while i < length:
        c = lldb_value.CreateValueFromAddress("x", ptr + i * size, char_type).GetValueAsUnsigned(0) & mask
        if c == 0:
            terminated = True
            break
        s += format_char(c)
        i = i + 1
    s += "\""
    if not terminated and i != length:
        s += "..."
    return s

# Dereferences a raw pointer, nsCOMPtr, nsRefPtr, nsAutoPtr, already_AddRefed or
# mozilla::RefPtr; otherwise returns the value unchanged.
def dereference(lldb_value):
    if lldb_value.TypeIsPointerType():
        return lldb_value.Dereference()
    name = lldb_value.GetType().GetUnqualifiedType().GetName()
    if name.startswith("nsCOMPtr<") or name.startswith("nsRefPtr<") or name.startswith("nsAutoPtr<") or name.startswith("already_AddRefed<"):
        return lldb_value.GetChildMemberWithName("mRawPtr")
    if name.startswith("mozilla::RefPtr<"):
        return lldb_value.GetChildMemberWithName("ptr")
    return lldb_value
