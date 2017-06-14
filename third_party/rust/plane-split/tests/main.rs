extern crate euclid;
extern crate plane_split;

use euclid::{Point2D, Radians, TypedPoint2D, TypedPoint3D, TypedRect, TypedSize2D, TypedMatrix4D};
use euclid::approxeq::ApproxEq;
use plane_split::{Intersection, Line, LineProjection, Polygon};


#[test]
fn line_proj_bounds() {
    assert_eq!((-5i8, 4), LineProjection { markers: [-5i8, 1, 4, 2] }.get_bounds());
    assert_eq!((1f32, 4.0), LineProjection { markers: [4f32, 3.0, 2.0, 1.0] }.get_bounds());
}

#[test]
fn valid() {
    let poly_a: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 0.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(1.0, 1.0, 0.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 1.0, 0.0),
        offset: -1.0,
        anchor: 0,
    };
    assert!(!poly_a.is_valid()); // points[0] is outside
    let poly_b: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 1.0, 0.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(1.0, 1.0, 0.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 1.0, 0.0),
        offset: -1.0,
        anchor: 0,
    };
    assert!(!poly_b.is_valid()); // winding is incorrect
    let poly_c: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 0.0, 1.0),
        offset: -1.0,
        anchor: 0,
    };
    assert!(poly_c.is_valid());
}

#[test]
fn from_transformed_rect() {
    let rect: TypedRect<f32, ()> = TypedRect::new(TypedPoint2D::new(10.0, 10.0), TypedSize2D::new(20.0, 30.0));
    let transform: TypedMatrix4D<f32, (), ()> =
        TypedMatrix4D::create_rotation(0.5f32.sqrt(), 0.0, 0.5f32.sqrt(), Radians::new(5.0))
        .pre_translated(0.0, 0.0, 10.0);
    let poly = Polygon::from_transformed_rect(rect, transform, 0);
    assert!(poly.is_valid());
}

#[test]
fn untransform_point() {
    let poly: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 0.0),
            TypedPoint3D::new(0.5, 1.0, 0.0),
            TypedPoint3D::new(1.5, 1.0, 0.0),
            TypedPoint3D::new(1.0, 0.0, 0.0),
        ],
        normal: TypedPoint3D::new(0.0, 1.0, 0.0),
        offset: 0.0,
        anchor: 0,
    };
    assert_eq!(poly.untransform_point(poly.points[0]), Point2D::new(0.0, 0.0));
    assert_eq!(poly.untransform_point(poly.points[1]), Point2D::new(1.0, 0.0));
    assert_eq!(poly.untransform_point(poly.points[2]), Point2D::new(1.0, 1.0));
    assert_eq!(poly.untransform_point(poly.points[3]), Point2D::new(0.0, 1.0));
}

#[test]
fn are_outside() {
    let poly: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 0.0, 1.0),
        offset: -1.0,
        anchor: 0,
    };
    assert!(poly.is_valid());
    assert!(poly.are_outside(&[
        TypedPoint3D::new(0.0, 0.0, 1.1),
        TypedPoint3D::new(1.0, 1.0, 2.0),
    ]));
    assert!(poly.are_outside(&[
        TypedPoint3D::new(0.5, 0.5, 1.0),
    ]));
    assert!(!poly.are_outside(&[
        TypedPoint3D::new(0.0, 0.0, 1.0),
        TypedPoint3D::new(0.0, 0.0, -1.0),
    ]));
}

