extern crate euclid;
extern crate plane_split;

use euclid::{Angle, Rect, Size2D, Transform3D, point2, point3, vec3};
use euclid::approxeq::ApproxEq;
use plane_split::{Intersection, Line, LineProjection, NegativeHemisphereError, Plane, Polygon};


#[test]
fn line_proj_bounds() {
    assert_eq!((-5i8, 4), LineProjection { markers: [-5i8, 1, 4, 2] }.get_bounds());
    assert_eq!((1f32, 4.0), LineProjection { markers: [4f32, 3.0, 2.0, 1.0] }.get_bounds());
}

#[test]
fn valid() {
    let poly_a: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 0.0, 0.0),
            point3(1.0, 1.0, 1.0),
            point3(1.0, 1.0, 0.0),
            point3(0.0, 1.0, 1.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 1.0, 0.0),
            offset: -1.0,
        },
        anchor: 0,
    };
    assert!(!poly_a.is_valid()); // points[0] is outside
    let poly_b: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 1.0, 0.0),
            point3(1.0, 1.0, 1.0),
            point3(1.0, 1.0, 0.0),
            point3(0.0, 1.0, 1.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 1.0, 0.0),
            offset: -1.0,
        },
        anchor: 0,
    };
    assert!(!poly_b.is_valid()); // winding is incorrect
    let poly_c: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 0.0, 1.0),
            point3(1.0, 0.0, 1.0),
            point3(1.0, 1.0, 1.0),
            point3(0.0, 1.0, 1.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 0.0, 1.0),
            offset: -1.0,
        },
        anchor: 0,
    };
    assert!(poly_c.is_valid());
}

#[test]
fn empty() {
    let poly = Polygon::<f32, (), usize>::from_points(
        [
            point3(0.0, 0.0, 1.0),
            point3(0.0, 0.0, 1.0),
            point3(0.0, 0.00001, 1.0),
            point3(1.0, 0.0, 0.0),
        ],
        1,
    );
    assert_eq!(None, poly);
}

fn test_transformed(rect: Rect<f32, ()>, transform: Transform3D<f32, (), ()>) {
    let poly = Polygon::from_transformed_rect(rect, transform, 0).unwrap();
    assert!(poly.is_valid());

    let inv_transform = transform.inverse().unwrap();
    let poly2 = Polygon::from_transformed_rect_with_inverse(rect, &transform, &inv_transform, 0).unwrap();
    assert_eq!(poly.points, poly2.points);
    assert!(poly.plane.offset.approx_eq(&poly2.plane.offset));
    assert!(poly.plane.normal.dot(poly2.plane.normal).approx_eq(&1.0));
}

#[test]
fn from_transformed_rect() {
    let rect = Rect::new(point2(10.0, 10.0), Size2D::new(20.0, 30.0));
    let transform =
        Transform3D::create_rotation(0.5f32.sqrt(), 0.0, 0.5f32.sqrt(), Angle::radians(5.0))
        .pre_translate(vec3(0.0, 0.0, 10.0));
    test_transformed(rect, transform);
}

#[test]
fn from_transformed_rect_perspective() {
    let rect = Rect::new(point2(-10.0, -5.0), Size2D::new(20.0, 30.0));
    let mut transform =
        Transform3D::create_perspective(400.0)
        .pre_translate(vec3(0.0, 0.0, 100.0));
    transform.m44 = 0.7; //for fun
    test_transformed(rect, transform);
}

#[test]
fn untransform_point() {
    let poly: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 0.0, 0.0),
            point3(0.5, 1.0, 0.0),
            point3(1.5, 1.0, 0.0),
            point3(1.0, 0.0, 0.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 1.0, 0.0),
            offset: 0.0,
        },
        anchor: 0,
    };
    assert_eq!(poly.untransform_point(poly.points[0]), point2(0.0, 0.0));
    assert_eq!(poly.untransform_point(poly.points[1]), point2(1.0, 0.0));
    assert_eq!(poly.untransform_point(poly.points[2]), point2(1.0, 1.0));
    assert_eq!(poly.untransform_point(poly.points[3]), point2(0.0, 1.0));
}

#[test]
fn are_outside() {
    let plane: Plane<f32, ()> = Plane {
        normal: vec3(0.0, 0.0, 1.0),
        offset: -1.0,
    };
    assert!(plane.are_outside(&[
        point3(0.0, 0.0, 1.1),
        point3(1.0, 1.0, 2.0),
    ]));
    assert!(plane.are_outside(&[
        point3(0.5, 0.5, 1.0),
    ]));
    assert!(!plane.are_outside(&[
        point3(0.0, 0.0, 1.0),
        point3(0.0, 0.0, -1.0),
    ]));
}

