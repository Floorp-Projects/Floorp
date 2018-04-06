use std::cmp;

/// A fragment of a computed diff.
#[derive(Clone, Debug, PartialEq)]
pub enum Result<T> {
    Left(T),
    Both(T, T),
    Right(T),
}

/// Computes the diff between two slices.
pub fn slice<'a, T: PartialEq>(left: &'a [T], right: &'a [T]) -> Vec<Result<&'a T>> {
    iter(left.iter(), right.iter())
}

/// Computes the diff between the lines of two strings.
pub fn lines<'a>(left: &'a str, right: &'a str) -> Vec<Result<&'a str>> {
    let mut diff = iter(left.lines(), right.lines());
    // str::lines() does not yield an empty str at the end if the str ends with
    // '\n'. We handle this special case by inserting one last diff item,
    // depending on whether the left string ends with '\n', or the right one,
    // or both.
    match (
        left.as_bytes().last().cloned(),
        right.as_bytes().last().cloned(),
    ) {
        (Some(b'\n'), Some(b'\n')) => {
            diff.push(Result::Both(&left[left.len()..], &right[right.len()..]))
        }
        (Some(b'\n'), _) => diff.push(Result::Left(&left[left.len()..])),
        (_, Some(b'\n')) => diff.push(Result::Right(&right[right.len()..])),
        _ => {}
    }
    diff
}

/// Computes the diff between the chars of two strings.
pub fn chars<'a>(left: &'a str, right: &'a str) -> Vec<Result<char>> {
    iter(left.chars(), right.chars())
}

fn iter<I, T>(left: I, right: I) -> Vec<Result<T>>
where
    I: Clone + Iterator<Item = T> + DoubleEndedIterator,
    T: PartialEq,
{
    let left_count = left.clone().count();
    let right_count = right.clone().count();
    let min_count = cmp::min(left_count, right_count);

    let leading_equals = left.clone()
        .zip(right.clone())
        .take_while(|p| p.0 == p.1)
        .count();
    let trailing_equals = left.clone()
        .rev()
        .zip(right.clone().rev())
        .take(min_count - leading_equals)
        .take_while(|p| p.0 == p.1)
        .count();

    let left_diff_size = left_count - leading_equals - trailing_equals;
    let right_diff_size = right_count - leading_equals - trailing_equals;

    let table: Vec<Vec<u32>> = {
        let mut table = vec![vec![0; right_diff_size + 1]; left_diff_size + 1];
        let left_skip = left.clone().skip(leading_equals).take(left_diff_size);
        let right_skip = right.clone().skip(leading_equals).take(right_diff_size);

        for (i, l) in left_skip.clone().enumerate() {
            for (j, r) in right_skip.clone().enumerate() {
                table[i + 1][j + 1] = if l == r {
                    table[i][j] + 1
                } else {
                    std::cmp::max(table[i][j + 1], table[i + 1][j])
                };
            }
        }

        table
    };

    let diff = {
        let mut diff = Vec::with_capacity(left_diff_size + right_diff_size);
        let mut i = left_diff_size;
        let mut j = right_diff_size;
        let mut li = left.clone().rev().skip(trailing_equals);
        let mut ri = right.clone().rev().skip(trailing_equals);

        loop {
            if j > 0 && (i == 0 || table[i][j] == table[i][j - 1]) {
                j -= 1;
                diff.push(Result::Right(ri.next().unwrap()));
            } else if i > 0 && (j == 0 || table[i][j] == table[i - 1][j]) {
                i -= 1;
                diff.push(Result::Left(li.next().unwrap()));
            } else if i > 0 && j > 0 {
                i -= 1;
                j -= 1;
                diff.push(Result::Both(li.next().unwrap(), ri.next().unwrap()));
            } else {
                break;
            }
        }

        diff
    };

    let diff_size = leading_equals + diff.len() + trailing_equals;
    let mut total_diff = Vec::with_capacity(diff_size);

    total_diff.extend(
        left.clone()
            .zip(right.clone())
            .take(leading_equals)
            .map(|(l, r)| Result::Both(l, r)),
    );
    total_diff.extend(diff.into_iter().rev());
    total_diff.extend(
        left.skip(leading_equals + left_diff_size)
            .zip(right.skip(leading_equals + right_diff_size))
            .map(|(l, r)| Result::Both(l, r)),
    );

    total_diff
}
