use inplace_it::*;
use std::cmp::Ordering;

#[inline(never)]
fn get_stack_pointer_value() -> usize {
    use std::mem::transmute;

    let begin_of_function_stack: usize = 0;
    unsafe {
        transmute::<&usize, usize>(&begin_of_function_stack)
    }
}

fn calculate_stack_consumption(begin: usize, end: usize) -> usize {
    if begin >= end {
        begin - end
    } else {
        end - begin
    }
}

/// This test measures stack memory consumption from 0 to 4096 items by step 32
/// Then, it calculates "tangent of an angle" (y/x) from "point of zero" (0 items and it's stack size).
///
/// It bad cases, when compiler optimizes `try_inplace_array` function so that it doesn't make sense,
/// difference between lowest and highest borders is huge.
///
/// In good cases, all "points" are form almost a line so all "tangents of an angle" do not have much difference among themselves.
/// And the difference of the borders is small. It's about 0 <= D <= 2.
/// Also, "tangents of an angle" about 0 < T <= 3. In bad cases tangents are VERY HUGE.
///
/// This is what that test checks.
#[test]
fn stack_memory_should_allocate_no_more_than_needed() {
    #[inline(never)]
    fn inplace_and_sum(size: usize) -> usize {
        let begin = get_stack_pointer_value();
        let result = try_inplace_array(size, |mem: UninitializedSliceMemoryGuard<usize>| {
            let end = get_stack_pointer_value();
            let mem = mem.init(|i| i);
            let mut sum = 0usize;
            for i in mem.iter() {
                sum += *i as usize;
            }
            // To sure sum operation was not optimized to no-op
            let len = mem.len();
            assert_eq!(sum, if len > 0 {
                len * (len - 1) / 2
            } else {
                0
            });
            let stack_consumption = calculate_stack_consumption(begin, end);
            stack_consumption
        });
        match result {
            Ok(result) => result,
            Err(_) => panic!("Inplace should never fail is this test"),
        }
    }

    fn calc_differential_coefficients(mut data: impl Iterator<Item = (usize, usize)>) -> impl Iterator<Item = f64> {
        let (zero_x, zero_y) = data.next()
            .expect("Input iterator should not be empty");

        data.map(move |(x, y)| {
            let dx = (x - zero_x) as f64;
            let dy = (y - zero_y) as f64;
            dy / dx
        })
    }

    fn calc_differentials_borders_dispersion(data: &[f64]) -> f64 {
        let min = data.iter().min_by(|a, b| a.partial_cmp(b).unwrap_or(Ordering::Equal))
            .expect("Input dataset should not be empty");
        let max = data.iter().max_by(|a, b| a.partial_cmp(b).unwrap_or(Ordering::Equal))
            .expect("Input dataset should not be empty");
        max - min
    }

    let stack_sizes = (0..=4096).step_by(32)
        .map(|length| {
            let stack_size = inplace_and_sum(length);
            const USIZE_LAYOUT: usize = std::mem::size_of::<usize>() + std::mem::align_of::<usize>(); // usize layout coefficient
            let stack_size = stack_size / USIZE_LAYOUT;
            (length, stack_size)
        })
        .collect::<Vec<_>>();

    // dbg!(&stack_sizes);

    let stack_sizes_differentials = calc_differential_coefficients(stack_sizes.into_iter())
        .collect::<Vec<_>>();

    for differential in &stack_sizes_differentials {
        assert!(*differential <= 3.0);
    }

    let differentials_borders_dispersion = calc_differentials_borders_dispersion(&stack_sizes_differentials);

    // dbg!(stack_sizes_differentials, differentials_borders_dispersion);

    assert!(differentials_borders_dispersion <= 2.0);
}
