use std::{fmt, ops};
use std::cmp::Ordering;
use {Intersection, Line, Polygon, Splitter};
use euclid::TypedPoint3D;
use euclid::approxeq::ApproxEq;
use num_traits::{Float, One, Zero};


/// Naive plane splitter, has at least O(n^2) complexity.
pub struct NaiveSplitter<T, U> {
    result: Vec<Polygon<T, U>>,
    current: Vec<Polygon<T, U>>,
    temp: Vec<Polygon<T, U>>,
}

impl<T, U> NaiveSplitter<T, U> {
    /// Create a new `NaiveSplitter`.
    pub fn new() -> Self {
        NaiveSplitter {
            result: Vec::new(),
            current: Vec::new(),
            temp: Vec::new(),
        }
    }
}

/// Find a closest intersection point between two polygons,
/// across the specified direction.
fn intersect_across<T, U>(a: &Polygon<T, U>, b: &Polygon<T, U>,
                          dir: TypedPoint3D<T, U>)
                          -> TypedPoint3D<T, U>
where
    T: Copy + fmt::Debug + PartialOrd + ApproxEq<T> +
        ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
        ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
        Zero + One + Float,
    U: fmt::Debug,
{
    let pa = a.project_on(&dir).get_bounds();
    let pb = b.project_on(&dir).get_bounds();
    let pmin = pa.0.max(pb.0);
    let pmax = pa.1.min(pb.1);
    let k = (pmin + pmax) / (T::one() + T::one());
    debug!("\t\tIntersection pa {:?} pb {:?} k {:?}", pa, pb, k);
    dir * k
}

fn partial_sort_by<T, F>(array: &mut [T], fun: F) where
    F: Fn(&T, &T) -> Ordering,
    T: fmt::Debug,
{
    debug!("Sorting");
    if array.is_empty() {
        return
    }
    for i in 0 .. array.len() - 1 {
        let mut up_start = array.len();
        // placement is: [i, ... equals ..., up_start, ... greater ..., j]
        // if this condition fails, everything to the right is greater
        'find_smallest: while i + 1 != up_start {
            let mut j = i + 1;
            'partition: loop {
                debug!("\tComparing {} to {}, up_start = {}", i, j, up_start);
                let order = fun(&array[i], &array[j]);
                debug!("\t\t{:?}", order);
                match order {
                    Ordering::Less => {
                        // push back to "greater" area
                        up_start -= 1;
                        if j == up_start {
                            break 'partition
                        }
                        array.swap(j, up_start);
                    },
                    Ordering::Equal => {
                        // continue
                        j += 1;
                        if j == up_start {
                            // we reached the end of the "equal" area
                            // so our "i" can be placed anywhere
                            break 'find_smallest;
                        }
                    }
                    Ordering::Greater => {
                        array.swap(i, j);
                        up_start -= 1;
                        array.swap(j, up_start);
                        // found a smaller one, push "i" to the "greater" area
                        // and restart the search from the current element
                        break 'partition;
                    },
                }
            }
        }
        debug!("\tEnding {} with up_start={}, poly {:?}", i, up_start, array[i]);
    }
}


impl<
    T: Copy + fmt::Debug + PartialOrd + ApproxEq<T> +
       ops::Sub<T, Output=T> + ops::Add<T, Output=T> +
       ops::Mul<T, Output=T> + ops::Div<T, Output=T> +
       Zero + One + Float,
    U: fmt::Debug,
> Splitter<T, U> for NaiveSplitter<T, U> {
    fn reset(&mut self) {
        self.result.clear();
        self.current.clear();
        self.temp.clear();
    }

    fn add(&mut self, poly: Polygon<T, U>) {
        // "current" accumulates all the subdivisions of the originally
        // added polygon
        self.current.push(poly);
        for old in self.result.iter() {
            for new in self.current.iter_mut() {
                // temp accumulates all the new subdivisions to be added
                // to the current, since we can't modify it in place
                if let Intersection::Inside(line) = old.intersect(new) {
                    let (res_add1, res_add2) = new.split(&line);
                    if let Some(res) = res_add1 {
                        self.temp.push(res);
                    }
                    if let Some(res) = res_add2 {
                        self.temp.push(res);
                    }
                }
            }
            self.current.extend(self.temp.drain(..));
        }
        let index = self.result.len();
        self.result.extend(self.current.drain(..));
        debug!("Split result: {:?}", &self.result[index..]);
    }

    //TODO: verify/prove that the sorting approach is consistent
    fn sort(&mut self, view: TypedPoint3D<T, U>) -> &[Polygon<T, U>] {
        // choose the most perpendicular axis among these two
        let axis_pre = {
            let axis_pre0 = TypedPoint3D::new(T::one(), T::zero(), T::zero());
            let axis_pre1 = TypedPoint3D::new(T::zero(), T::one(), T::zero());
            if view.dot(axis_pre0).abs() < view.dot(axis_pre1).abs() {
                axis_pre0
            } else {
                axis_pre1
            }
        };
        // do the orthogonalization
        let axis_x = view.cross(axis_pre);
        let axis_y = view.cross(axis_x);
        debug!("Chosen axis {:?} {:?}", axis_x, axis_y);
        // sort everything
        partial_sort_by(&mut self.result, |a, b| {
            debug!("\t\t{:?}\n\t\t{:?}", a, b);
            //TODO: proper intersection
            // compute the origin
            let comp_x = intersect_across(a, b, axis_x);
            let comp_y = intersect_across(a, b, axis_y);
            // line that tries to intersect both
            let line = Line {
                origin: comp_x + comp_y,
                dir: view,
            };
            debug!("\t\tGot {:?}", line);
            // distances across the line
            let da = a.distance_to_line(&line);
            let db = b.distance_to_line(&line);
            debug!("\t\tDistances {:?} {:?}", da, db);
            // final compare
            da.partial_cmp(&db).unwrap_or(Ordering::Equal)
        });
        // done
        &self.result
    }
}
