extern crate euclid;
extern crate plane_split;

use euclid::{point3, vec3};
use plane_split::{Clipper, Plane, Polygon};


#[test]
fn clip_in() {
    let plane: Plane<f32, ()> = Plane {
        normal: vec3(1.0, 0.0, 1.0).normalize(),
        offset: 20.0,
    };
    let mut clipper = Clipper::new();
    clipper.add(plane);

    let poly = Polygon::from_points([
        point3(-10.0, -10.0, 0.0),
        point3(10.0, -10.0, 0.0),
        point3(10.0, 10.0, 0.0),
        point3(-10.0, 10.0, 0.0),
    ], 0);

    let results = clipper.clip(poly.clone());
    assert_eq!(results[0], poly);
    assert_eq!(results.len(), 1);
}

#[test]
fn clip_out() {
    let plane: Plane<f32, ()> = Plane {
        normal: vec3(1.0, 0.0, 1.0).normalize(),
        offset: -20.0,
    };
    let mut clipper = Clipper::new();
    clipper.add(plane);

    let poly = Polygon::from_points([
        point3(-10.0, -10.0, 0.0),
        point3(10.0, -10.0, 0.0),
        point3(10.0, 10.0, 0.0),
        point3(-10.0, 10.0, 0.0),
    ], 0);

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

    let poly = Polygon::from_points([
        point3(-10.0, -10.0, 0.0),
        point3(10.0, -10.0, 0.0),
        point3(10.0, 10.0, 0.0),
        point3(-10.0, 10.0, 0.0),
    ], 0);

    let results = clipper.clip(poly);
    assert!(results.is_empty());
}

#[test]
fn clip_repeat() {
    let plane: Plane<f32, ()> = Plane {
        normal: vec3(1.0, 0.0, 1.0).normalize(),
        offset: 0.0,
    };
    let mut clipper = Clipper::new();
    clipper.add(plane.clone());
    clipper.add(plane.clone());

    let poly = Polygon::from_points([
        point3(-10.0, -10.0, 0.0),
        point3(10.0, -10.0, 0.0),
        point3(10.0, 10.0, 0.0),
        point3(-10.0, 10.0, 0.0),
    ], 0);

    let results = clipper.clip(poly);
    assert_eq!(results.len(), 1);
    assert!(plane.signed_distance_sum_to(&results[0]) > 0.0);
}
