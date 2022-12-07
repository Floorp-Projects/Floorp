pub(crate) type LONG = i32;
pub(crate) type INT = i32;
pub(crate) type UINT = u32;
pub(crate) type ULONG = u32;
pub(crate) type DWORD = ULONG;
pub(crate) type WORD = u16;
pub(crate) type LONGLONG = i64;
pub(crate) type ULONGLONG = u64;
pub(crate) type BYTE = u8;
pub(crate) type FLOAT = f32;
pub(crate) type REAL = FLOAT;
pub(crate) type HRESULT = LONG;

pub(crate) const S_OK: HRESULT = 0;
pub(crate) const INTSAFE_E_ARITHMETIC_OVERFLOW: HRESULT = 0x80070216;
pub(crate) const WGXERR_VALUEOVERFLOW: HRESULT = INTSAFE_E_ARITHMETIC_OVERFLOW;
pub(crate) const WINCODEC_ERR_VALUEOVERFLOW: HRESULT = INTSAFE_E_ARITHMETIC_OVERFLOW;
const fn MAKE_HRESULT(sev: LONG,fac: LONG,code: LONG) -> HRESULT {
    ( (((sev)<<31) | ((fac)<<16) | ((code))) )
}

const FACILITY_WGX: LONG = 0x898;


const fn MAKE_WGXHR( sev: LONG, code: LONG) -> HRESULT {
        MAKE_HRESULT( sev, FACILITY_WGX, (code) )
}
    
const fn MAKE_WGXHR_ERR( code: LONG ) -> HRESULT 
{
        MAKE_WGXHR( 1, code )
}

pub const WGXHR_CLIPPEDTOEMPTY: HRESULT =                MAKE_WGXHR(0, 1);
pub const WGXHR_EMPTYFILL: HRESULT =                      MAKE_WGXHR(0, 2);
pub const WGXHR_INTERNALTEMPORARYSUCCESS: HRESULT =      MAKE_WGXHR(0, 3);
pub const WGXHR_RESETSHAREDHANDLEMANAGER: HRESULT =      MAKE_WGXHR(0, 4);

pub const WGXERR_BADNUMBER: HRESULT =                     MAKE_WGXHR_ERR(0x00A);   //  4438

pub fn FAILED(hr: HRESULT) -> bool {
    hr != S_OK
}
pub trait NullPtr {
    fn make() -> Self;
}

impl<T> NullPtr for *mut T {
    fn make() -> Self {
        std::ptr::null_mut()
    }
}

impl<T> NullPtr for *const T {
    fn make() -> Self {
        std::ptr::null()
    }
}

pub fn NULL<T: NullPtr>() -> T {
    T::make()
}
#[derive(Default, Clone)]
pub struct RECT {
    pub left: LONG,
    pub top: LONG,
    pub right: LONG,
    pub bottom: LONG,
}
#[derive(Default, Clone, Copy, PartialEq, Eq)]
pub struct POINT {
    pub x: LONG,
    pub y: LONG
}
#[derive(Clone, Copy)]
pub struct MilPoint2F
{
    pub X: FLOAT,
    pub Y: FLOAT,
}

#[derive(Default, Clone)]
pub struct MilPointAndSizeL
{
    pub X: INT,
    pub Y: INT,
    pub Width: INT,
    pub Height: INT,
}

pub type CMILSurfaceRect = RECT;

#[derive(PartialEq)]
pub enum MilAntiAliasMode {
    None = 0,
    EightByEight = 1,
}
#[derive(PartialEq, Clone, Copy)]
pub enum MilFillMode {
    Alternate = 0,
    Winding = 1,
}

pub const    PathPointTypeStart: u8           = 0;    // move, 1 point
pub const    PathPointTypeLine: u8            = 1;    // line, 1 point
pub const    PathPointTypeBezier: u8          = 3;    // default Bezier (= cubic Bezier), 3 points
pub const    PathPointTypePathTypeMask: u8    = 0x07; // type mask (lowest 3 bits).
pub const    PathPointTypeCloseSubpath: u8    = 0x80; // closed flag


pub type DynArray<T> = Vec<T>;

pub trait DynArrayExts<T> {
    fn Reset(&mut self, shrink: bool);
    fn GetCount(&self) -> usize;
    fn SetCount(&mut self, count: usize);
    fn GetDataBuffer(&self) -> &[T];
}

impl<T> DynArrayExts<T> for DynArray<T> {
    fn Reset(&mut self, shrink: bool) {
        self.clear();
        if shrink {
            self.shrink_to_fit();
        }
    }
    fn GetCount(&self) -> usize {
        self.len()
    }
    fn SetCount(&mut self, count: usize) {
        assert!(count <= self.len());
        self.truncate(count);
    }

    fn GetDataBuffer(&self) -> &[T] {
        self
    }
}

pub struct CHwPipelineBuilder;

pub mod CoordinateSpace {
    #[derive(Default, Clone)]
    pub struct Shape;
    #[derive(Default, Clone)]
    pub struct Device;
}

pub trait IShapeData {
    fn GetFillMode(&self) -> MilFillMode;
}

pub type MilVertexFormat = DWORD;

pub enum MilVertexFormatAttribute {
    MILVFAttrNone = 0x0,
    MILVFAttrXY = 0x1,
    MILVFAttrZ = 0x2,
    MILVFAttrXYZ = 0x3,
    MILVFAttrNormal = 0x4,
    MILVFAttrDiffuse = 0x8,
    MILVFAttrSpecular = 0x10,
    MILVFAttrUV1 = 0x100,
    MILVFAttrUV2 = 0x300,
    MILVFAttrUV3 = 0x700,
    MILVFAttrUV4 = 0xf00,
    MILVFAttrUV5 = 0x1f00,
    MILVFAttrUV6 = 0x3f00,
    MILVFAttrUV7 = 0x7f00,
    MILVFAttrUV8 = 0xff00,      // Vertex fields that are pre-generated

}

pub struct CHwPipeline;

pub struct CBufferDispenser;
#[derive(Default)]
pub struct PointXYA
{
    pub x: f32,pub y: f32, pub a: f32,
}
