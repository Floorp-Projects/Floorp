use std::marker::PhantomData;

use crate::types::CoordinateSpace;

pub type CMILMatrix  = CMatrix<CoordinateSpace::Shape,CoordinateSpace::Device>;
#[derive(Default, Clone)]
pub struct CMatrix<InCoordSpace, OutCoordSpace> {
    _11: f32, _12: f32, _13: f32, _14: f32,
    _21: f32, _22: f32, _23: f32 , _24: f32,
    _31: f32, _32: f32, _33: f32, _34: f32,
     _41: f32, _42: f32, _43: f32, _44: f32,
    in_coord: PhantomData<InCoordSpace>,
    out_coord: PhantomData<OutCoordSpace>
}

impl<InCoordSpace: Default, OutCoordSpace: Default> CMatrix<InCoordSpace, OutCoordSpace> {
    pub fn Identity() -> Self { let mut ret: Self = Default::default();
        ret._11 = 1.;
        ret._22 = 1.;
        ret._33 = 1.;
        ret._44 = 1.;
        ret
    }
    pub fn GetM11(&self) -> f32 { self._11 }
    pub fn GetM12(&self) -> f32 { self._12 }
    pub fn GetM21(&self) -> f32 { self._21 }
    pub fn GetM22(&self) -> f32 { self._22 }
    pub fn GetDx(&self) -> f32 { self._41 }
    pub fn GetDy(&self) -> f32 { self._42 }

    pub fn SetM11(&mut self, r: f32) { self._11 = r}
    pub fn SetM12(&mut self, r: f32) { self._12 = r}
    pub fn SetM21(&mut self, r: f32) { self._21 = r}
    pub fn SetM22(&mut self, r: f32) { self._22 = r}
    pub fn SetDx(&mut self, dx: f32) { self._41 = dx }
    pub fn SetDy(&mut self, dy: f32) { self._42 = dy }
}