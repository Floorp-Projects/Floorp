extern crate euclid;
extern crate plane_split;

use euclid::{point3, rect, vec3};
use euclid::{Angle, Rect, Transform3D};
use plane_split::{Clipper, Plane, Polygon};

use std::f32::consts::FRAC_PI_4;


#[test]
fn clip_in() {
    let plane: Plane<f32, ()> = Plane::from_unnormalized(vec3(1.0, 0.0, 1.0), 20.0).unwrap().unwrap();
    let mut clipper = Clipper::new();
    clipper.add(plane);

    let poly = Polygon::from_points(
        [
            point3(-10.0, -10.0, 0.0),
            point3(10.0, -10.0, 0.0),
            point3(10.0, 10.0, 0.0),
            point3(-10.0, 10.0, 0.0),
        ],
        0,
    ).unwrap();

    let results = clipper.clip(poly.clone());
    assert_eq!(results[0], poly);
    assert_eq!(results.len(), 1);
}

#[test]
fn clip_out() {
    let plane: Plane<f32, ()> = Plane::from_unnormalized(vec3(1.0, 0.0, 1.0), -20.0).unwrap().unwrap();
    let mut clipper = Clipper::new();
    clipper.add(plane);

    let poly = Polygon::from_points(
        [
            point3(-10.0, -10.0, 0.0),
            point3(10.0, -10.0, 0.0),
            point3(10.0, 10.0, 0.0),
            point3(-10.0, 10.0, 0.0),
        ],
        0,
    ).unwrap();

    let results = clipper.clip(poly);
    assert!(results.is_empty());
}

#[test]
fn clip_parallel() {
    let plane: Plane<f32, ()> = Plane {
        normal: vec3(0.0, 0.0, 1.0),
        offset: 0.0,
    };
    let mut clipper = Clipper::new();
    clipper.add(plane);

    let poly = Polygon::from_points(
        [
            point3(-10.0, -10.0, 0.0),
            point3(10.0, -10.0, 0.0),
            point3(10.0, 10.0, 0.0),
            point3(-10.0, 10.0, 0.0),
        ],
        0,
    ).unwrap();

    let results = clipper.clip(poly);
    assert!(results.is_empty());
}

#[test]
fn clip_repeat() {
    let plane: Plane<f32, ()> = Plane::from_unnormalized(vec3(1.0, 0.0, 1.0), 0.0).unwrap().unwrap();
    let mut clipper = Clipper::new();
    clipper.add(plane.clone());
    clipper.add(plane.clone());

    let poly = Polygon::from_points(
        [
            point3(-10.0, -10.0, 0.0),
            point3(10.0, -10.0, 0.0),
            point3(10.0, 10.0, 0.0),
            point3(-10.0, 10.0, 0.0),
        ],
        0,
    ).unwrap();

    let results = clipper.clip(poly);
    assert_eq!(results.len(), 1);
    assert!(plane.signed_distance_sum_to(&results[0]) > 0.0);
}

#[test]
fn clip_transformed() {
    let t_rot: Transform3D<f32, (), ()> =
        Transform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(-FRAC_PI_4));
    let t_div: Transform3D<f32, (), ()> =
        Transform3D::create_perspective(5.0);
    let transform = t_rot.post_transform(&t_div);

    let polygon = Polygon::from_rect(rect(-10.0, -10.0, 20.0, 20.0), 0);
    let bounds: Rect<f32, ()> = rect(-1.0, -1.0, 2.0, 2.0);

    let mut clipper = Clipper::new();
    let results = clipper.clip_transformed(polygon, &transform, Some(bounds));
    // iterating enforces the transformation checks/unwraps
    assert_ne!(0, results.unwrap().count());
}

#[test]
fn clip_badly_transformed() {
    let mut tx = Transform3D::<f32, (), ()>::identity();
    tx.m14 = -0.0000001;
    tx.m44 = 0.0;

    let mut clipper = Clipper::new();
    let polygon = Polygon::from_rect(rect(-10.0, -10.0, 20.0, 20.0), 0);
    let results = clipper.clip_transformed(polygon, &tx, None);
    assert!(results.is_err());
}

#[test]
fn clip_near_coplanar() {
    let tx = Transform3D::<f32, (), ()>::row_major(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        -960.0, -625.0, 1.0, -1.0,
        100.0, -2852.0, 0.0, 1.0,
    );
    let mut clipper = Clipper::new();
    let polygon = Polygon::from_rect(rect(0.0, 0.0, 1703.0, 4020.0), 0);

    let bounds1 = rect(0.0, -430.0, 2048.0, 2048.0);
    let results1 = clipper.clip_transformed(polygon.clone(), &tx, Some(bounds1));
    assert_ne!(0, results1.unwrap().count());

    let bounds2 = rect(0.0, 0.0, 816.0, 1039.0);
    let results2 = clipper.clip_transformed(polygon, &tx, Some(bounds2));
    assert_ne!(0, results2.unwrap().count());
}