#[test]
fn intersect() {
    let poly_a: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 0.0, 1.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 0.0, 1.0),
        offset: -1.0,
        anchor: 0,
    };
    assert!(poly_a.is_valid());
    let poly_b: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.5, 0.0, 2.0),
            TypedPoint3D::new(0.5, 1.0, 2.0),
            TypedPoint3D::new(0.5, 1.0, 0.0),
            TypedPoint3D::new(0.5, 0.0, 0.0),
        ],
        normal: TypedPoint3D::new(1.0, 0.0, 0.0),
        offset: -0.5,
        anchor: 0,
    };
    assert!(poly_b.is_valid());

    let intersection = match poly_a.intersect(&poly_b) {
        Intersection::Inside(result) => result,
        _ => panic!("Bad intersection"),
    };
    assert!(intersection.is_valid());
    // confirm the origin is on both planes
    assert!(poly_a.signed_distance_to(&intersection.origin).approx_eq(&0.0));
    assert!(poly_b.signed_distance_to(&intersection.origin).approx_eq(&0.0));
    // confirm the direction is coplanar to both planes
    assert!(poly_a.normal.dot(intersection.dir).approx_eq(&0.0));
    assert!(poly_b.normal.dot(intersection.dir).approx_eq(&0.0));

    let poly_c: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, -1.0, 2.0),
            TypedPoint3D::new(0.0, -1.0, 0.0),
            TypedPoint3D::new(0.0, 0.0, 0.0),
            TypedPoint3D::new(0.0, 0.0, 2.0),
        ],
        normal: TypedPoint3D::new(1.0, 0.0, 0.0),
        offset: 0.0,
        anchor: 0,
    };
    assert!(poly_c.is_valid());
    let poly_d: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 0.0, 0.5),
            TypedPoint3D::new(1.0, 0.0, 0.5),
            TypedPoint3D::new(1.0, 1.0, 0.5),
            TypedPoint3D::new(0.0, 1.0, 0.5),
        ],
        normal: TypedPoint3D::new(0.0, 0.0, 1.0),
        offset: -0.5,
        anchor: 0,
    };
    assert!(poly_d.is_valid());

    assert!(poly_a.intersect(&poly_c).is_outside());
    assert!(poly_a.intersect(&poly_d).is_outside());
}

fn test_cut(poly_base: &Polygon<f32, ()>, extra_count: u8, line: Line<f32, ()>) {
    assert!(line.is_valid());
    let mut poly = poly_base.clone();
    let (extra1, extra2) = poly.split(&line);
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
    let poly: Polygon<f32, ()> = Polygon {
        points: [
            TypedPoint3D::new(0.0, 1.0, 0.0),
            TypedPoint3D::new(1.0, 1.0, 0.0),
            TypedPoint3D::new(1.0, 1.0, 1.0),
            TypedPoint3D::new(0.0, 1.0, 1.0),
        ],
        normal: TypedPoint3D::new(0.0, 1.0, 0.0),
        offset: -1.0,
        anchor: 0,
    };

    // non-intersecting line
    test_cut(&poly, 0, Line {
        origin: TypedPoint3D::new(0.0, 1.0, 0.5),
        dir: TypedPoint3D::new(0.0, 1.0, 0.0),
    });

    // simple cut (diff=2)
    test_cut(&poly, 1, Line {
        origin: TypedPoint3D::new(0.0, 1.0, 0.5),
        dir: TypedPoint3D::new(1.0, 0.0, 0.0),
    });

    // complex cut (diff=1, wrapped)
    test_cut(&poly, 2, Line {
        origin: TypedPoint3D::new(0.0, 1.0, 0.5),
        dir: TypedPoint3D::new(0.5f32.sqrt(), 0.0, -0.5f32.sqrt()),
    });

    // complex cut (diff=1, non-wrapped)
    test_cut(&poly, 2, Line {
        origin: TypedPoint3D::new(0.5, 1.0, 0.0),
        dir: TypedPoint3D::new(0.5f32.sqrt(), 0.0, 0.5f32.sqrt()),
    });

    // complex cut (diff=3)
    test_cut(&poly, 2, Line {
        origin: TypedPoint3D::new(0.5, 1.0, 0.0),
        dir: TypedPoint3D::new(-0.5f32.sqrt(), 0.0, 0.5f32.sqrt()),
    });
}
