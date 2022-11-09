use euclid::{
    default::{Rect, Transform3D},
    rect, vec3, Angle,
};
use plane_split::PlaneCut;
use plane_split::{make_grid, BspSplitter, Polygon};
use std::f64::consts::FRAC_PI_4;

fn grid_impl(count: usize, splitter: &mut BspSplitter<usize>) {
    let polys = make_grid(count);
    let result = splitter.solve(&polys, vec3(0.0, 0.0, 1.0));
    assert_eq!(result.len(), count + count * count + count * count * count);
}

#[test]
fn grid_bsp() {
    grid_impl(2, &mut BspSplitter::new());
}

fn sort_rotation(splitter: &mut BspSplitter<usize>) {
    let transform0: Transform3D<f64> =
        Transform3D::rotation(0.0, 1.0, 0.0, Angle::radians(-FRAC_PI_4));
    let transform1: Transform3D<f64> = Transform3D::rotation(0.0, 1.0, 0.0, Angle::radians(0.0));
    let transform2: Transform3D<f64> =
        Transform3D::rotation(0.0, 1.0, 0.0, Angle::radians(FRAC_PI_4));

    let rect: Rect<f64> = rect(-10.0, -10.0, 20.0, 20.0);
    let p1 = Polygon::from_transformed_rect(rect, transform0, 0);
    let p2 = Polygon::from_transformed_rect(rect, transform1, 1);
    let p3 = Polygon::from_transformed_rect(rect, transform2, 2);
    assert!(
        p1.is_some() && p2.is_some() && p3.is_some(),
        "Cannot construct transformed polygons"
    );

    let polys = [p1.unwrap(), p2.unwrap(), p3.unwrap()];
    let result = splitter.solve(&polys, vec3(0.0, 0.0, -1.0));
    let ids: Vec<_> = result.iter().map(|poly| poly.anchor).collect();
    assert_eq!(&ids, &[2, 1, 0, 1, 2]);
}

#[test]
fn rotation_bsp() {
    sort_rotation(&mut BspSplitter::new());
}

fn sort_trivial(splitter: &mut BspSplitter<usize>) {
    let anchors: Vec<_> = (0usize..10).collect();
    let rect: Rect<f64> = rect(-10.0, -10.0, 20.0, 20.0);
    let polys: Vec<_> = anchors
        .iter()
        .map(|&anchor| {
            let transform: Transform3D<f64> = Transform3D::translation(0.0, 0.0, anchor as f64);
            let poly = Polygon::from_transformed_rect(rect, transform, anchor);
            assert!(poly.is_some(), "Cannot construct transformed polygons");
            poly.unwrap()
        })
        .collect();

    let result = splitter.solve(&polys, vec3(0.0, 0.0, -1.0));
    let anchors1: Vec<_> = result.iter().map(|p| p.anchor).collect();
    let mut anchors2 = anchors1.clone();
    anchors2.sort_by_key(|&a| -(a as i32));
    //make sure Z is sorted backwards
    assert_eq!(anchors1, anchors2);
}

fn sort_external(splitter: &mut BspSplitter<usize>) {
    let rect0: Rect<f64> = rect(-10.0, -10.0, 20.0, 20.0);
    let poly0 = Polygon::from_rect(rect0, 0);
    let poly1 = {
        let transform0: Transform3D<f64> =
            Transform3D::rotation(1.0, 0.0, 0.0, Angle::radians(2.0 * FRAC_PI_4));
        let transform1: Transform3D<f64> = Transform3D::translation(0.0, 100.0, 0.0);
        Polygon::from_transformed_rect(rect0, transform0.then(&transform1), 1).unwrap()
    };

    let result = splitter.solve(&[poly0, poly1], vec3(1.0, 1.0, 0.0).normalize());
    let anchors: Vec<_> = result.iter().map(|p| p.anchor).collect();
    // make sure the second polygon is split in half around the plane of the first one,
    // even if geometrically their polygons don't intersect.
    assert_eq!(anchors, vec![1, 0, 1]);
}

#[test]
fn trivial_bsp() {
    sort_trivial(&mut BspSplitter::new());
}

#[test]
fn external_bsp() {
    sort_external(&mut BspSplitter::new());
}

#[test]
fn test_cut() {
    use smallvec::SmallVec;
    let rect: Rect<f64> = rect(-10.0, -10.0, 20.0, 20.0);
    let poly = Polygon::from_rect(rect, 0);
    let mut poly2 = Polygon::from_rect(rect, 0);
    // test robustness for positions
    for p in &mut poly2.points {
        p.z += 0.00000001;
    }

    let mut front: SmallVec<[Polygon<i32>; 2]> = SmallVec::new();
    let mut back: SmallVec<[Polygon<i32>; 2]> = SmallVec::new();

    assert_eq!(poly.cut(&poly2, &mut front, &mut back), PlaneCut::Sibling);
    assert!(front.is_empty());
    assert!(back.is_empty());

    // test robustness for normal
    poly2.plane.normal.z += 0.00000001;
    assert_eq!(poly.cut(&poly2, &mut front, &mut back), PlaneCut::Sibling);
    assert!(front.is_empty());
    assert!(back.is_empty());

    // test opposite normal handling
    poly2.plane.normal *= -1.0;
    assert_eq!(poly.cut(&poly2, &mut front, &mut back), PlaneCut::Sibling);
    assert!(front.is_empty());
    assert!(back.is_empty());

    // test grouping front
    poly2.plane.offset += 0.1;
    assert_eq!(poly.cut(&poly2, &mut front, &mut back), PlaneCut::Cut);
    assert_eq!(front.len(), 1);
    assert!(back.is_empty());

    front.clear();

    // test grouping back
    poly2.plane.normal *= -1.0;
    assert_eq!(poly.cut(&poly2, &mut front, &mut back), PlaneCut::Cut);
    assert_eq!(back.len(), 1);
    assert!(front.is_empty());
}
