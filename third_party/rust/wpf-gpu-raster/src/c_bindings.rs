use crate::{PathBuilder, OutputPath, OutputVertex, FillMode, rasterize_to_tri_list};
use crate::types::{BYTE, POINT};

#[no_mangle]
pub extern "C" fn wgr_new_builder() -> *mut PathBuilder {
    let pb = PathBuilder::new();
    Box::into_raw(Box::new(pb))
}

#[no_mangle]
pub extern "C" fn wgr_builder_reset(pb: &mut PathBuilder) {
    pb.reset();
}

#[no_mangle]
pub extern "C" fn wgr_builder_move_to(pb: &mut PathBuilder, x: f32, y: f32) {
    pb.move_to(x, y);
}

#[no_mangle]
pub extern "C" fn wgr_builder_line_to(pb: &mut PathBuilder, x: f32, y: f32) {
    pb.line_to(x, y);
}

#[no_mangle]
pub extern "C" fn wgr_builder_curve_to(pb: &mut PathBuilder, c1x: f32, c1y: f32, c2x: f32, c2y: f32, x: f32, y: f32) {
    pb.curve_to(c1x, c1y, c2x, c2y, x, y);
}

#[no_mangle]
pub extern "C" fn wgr_builder_quad_to(pb: &mut PathBuilder, cx: f32, cy: f32, x: f32, y: f32) {
    pb.quad_to(cx, cy, x, y);
}

#[no_mangle]
pub extern "C" fn wgr_builder_close(pb: &mut PathBuilder) {
    pb.close();
}

#[no_mangle]
pub extern "C" fn wgr_builder_set_fill_mode(pb: &mut PathBuilder, fill_mode: FillMode) {
    pb.set_fill_mode(fill_mode)
}

#[repr(C)]
pub struct Path {
    fill_mode: FillMode,
    points: *const POINT,
    num_points: usize,
    types: *const BYTE,
    num_types: usize,
}

impl From<OutputPath> for Path {
    fn from(output_path: OutputPath) -> Self {
        let path = Self {
            fill_mode: output_path.fill_mode,
            points: output_path.points.as_ptr(),
            num_points: output_path.points.len(),
            types: output_path.types.as_ptr(),
            num_types: output_path.types.len(),
        };
        std::mem::forget(output_path);
        path
    }
}

impl Into<OutputPath> for Path {
    fn into(self) -> OutputPath {
        OutputPath {
            fill_mode: self.fill_mode,
            points: unsafe {
                if self.points == std::ptr::null() {
                    Default::default()
                } else {
                    Box::from_raw(std::slice::from_raw_parts_mut(self.points as *mut POINT, self.num_points))
                }
            },
            types: unsafe {
                if self.types == std::ptr::null() {
                    Default::default()
                } else {
                    Box::from_raw(std::slice::from_raw_parts_mut(self.types as *mut BYTE, self.num_types))
                }
            },
        }
    }
}

#[no_mangle]
pub extern "C" fn wgr_builder_get_path(pb: &mut PathBuilder) -> Path {
    Path::from(pb.get_path().unwrap_or_default())
}

#[repr(C)]
pub struct VertexBuffer {
    data: *const OutputVertex,
    len: usize
}

#[no_mangle]
pub extern "C" fn wgr_path_rasterize_to_tri_list(
    path: &Path,
    clip_x: i32,
    clip_y: i32,
    clip_width: i32,
    clip_height: i32,
    need_inside: bool,
    need_outside: bool,
    rasterization_truncates: bool,
    output_ptr: *mut OutputVertex,
    output_capacity: usize,
) -> VertexBuffer {
    let output_buffer = if output_ptr != std::ptr::null_mut() {
        unsafe { Some(std::slice::from_raw_parts_mut(output_ptr, output_capacity)) }
    } else {
        None
    };
    let mut result = rasterize_to_tri_list(
        path.fill_mode,
        unsafe { std::slice::from_raw_parts(path.types, path.num_types) },
        unsafe { std::slice::from_raw_parts(path.points, path.num_points) },
        clip_x, clip_y, clip_width, clip_height,
        need_inside, need_outside,
        rasterization_truncates,
        output_buffer
    );
    if let Some(output_buffer_size) = result.get_output_buffer_size() {
        VertexBuffer {
            data: std::ptr::null(),
            len: output_buffer_size,
        }
    } else {
        let slice = result.flush_output();
        let vb = VertexBuffer {
            data: slice.as_ptr(),
            len: slice.len(),
        };
        std::mem::forget(slice);
        vb
    }
}

#[no_mangle]
pub extern "C" fn wgr_path_release(path: Path) {
    let output_path: OutputPath = path.into();
    drop(output_path);
}

#[no_mangle]
pub extern "C" fn wgr_vertex_buffer_release(vb: VertexBuffer)
{
    if vb.data != std::ptr::null() {
        unsafe {
            drop(Box::from_raw(std::slice::from_raw_parts_mut(vb.data as *mut OutputVertex, vb.len)));
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgr_builder_release(pb: *mut PathBuilder) {
    drop(Box::from_raw(pb));
}