#[test]
fn intersect() {
    let poly_a: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 0.0, 1.0),
            point3(1.0, 0.0, 1.0),
            point3(1.0, 1.0, 1.0),
            point3(0.0, 1.0, 1.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 0.0, 1.0),
            offset: -1.0,
        },
        anchor: 0,
    };
    assert!(poly_a.is_valid());
    let poly_b: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.5, 0.0, 2.0),
            point3(0.5, 1.0, 2.0),
            point3(0.5, 1.0, 0.0),
            point3(0.5, 0.0, 0.0),
        ],
        plane: Plane {
            normal: vec3(1.0, 0.0, 0.0),
            offset: -0.5,
        },
        anchor: 0,
    };
    assert!(poly_b.is_valid());

    let intersection = match poly_a.intersect(&poly_b) {
        Intersection::Inside(result) => result,
        _ => panic!("Bad intersection"),
    };
    assert!(intersection.is_valid());
    // confirm the origin is on both planes
    assert!(poly_a.plane.signed_distance_to(&intersection.origin).approx_eq(&0.0));
    assert!(poly_b.plane.signed_distance_to(&intersection.origin).approx_eq(&0.0));
    // confirm the direction is coplanar to both planes
    assert!(poly_a.plane.normal.dot(intersection.dir).approx_eq(&0.0));
    assert!(poly_b.plane.normal.dot(intersection.dir).approx_eq(&0.0));

    let poly_c: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, -1.0, 2.0),
            point3(0.0, -1.0, 0.0),
            point3(0.0, 0.0, 0.0),
            point3(0.0, 0.0, 2.0),
        ],
        plane: Plane {
            normal: vec3(1.0, 0.0, 0.0),
            offset: 0.0,
        },
        anchor: 0,
    };
    assert!(poly_c.is_valid());
    let poly_d: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 0.0, 0.5),
            point3(1.0, 0.0, 0.5),
            point3(1.0, 1.0, 0.5),
            point3(0.0, 1.0, 0.5),
        ],
        plane: Plane {
            normal: vec3(0.0, 0.0, 1.0),
            offset: -0.5,
        },
        anchor: 0,
    };
    assert!(poly_d.is_valid());

    assert!(poly_a.intersect(&poly_c).is_outside());
    assert!(poly_a.intersect(&poly_d).is_outside());
}

fn test_cut(
    poly_base: &Polygon<f32, (), usize>,
    extra_count: u8,
    line: Line<f32, ()>,
) {
    assert!(line.is_valid());

    let normal = poly_base.plane.normal.cross(line.dir).normalize();
    let mut poly = poly_base.clone();
    let (extra1, extra2) = poly.split_with_normal(&line, &normal);
    assert!(poly.is_valid() && poly_base.contains(&poly));
    assert_eq!(extra_count > 0, extra1.is_some());
    assert_eq!(extra_count > 1, extra2.is_some());
    if let Some(extra) = extra1 {
        assert!(extra.is_valid() && poly_base.contains(&extra));
    }
    if let Some(extra) = extra2 {
        assert!(extra.is_valid() && poly_base.contains(&extra));
    }
}

#[test]
fn split() {
    let poly: Polygon<f32, (), usize> = Polygon {
        points: [
            point3(0.0, 1.0, 0.0),
            point3(1.0, 1.0, 0.0),
            point3(1.0, 1.0, 1.0),
            point3(0.0, 1.0, 1.0),
        ],
        plane: Plane {
            normal: vec3(0.0, 1.0, 0.0),
            offset: -1.0,
        },
        anchor: 0,
    };

    // non-intersecting line
    test_cut(&poly, 0, Line {
        origin: point3(0.0, 1.0, 0.5),
        dir: vec3(0.0, 1.0, 0.0),
    });

    // simple cut (diff=2)
    test_cut(&poly, 1, Line {
        origin: point3(0.0, 1.0, 0.5),
        dir: vec3(1.0, 0.0, 0.0),
    });

    // complex cut (diff=1, wrapped)
    test_cut(&poly, 2, Line {
        origin: point3(0.0, 1.0, 0.5),
        dir: vec3(0.5f32.sqrt(), 0.0, -0.5f32.sqrt()),
    });

    // complex cut (diff=1, non-wrapped)
    test_cut(&poly, 2, Line {
        origin: point3(0.5, 1.0, 0.0),
        dir: vec3(0.5f32.sqrt(), 0.0, 0.5f32.sqrt()),
    });

    // complex cut (diff=3)
    test_cut(&poly, 2, Line {
        origin: point3(0.5, 1.0, 0.0),
        dir: vec3(-0.5f32.sqrt(), 0.0, 0.5f32.sqrt()),
    });

    // perfect diagonal
    test_cut(&poly, 1, Line {
        origin: point3(0.0, 1.0, 0.0),
        dir: vec3(0.5f32.sqrt(), 0.0, 0.5f32.sqrt()),
    });
}

#[test]
fn plane_unnormalized() {
    let zero_vec = vec3(0.0000001, 0.0, 0.0);
    let mut plane: Result<Option<Plane<f32, ()>>, _> = Plane::from_unnormalized(zero_vec, 1.0);
    assert_eq!(plane, Ok(None));
    plane = Plane::from_unnormalized(zero_vec, 0.0);
    assert_eq!(plane, Err(NegativeHemisphereError));
    plane = Plane::from_unnormalized(zero_vec, -0.5);
    assert_eq!(plane, Err(NegativeHemisphereError));

    plane = Plane::from_unnormalized(vec3(-3.0, 4.0, 0.0), 2.0);
    assert_eq!(plane, Ok(Some(Plane {
        normal: vec3(-3.0/5.0, 4.0/5.0, 0.0),
        offset: 2.0/5.0,
    })));
}
