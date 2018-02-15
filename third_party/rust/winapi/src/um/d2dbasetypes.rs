// Copyright © 2015-2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
//! Mappings for the contents of d2dbasetypes.h
use shared::basetsd::{UINT32};
use shared::d3d9types::{D3DCOLORVALUE};
use shared::minwindef::{FLOAT};
use shared::windef::{POINT, RECT};
STRUCT!{struct D2D_POINT_2U {
    x: UINT32,
    y: UINT32,
}}
STRUCT!{struct D2D_POINT_2F {
    x: FLOAT,
    y: FLOAT,
}}
pub type D2D_POINT_2L = POINT;
STRUCT!{struct D2D_VECTOR_2F {
    x: FLOAT,
    y: FLOAT,
}}
STRUCT!{struct D2D_VECTOR_3F {
    x: FLOAT,
    y: FLOAT,
    z: FLOAT,
}}
STRUCT!{struct D2D_VECTOR_4F {
    x: FLOAT,
    y: FLOAT,
    z: FLOAT,
    w: FLOAT,
}}
STRUCT!{struct D2D_RECT_F {
    left: FLOAT,
    top: FLOAT,
    right: FLOAT,
    bottom: FLOAT,
}}
STRUCT!{struct D2D_RECT_U {
    left: UINT32,
    top: UINT32,
    right: UINT32,
    bottom: UINT32,
}}
pub type D2D_RECT_L = RECT;
STRUCT!{struct D2D_SIZE_F {
    width: FLOAT,
    height: FLOAT,
}}
STRUCT!{struct D2D_SIZE_U {
    width: UINT32,
    height: UINT32,
}}
pub type D2D_COLOR_F = D3DCOLORVALUE;
STRUCT!{struct D2D_MATRIX_3X2_F {
    matrix: [[FLOAT; 2]; 3],
}}
STRUCT!{struct D2D_MATRIX_4X3_F {
    matrix: [[FLOAT; 3]; 4],
}}
STRUCT!{struct D2D_MATRIX_4X4_F {
    matrix: [[FLOAT; 4]; 4],
}}
STRUCT!{struct D2D_MATRIX_5X4_F {
    matrix: [[FLOAT; 4]; 5],
}}
