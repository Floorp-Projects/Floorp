#![allow(non_snake_case, non_upper_case_globals)]

use std::mem;
use std::os::raw::c_void;
use std::slice;
use std::sync::atomic::AtomicUsize;
use winapi::shared::guiddef::REFIID;
use winapi::shared::minwindef::{UINT, ULONG};
use winapi::shared::winerror::S_OK;
use winapi::um::d2d1::{D2D1_BEZIER_SEGMENT, D2D1_FIGURE_BEGIN, D2D1_FIGURE_END, D2D1_FIGURE_END_CLOSED};
use winapi::um::d2d1::{D2D1_FILL_MODE, D2D1_PATH_SEGMENT, D2D1_POINT_2F};
use winapi::um::d2d1::{ID2D1SimplifiedGeometrySink, ID2D1SimplifiedGeometrySinkVtbl};
use winapi::um::unknwnbase::{IUnknown, IUnknownVtbl};
use winapi::um::winnt::HRESULT;

use com_helpers::{Com, UuidOfIUnknown};
use comptr::ComPtr;
use outline_builder::OutlineBuilder;

DEFINE_GUID!{
    D2D1_SIMPLIFIED_GEOMETRY_SINK_UUID,
    0x2cd9069e, 0x12e2, 0x11dc, 0x9f, 0xed, 0x00, 0x11, 0x43, 0xa0, 0x55, 0xf9
}

static GEOMETRY_SINK_VTBL: ID2D1SimplifiedGeometrySinkVtbl = ID2D1SimplifiedGeometrySinkVtbl {
    parent: implement_iunknown!(static ID2D1SimplifiedGeometrySink,
                                D2D1_SIMPLIFIED_GEOMETRY_SINK_UUID,
                                GeometrySinkImpl),
    BeginFigure: GeometrySinkImpl_BeginFigure,
    EndFigure: GeometrySinkImpl_EndFigure,
    AddLines: GeometrySinkImpl_AddLines,
    AddBeziers: GeometrySinkImpl_AddBeziers,
    Close: GeometrySinkImpl_Close,
    SetFillMode: GeometrySinkImpl_SetFillMode,
    SetSegmentFlags: GeometrySinkImpl_SetSegmentFlags,
};

pub struct GeometrySinkImpl {
    refcount: AtomicUsize,
    outline_builder: Box<OutlineBuilder>,
}

impl Com<ID2D1SimplifiedGeometrySink> for GeometrySinkImpl {
    type Vtbl = ID2D1SimplifiedGeometrySinkVtbl;
    #[inline]
    fn vtbl() -> &'static ID2D1SimplifiedGeometrySinkVtbl {
        &GEOMETRY_SINK_VTBL
    }
}

impl Com<IUnknown> for GeometrySinkImpl {
    type Vtbl = IUnknownVtbl;
    #[inline]
    fn vtbl() -> &'static IUnknownVtbl {
        &GEOMETRY_SINK_VTBL.parent
    }
}

impl GeometrySinkImpl {
    pub fn new(outline_builder: Box<OutlineBuilder>) -> GeometrySinkImpl {
        GeometrySinkImpl {
            refcount: AtomicUsize::new(1),
            outline_builder,
        }
    }
}

unsafe extern "system" fn GeometrySinkImpl_BeginFigure(this: *mut ID2D1SimplifiedGeometrySink,
                                                       start_point: D2D1_POINT_2F,
                                                       _: D2D1_FIGURE_BEGIN) {
    let this = GeometrySinkImpl::from_interface(this);
    (*this).outline_builder.move_to(start_point.x, start_point.y)
}

unsafe extern "system" fn GeometrySinkImpl_EndFigure(this: *mut ID2D1SimplifiedGeometrySink,
                                                     figure_end: D2D1_FIGURE_END) {
    let this = GeometrySinkImpl::from_interface(this);
    if figure_end == D2D1_FIGURE_END_CLOSED {
        (*this).outline_builder.close()
    }
}

unsafe extern "system" fn GeometrySinkImpl_AddLines(this: *mut ID2D1SimplifiedGeometrySink,
                                                    points: *const D2D1_POINT_2F,
                                                    points_count: UINT) {
    let this = GeometrySinkImpl::from_interface(this);
    let points = slice::from_raw_parts(points, points_count as usize);
    for point in points {
        (*this).outline_builder.line_to(point.x, point.y)
    }
}

unsafe extern "system" fn GeometrySinkImpl_AddBeziers(this: *mut ID2D1SimplifiedGeometrySink,
                                                      beziers: *const D2D1_BEZIER_SEGMENT,
                                                      beziers_count: UINT) {
    let this = GeometrySinkImpl::from_interface(this);
    let beziers = slice::from_raw_parts(beziers, beziers_count as usize);
    for bezier in beziers {
        (*this).outline_builder.curve_to(bezier.point1.x, bezier.point1.y,
                                         bezier.point2.x, bezier.point2.y,
                                         bezier.point3.x, bezier.point3.y)
    }
}

unsafe extern "system" fn GeometrySinkImpl_Close(_: *mut ID2D1SimplifiedGeometrySink) -> HRESULT {
    S_OK
}

unsafe extern "system" fn GeometrySinkImpl_SetFillMode(_: *mut ID2D1SimplifiedGeometrySink,
                                                       _: D2D1_FILL_MODE) {}

unsafe extern "system" fn GeometrySinkImpl_SetSegmentFlags(_: *mut ID2D1SimplifiedGeometrySink,
                                                           _: D2D1_PATH_SEGMENT) {}
