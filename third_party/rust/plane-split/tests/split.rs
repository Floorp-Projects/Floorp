extern crate euclid;
extern crate plane_split;

use std::f32::consts::FRAC_PI_4;
use euclid::{Angle, TypedTransform3D, TypedRect, vec3};
use plane_split::{BspSplitter, Polygon, Splitter, make_grid};


fn grid_impl(count: usize, splitter: &mut Splitter<f32, ()>) {
    let polys = make_grid(count);
    let result = splitter.solve(&polys, vec3(0.0, 0.0, 1.0));
    assert_eq!(result.len(), count + count*count + count*count*count);
}

#[test]
fn grid_bsp() {
    grid_impl(2, &mut BspSplitter::new());
}


fn sort_rotation(splitter: &mut Splitter<f32, ()>) {
    let transform0: TypedTransform3D<f32, (), ()> =
        TypedTransform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(-FRAC_PI_4));
    let transform1: TypedTransform3D<f32, (), ()> =
        TypedTransform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(0.0));
    let transform2: TypedTransform3D<f32, (), ()> =
        TypedTransform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(FRAC_PI_4));

    let rect: TypedRect<f32, ()> = euclid::rect(-10.0, -10.0, 20.0, 20.0);
    let p1 = Polygon::from_transformed_rect(rect, transform0, 0);
    let p2 = Polygon::from_transformed_rect(rect, transform1, 1);
    let p3 = Polygon::from_transformed_rect(rect, transform2, 2);
    assert!(p1.is_some() && p2.is_some() && p3.is_some(), "Cannot construct transformed polygons");

    let polys = [ p1.unwrap(), p2.unwrap(), p3.unwrap() ];
    let result = splitter.solve(&polys, vec3(0.0, 0.0, -1.0));
    let ids: Vec<_> = result.iter().map(|poly| poly.anchor).collect();
    assert_eq!(&ids, &[2, 1, 0, 1, 2]);
}

#[test]
fn rotation_bsp() {
    sort_rotation(&mut BspSplitter::new());
}


fn sort_trivial(splitter: &mut Splitter<f32, ()>) {
    let anchors: Vec<_> = (0usize .. 10).collect();
    let rect: TypedRect<f32, ()> = euclid::rect(-10.0, -10.0, 20.0, 20.0);
    let polys: Vec<_> = anchors.iter().map(|&anchor| {
        let transform: TypedTransform3D<f32, (), ()> = TypedTransform3D::create_translation(0.0, 0.0, anchor as f32);
        let poly = Polygon::from_transformed_rect(rect, transform, anchor);
        assert!(poly.is_some(), "Cannot construct transformed polygons");
        poly.unwrap()
    }).collect();

    let result = splitter.solve(&polys, vec3(0.0, 0.0, -1.0));
    let anchors1: Vec<_> = result.iter().map(|p| p.anchor).collect();
    let mut anchors2 = anchors1.clone();
    anchors2.sort_by_key(|&a| -(a as i32));
    assert_eq!(anchors1, anchors2); //make sure Z is sorted backwards
}

#[test]
fn trivial_bsp() {
    sort_trivial(&mut BspSplitter::new());
}
