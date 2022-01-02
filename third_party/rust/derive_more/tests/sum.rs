#[macro_use]
extern crate derive_more;

#[derive(Sum)]
struct MyInts(i32, i64);

// Add implementation is needed for Sum
impl ::core::ops::Add for MyInts {
    type Output = MyInts;
    #[inline]
    fn add(self, rhs: MyInts) -> MyInts {
        MyInts(self.0.add(rhs.0), self.1.add(rhs.1))
    }
}

#[derive(Sum)]
struct Point2D {
    x: i32,
    y: i32,
}

impl ::core::ops::Add for Point2D {
    type Output = Point2D;
    #[inline]
    fn add(self, rhs: Point2D) -> Point2D {
        Point2D {
            x: self.x.add(rhs.x),
            y: self.y.add(rhs.y),
        }
    }
}
