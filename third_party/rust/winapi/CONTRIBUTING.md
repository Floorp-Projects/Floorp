# Guidelines

* Never get definitions from MinGW headers or MSDN. Always stick to the Windows SDK headers, in
  particular the latest Windows 10 SDK.
* Definitions which depend on whether `UNICODE` is defined should not be included. It is the user's
  responsibility to explicitly choose between `W` and `A` functions (and they should always choose
  `W`).
* For hexadecimal numbers, preserve the casing from the original headers (except for the uuid for
  `RIDL!`).
* If an identifier happens to match a Rust keyword, then append an underscore. For example `type`
  would turn into `type_`.
* Macro invocations at the item level should use `{}`. Macro invocations at the expression level
  should use `()`.

## Newlines and indentation

* The maximum line length for `winapi-rs` is 99, and is strictly enforced.
* Avoid line breaks when possible, but if you cannot make it fit, add line breaks as late as
  possible.
* When breaking on binary operators, put the operator at the beginning of the new line.
** This does not apply to the `:` used for inheritance in COM interfaces, which should remain at
   the end of the previous line.
* Do not use aligned indentation. Indentation should always be block indentation.
* Always use spaces for indentation.
* Blank lines are evil.
* Files must end with a trailing newline.

## Imports

* Imports should be in asciibetical order.

```Rust
use shared::basetsd::UINT64;
use shared::minwindef::{BOOL, BYTE, INT, LPVOID, UINT};
use um::d3dcommon::{
    D3D_CBUFFER_TYPE, D3D_FEATURE_LEVEL, D3D_INTERPOLATION_MODE, D3D_MIN_PRECISION, D3D_NAME,
    D3D_PARAMETER_FLAGS, D3D_PRIMITIVE, D3D_PRIMITIVE_TOPOLOGY, D3D_REGISTER_COMPONENT_TYPE,
    D3D_RESOURCE_RETURN_TYPE, D3D_SHADER_INPUT_TYPE, D3D_SHADER_VARIABLE_CLASS,
    D3D_SHADER_VARIABLE_TYPE, D3D_SRV_DIMENSION, D3D_TESSELLATOR_DOMAIN,
    D3D_TESSELLATOR_OUTPUT_PRIMITIVE, D3D_TESSELLATOR_PARTITIONING, ID3DBlob,
};
```

## Extern functions

* The calling convention specified should be the one for 32bit. Specify `system` for stdcall and
  `C` for cdecl (and `fastcall` for those couple of fastcall functions out there).
* One parameter per line.

```Rust
extern "system" {
    pub fn GetProcessTimes(
        hProcess: HANDLE,
        lpCreationTime: LPFILETIME,
        lpExitTime: LPFILETIME,
        lpKernelTime: LPFILETIME,
        lpUserTime: LPFILETIME,
    ) -> BOOL;
    pub fn GetCurrentProcess() -> HANDLE;
}
```

## Inline functions and macros

* Inline functions and macros should be translated into Rust functions.
* These functions should always be marked `#[inline]`.
* Until constant functions can be defined in the minimum Rust that winapi supports, if a function
  needs to be called in a constant, then a macro version of the function should be added.
* Inline functions are allowed to undergo some Rustification because they are not required to match
  the ABI of the original. Raw pointers can be translated into references, `BOOLEAN` into `bool`,
  and so on.
* If the function needs to do unsafe operations, then the function should be marked unsafe. If the
  function does not do anything unsafe, then it should remain a safe function.

## Function pointers

```Rust
FN!{stdcall DRAWSTATEPROC(
    hdc: HDC,
    lData: LPARAM,
    wData: WPARAM,
    cx: c_int,
    cy: c_int,
) -> BOOL}
FN!{stdcall NAMEENUMPROCA(
    LPSTR,
    LPARAM,
) -> BOOL}
```

## Constants

* Convert macro constants to Rust constants.
* The type of the constant should depend on where the constant is used. MSDN may help for this.
* If the constant has an unsigned type, but the literal needs to be negative, perform a cast such
  as `-1i16 as u16`. Use the primitive integer types that correspond to the type of the constant.
* If the constant is initialized to an expression involving a constant of a different type and a
  cast must be performed, do the cast using the primitive integer types.
* If the constant is a pointer that is initialized to a negative literal, do `-1isize as LPFOO`.

```C
#define CLSCTX_INPROC           (CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER)
```
```Rust
pub const CLSCTX_INPROC: CLSCTX = CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER;
```

## GUIDs

* Numbers should be padded with zeros to ensure consistent width.

```Rust
DEFINE_GUID!{GUID_DEVCLASS_SENSOR,
    0x5175d334, 0xc371, 0x4806, 0xb3, 0xba, 0x71, 0xfd, 0x53, 0xc9, 0x25, 0x8d}
```

## Structs

* One field per line.

```C
typedef struct _GROUP_AFFINITY {
    KAFFINITY Mask;
    WORD   Group;
    WORD   Reserved[3];
} GROUP_AFFINITY, *PGROUP_AFFINITY;
```
```Rust
STRUCT!{struct GROUP_AFFINITY {
    Mask: KAFFINITY,
    Group: WORD,
    Reserved: [WORD; 3],
}}
pub type PGROUP_AFFINITY = *mut GROUP_AFFINITY;
```

## Unions

