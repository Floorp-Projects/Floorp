use peek_poke::{Peek, PeekCopy, PeekPoke, Poke};

#[repr(C)]
#[derive(Debug, PartialEq, PeekPoke)]
pub struct Point {
    pub x: f32,
    pub y: f32,
}

#[repr(C)]
#[derive(Debug, PartialEq, PeekPoke)]
pub struct Size {
    pub w: f32,
    pub h: f32,
}

#[repr(C)]
#[derive(Debug, PartialEq, PeekPoke)]
pub struct Rect {
    pub point: Point,
    pub size: Size,
}

pub type PipelineSourceId = u32;
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, PeekPoke)]
pub struct PipelineId(pub PipelineSourceId, pub u32);

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, PeekPoke)]
pub struct ClipChainId(pub u64, pub PipelineId);

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, PeekCopy, Poke)]
pub enum ClipId {
    Clip(usize, PipelineId),
    ClipChain(ClipChainId),
}

pub type ItemTag = (u64, u16);
#[repr(C)]
#[derive(Debug, PartialEq, PeekPoke)]
pub struct SpatialId(pub usize, PipelineId);

#[repr(C)]
#[derive(Debug, PartialEq, PeekPoke)]
pub struct CommonItemProperties {
    pub clip_rect: Rect,
    pub clip_id: ClipId,
    pub spatial_id: SpatialId,
    #[cfg(any(feature = "option_copy", feature = "option_default"))]
    pub hit_info: Option<ItemTag>,
    pub is_backface_visible: bool,
}

#[inline(never)]
unsafe fn test<T: Poke>(bytes: *mut u8, x: &T) -> *mut u8 {
    x.poke_into(bytes)
}

fn poke_into<T: Poke>(bytes: &mut Vec<u8>, x: &T) {
    bytes.reserve(<T>::max_size());
    let ptr = bytes.as_mut_ptr();
    let new_ptr = unsafe { test(ptr, x) };
    let new_len = (new_ptr as usize) - (bytes.as_ptr() as usize);
    unsafe {
        bytes.set_len(new_len);
    }
}

#[inline(never)]
unsafe fn test1<T: Peek>(x: &mut T, bytes: *const u8) -> *const u8 {
    x.peek_from(bytes)
}

fn peek_from<T: Peek>(x: &mut T, bytes: &[u8]) -> usize {
    assert!(bytes.len() >= <T>::max_size());
    let ptr = bytes.as_ptr();
    let new_ptr = unsafe { test1(x, ptr) };
    let size = (new_ptr as usize) - (ptr as usize);
    assert!(size <= bytes.len());
    size
}

pub fn main() {
    let x = CommonItemProperties {
        clip_rect: Rect {
            point: Point { x: 1.0, y: 2.0 },
            size: Size { w: 4.0, h: 5.0 },
        },
        clip_id: ClipId::Clip(5, PipelineId(1, 2)),
        spatial_id: SpatialId(3, PipelineId(4, 5)),
        #[cfg(any(feature = "option_copy", feature = "option_default"))]
        hit_info: None,
        is_backface_visible: true,
    };
    let mut bytes = Vec::<u8>::new();
    poke_into(&mut bytes, &x);
    println!("{:?}", bytes);
    assert_eq!(
        bytes,
        vec![
            0u8, 0, 128, 63, 0, 0, 0, 64, 0, 0, 128, 64, 0, 0, 160, 64, 3, 0, 0, 0, 0, 0, 0, 0, 4,
            0, 0, 0, 5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 1
        ]
    );
    let mut y: CommonItemProperties = unsafe { std::mem::zeroed() };
    peek_from(&mut y, &bytes);
    println!("{:?}", y);
    assert_eq!(x, y);
}
