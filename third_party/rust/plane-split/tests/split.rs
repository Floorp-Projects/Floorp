extern crate euclid;
extern crate plane_split;

use std::f32::consts::FRAC_PI_4;
use euclid::{Radians, TypedMatrix4D, TypedPoint2D, TypedPoint3D, TypedSize2D, TypedRect};
use plane_split::{BspSplitter, NaiveSplitter, Polygon, Splitter, _make_grid};


fn grid_impl(count: usize, splitter: &mut Splitter<f32, ()>) {
    let polys = _make_grid(count);
    let result = splitter.solve(&polys, TypedPoint3D::new(0.0, 0.0, 1.0));
    assert_eq!(result.len(), count + count*count + count*count*count);
}

#[test]
fn grid_naive() {
    grid_impl(2, &mut NaiveSplitter::new());
}

#[test]
fn grid_bsp() {
    grid_impl(2, &mut BspSplitter::new());
}


fn sort_rotation(splitter: &mut Splitter<f32, ()>) {
    let transform0: TypedMatrix4D<f32, (), ()> =
        TypedMatrix4D::create_rotation(0.0, 1.0, 0.0, Radians::new(-FRAC_PI_4));
    let transform1: TypedMatrix4D<f32, (), ()> =
        TypedMatrix4D::create_rotation(0.0, 1.0, 0.0, Radians::new(0.0));
    let transform2: TypedMatrix4D<f32, (), ()> =
        TypedMatrix4D::create_rotation(0.0, 1.0, 0.0, Radians::new(FRAC_PI_4));

    let rect: TypedRect<f32, ()> = TypedRect::new(TypedPoint2D::new(-10.0, -10.0), TypedSize2D::new(20.0, 20.0));
    let polys = [
        Polygon::from_transformed_rect(rect, transform0, 0),
        Polygon::from_transformed_rect(rect, transform1, 1),
        Polygon::from_transformed_rect(rect, transform2, 2),
    ];

    let result = splitter.solve(&polys, TypedPoint3D::new(0.0, 0.0, -1.0));
    let ids: Vec<_> = result.iter().map(|poly| poly.anchor).collect();
    assert_eq!(&ids, &[2, 1, 0, 1, 2]);
}

#[test]
fn rotation_naive() {
    sort_rotation(&mut NaiveSplitter::new());
}

#[test]
fn rotation_bsp() {
    sort_rotation(&mut BspSplitter::new());
}


fn sort_trivial(splitter: &mut Splitter<f32, ()>) {
    let anchors: Vec<_> = (0usize .. 10).collect();
    let rect: TypedRect<f32, ()> = TypedRect::new(TypedPoint2D::new(-10.0, -10.0), TypedSize2D::new(20.0, 20.0));
    let polys: Vec<_> = anchors.iter().map(|&anchor| {
        let transform: TypedMatrix4D<f32, (), ()> = TypedMatrix4D::create_translation(0.0, 0.0, anchor as f32);
        Polygon::from_transformed_rect(rect, transform, anchor)
    }).collect();

    let result = splitter.solve(&polys, TypedPoint3D::new(0.0, 0.0, -1.0));
    let anchors1: Vec<_> = result.iter().map(|p| p.anchor).collect();
    let mut anchors2 = anchors1.clone();
    anchors2.sort_by_key(|&a| -(a as i32));
    assert_eq!(anchors1, anchors2); //make sure Z is sorted backwards
}

#[test]
fn trivial_naive() {
    sort_trivial(&mut NaiveSplitter::new());
}

#[test]
fn trivial_bsp() {
    sort_trivial(&mut BspSplitter::new());
}