```C
typedef union {
    USN_RECORD_COMMON_HEADER Header;
    USN_RECORD_V2 V2;
    USN_RECORD_V3 V3;
    USN_RECORD_V4 V4;
} USN_RECORD_UNION, *PUSN_RECORD_UNION;
```
```Rust
UNION!{union USN_RECORD_UNION {
    [u64; 10],
    Header Header_mut: USN_RECORD_COMMON_HEADER,
    V2 V2_mut: USN_RECORD_V2,
    V3 V3_mut: USN_RECORD_V3,
    V4 V4_mut: USN_RECORD_V4,
}}
pub type PUSN_RECORD_UNION = *mut USN_RECORD_UNION;
```

* The first parameter to `UNION!` is the storage for that union. It must have both the correct
  size and alignment. You can use the following C++ code to print out the storage for any union
  type that can be named. You may need to use a combination of `#define NONAMELESSUNION` and
  `decltype` in order to name anonymous unions.

```C++
char const * type_for_alignment(uintptr_t align) {
    switch (align) {
    case 1: return "u8";
    case 2: return "u16";
    case 4: return "u32";
    case 8: return "u64";
    default: throw;
    }
}
#define PRINT_UNION(x) cout << "[" << type_for_alignment(alignof(x))\
    << "; " << sizeof(x) / alignof(x) << "]" << endl;
int main() {
    PRINT_UNION(USN_RECORD_UNION);
}
```

* Note that sometimes the storage of a union varies based on whether the target is 32bit or 64bit,
  in which case `UNION!` allows a second storage to be specified, the first for 32bit and the
  second for 64bit.

```Rust
UNION!{union D3D12_RESOURCE_BARRIER_u {
    [u32; 4] [u64; 3],
    Transition Transition_mut: D3D12_RESOURCE_TRANSITION_BARRIER,
    Aliasing Aliasing_mut: D3D12_RESOURCE_ALIASING_BARRIER,
    UAV UAV_mut: D3D12_RESOURCE_UAV_BARRIER,
}}
```

## Anonymous unions and structs

* If the type `FOO` contains a single anonymous struct or union, give the anonymous struct or union
  a name of `FOO_s` or `FOO_u` respectively, and the field a name of `s` or `u` respectively.
* If the type `FOO` contains multiple anonymous structs or unions, append a number, such as `s1:
  FOO_s1` `s2: FOO_s2` or `u1: FOO_u1` `u2: FOO_u2`.
* If the field does have a name, such as `Bar`, but still contains an anonymous struct or union
  then retain the name of `Bar` for the field and name the anonymous struct or union after the
  field, such as `FOO_Bar`.

## Union with a primitive field and an anonymous bitfield struct of the same type

```C
typedef union _USB_HUB_STATUS {
    USHORT  AsUshort16;
    struct {
        USHORT  LocalPowerLost:1;
        USHORT  OverCurrent:1;
        USHORT  Reserved:14;
    };
} USB_HUB_STATUS, *PUSB_HUB_STATUS;
```

```Rust
STRUCT!{struct USB_HUB_STATUS {
    AsUshort16: USHORT,
}}
BITFIELD!{USB_HUB_STATUS AsUshort16: USHORT [
    LocalPowerLost set_LocalPowerLost[0..1],
    OverCurrent set_OverCurrent[1..2],
    Reserved set_Reserved[2..16],
]}
```

* If the union is anonymous and the field for it is unnamed, then instead of creating a single
  field struct, simply inline that field into the containing struct and apply the bitfield to
  *that*.

## COM interfaces

* The uuid should always be lowercase hex.
* If the COM interface does not have a uuid then use a uuid of all zeroes.
* Uuid numbers should be padded with zeros to ensure consistent width.
* Sometimes a COM interface will have two methods with identical names (an overloaded method). If
  the two methods are both named `Foo`, then name them `Foo_1` and `Foo_2`. In addition,
  overloaded methods must appear in *reverse* order to comply with the COM binary interface. See
  #523 for more details.

```Rust
RIDL!{#[uuid(0x6d4865fe, 0x0ab8, 0x4d91, 0x8f, 0x62, 0x5d, 0xd6, 0xbe, 0x34, 0xa3, 0xe0)]
interface IDWriteFontFileStream(IDWriteFontFileStreamVtbl): IUnknown(IUnknownVtbl) {
    fn ReadFileFragment(
        fragmentStart: *mut *const c_void,
        fileOffset: UINT64,
        fragmentSize: UINT64,
        fragmentContext: *mut *mut c_void,
    ) -> HRESULT,
    fn ReleaseFileFragment(
        fragmentContext: *mut c_void,
    ) -> (),
    fn GetFileSize(
        fileSize: *mut UINT64,
    ) -> HRESULT,
    fn GetLastWriteTime(
        lastWriteTime: *mut UINT64,
    ) -> HRESULT
}}
```

## COM classes

* The uuid should always be lowercase hex.
* Uuid numbers should be padded with zeros to ensure consistent width.

```Rust
RIDL!{#[uuid(0xd9f6ee60, 0x58c9, 0x458b, 0x88, 0xe1, 0x2f, 0x90, 0x8f, 0xd7, 0xf8, 0x7c)]
class SpDataKey;}
```

## Organization of code

* All definitions go into the source file that directly maps to the header the definition is from.
    * Stuff in `src/winrt` is special and has its own namespaced organization.
* Definitions are defined in the same order as they are in the original header.

## Dealing with duplicates

* Sometimes two headers will define the same thing.
    * If the duplicated thing is a simple typedef or extern function or constant, then
      duplicate the definition.
    * If the duplicated thing is a struct or union or COM interface or COM class, then choose one
      header to be the canonical source of truth for that definition and publicly re-export the
      thing from the other header.
