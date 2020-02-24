use std::iter::Iterator;
use std::time::Duration;

use num_rational::Ratio;

use crate::buffer::RgbaImage;
use crate::error::ImageResult;

/// An implementation dependent iterator, reading the frames as requested
pub struct Frames<'a> {
    iterator: Box<dyn Iterator<Item = ImageResult<Frame>> + 'a>
}

impl<'a> Frames<'a> {
    /// Creates a new `Frames` from an implementation specific iterator.
    pub fn new(iterator: Box<dyn Iterator<Item = ImageResult<Frame>> + 'a>) -> Self {
        Frames { iterator }
    }

    /// Steps through the iterator from the current frame until the end and pushes each frame into
    /// a `Vec`.
    /// If en error is encountered that error is returned instead.
    ///
    /// Note: This is equivalent to `Frames::collect::<ImageResult<Vec<Frame>>>()`
    pub fn collect_frames(self) -> ImageResult<Vec<Frame>> {
        self.collect()
    }
}

impl<'a> Iterator for Frames<'a> {
    type Item = ImageResult<Frame>;
    fn next(&mut self) -> Option<ImageResult<Frame>> {
        self.iterator.next()
    }
}

/// A single animation frame
#[derive(Clone)]
pub struct Frame {
    /// Delay between the frames in milliseconds
    delay: Delay,
    /// x offset
    left: u32,
    /// y offset
    top: u32,
    buffer: RgbaImage,
}

/// The delay of a frame relative to the previous one.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd)]
pub struct Delay {
    ratio: Ratio<u32>,
}

impl Frame {
    /// Contructs a new frame without any delay.
    pub fn new(buffer: RgbaImage) -> Frame {
        Frame {
            delay: Delay::from_ratio(Ratio::from_integer(0)),
            left: 0,
            top: 0,
            buffer,
        }
    }

    /// Contructs a new frame
    pub fn from_parts(buffer: RgbaImage, left: u32, top: u32, delay: Delay) -> Frame {
        Frame {
            delay,
            left,
            top,
            buffer,
        }
    }

    /// Delay of this frame
    pub fn delay(&self) -> Delay {
        self.delay
    }

    /// Returns the image buffer
    pub fn buffer(&self) -> &RgbaImage {
        &self.buffer
    }

    /// Returns the image buffer
    pub fn into_buffer(self) -> RgbaImage {
        self.buffer
    }

    /// Returns the x offset
    pub fn left(&self) -> u32 {
        self.left
    }

    /// Returns the y offset
    pub fn top(&self) -> u32 {
        self.top
    }
}

impl Delay {
    /// Create a delay from a ratio of milliseconds.
    ///
    /// # Examples
    ///
    /// ```
    /// use image::Delay;
    /// let delay_10ms = Delay::from_numer_denom_ms(10, 1);
    /// ```
    pub fn from_numer_denom_ms(numerator: u32, denominator: u32) -> Self {
        Delay { ratio: Ratio::new_raw(numerator, denominator) }
    }

    /// Convert from a duration, clamped between 0 and an implemented defined maximum.
    ///
    /// The maximum is *at least* `i32::MAX` milliseconds. It should be noted that the accuracy of
    /// the result may be relative and very large delays have a coarse resolution.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::time::Duration;
    /// use image::Delay;
    ///
    /// let duration = Duration::from_millis(20);
    /// let delay = Delay::from_saturating_duration(duration);
    /// ```
    pub fn from_saturating_duration(duration: Duration) -> Self {
        // A few notes: The largest number we can represent as a ratio is u32::MAX but we can
        // sometimes represent much smaller numbers.
        //
        // We can represent duration as `millis+a/b` (where a < b, b > 0).
        // We must thus bound b with `b·millis + (b-1) <= u32::MAX` or
        // > `0 < b <= (u32::MAX + 1)/(millis + 1)`
        // Corollary: millis <= u32::MAX

        const MILLIS_BOUND: u128 = u32::max_value() as u128;

        let millis = duration.as_millis().min(MILLIS_BOUND);
        let submillis = (duration.as_nanos() % 1_000_000) as u32;

        let max_b = if millis > 0 {
            ((MILLIS_BOUND + 1)/(millis + 1)) as u32
        } else {
            MILLIS_BOUND as u32
        };
        let millis = millis as u32;

        let (a, b) = Self::closest_bounded_fraction(max_b, submillis, 1_000_000);
        Self::from_numer_denom_ms(a + b*millis, b)
    }

    /// The numerator and denominator of the delay in milliseconds.
    ///
    /// This is guaranteed to be an exact conversion if the `Delay` was previously created with the
    /// `from_numer_denom_ms` constructor.
    pub fn numer_denom_ms(self) -> (u32, u32) {
        (*self.ratio.numer(), *self.ratio.denom())
    }

    pub(crate) fn from_ratio(ratio: Ratio<u32>) -> Self {
        Delay { ratio }
    }

    pub(crate) fn into_ratio(self) -> Ratio<u32> {
        self.ratio
    }

    /// Given some fraction, compute an approximation with denominator bounded.
    ///
    /// Note that `denom_bound` bounds nominator and denominator of all intermediate
    /// approximations and the end result.
    fn closest_bounded_fraction(denom_bound: u32, nom: u32, denom: u32) -> (u32, u32) {
        use std::cmp::Ordering::{self, *};
        assert!(0 < denom);
        assert!(0 < denom_bound);
        assert!(nom < denom);

        // Avoid a few type troubles. All intermediate results are bounded by `denom_bound` which
        // is in turn bounded by u32::MAX. Representing with u64 allows multiplication of any two
        // values without fears of overflow.

        // Compare two fractions whose parts fit into a u32.
        fn compare_fraction((an, ad): (u64, u64), (bn, bd): (u64, u64)) -> Ordering {
            (an*bd).cmp(&(bn*ad))
        }

        // Computes the nominator of the absolute difference between two such fractions.
        fn abs_diff_nom((an, ad): (u64, u64), (bn, bd): (u64, u64)) -> u64 {
            let c0 = an*bd;
            let c1 = ad*bn;

            let d0 = c0.max(c1);
            let d1 = c0.min(c1);
            d0 - d1
        }

        let exact = (u64::from(nom), u64::from(denom));
        // The lower bound fraction, numerator and denominator.
        let mut lower = (0u64, 1u64);
        // The upper bound fraction, numerator and denominator.
        let mut upper = (1u64, 1u64);
        // The closest approximation for now.
        let mut guess = (u64::from(nom*2 > denom), 1u64);

        // loop invariant: ad, bd <= denom_bound
        // iterates the Farey sequence.
        loop {
            // Break if we are done.
            if compare_fraction(guess, exact) == Equal {
                break;
            }

            // Break if next Farey number is out-of-range.
            if u64::from(denom_bound) - lower.1 < upper.1 {
                break;
            }

            // Next Farey approximation n between a and b
            let next = (lower.0 + upper.0, lower.1 + upper.1);
            // if F < n then replace the upper bound, else replace lower.
            if compare_fraction(exact, next) == Less {
                upper = next;
            } else {
                lower = next;
            }

            // Now correct the closest guess.
            // In other words, if |c - f| > |n - f| then replace it with the new guess.
            // This favors the guess with smaller denominator on equality.

            // |g - f| = |g_diff_nom|/(gd*fd);
            let g_diff_nom = abs_diff_nom(guess, exact);
            // |n - f| = |n_diff_nom|/(nd*fd);
            let n_diff_nom = abs_diff_nom(next, exact);

            // The difference |n - f| is smaller than |g - f| if either the integral part of the
            // fraction |n_diff_nom|/nd is smaller than the one of |g_diff_nom|/gd or if they are
            // the same but the fractional part is larger.
            if match (n_diff_nom/next.1).cmp(&(g_diff_nom/guess.1)) {
                Less => true,
                Greater => false,
                // Note that the nominator for the fractional part is smaller than its denominator
                // which is smaller than u32 and can't overflow the multiplication with the other
                // denominator, that is we can compare these fractions by multiplication with the
                // respective other denominator.
                Equal => compare_fraction((n_diff_nom%next.1, next.1), (g_diff_nom%guess.1, guess.1)) == Less,
            } {
                guess = next;
            }
        }

        (guess.0 as u32, guess.1 as u32)
    }
}

impl From<Delay> for Duration {
    fn from(delay: Delay) -> Self {
        let ratio = delay.into_ratio();
        let ms = ratio.to_integer();
        let rest = ratio.numer() % ratio.denom();
        let nanos = (u64::from(rest) * 1_000_000) / u64::from(*ratio.denom());
        Duration::from_millis(ms.into()) + Duration::from_nanos(nanos)
    }
}

#[cfg(test)]
mod tests {
    use super::{Delay, Duration, Ratio};

    #[test]
    fn simple() {
        let second = Delay::from_numer_denom_ms(1000, 1);
        assert_eq!(Duration::from(second), Duration::from_secs(1));
    }

    #[test]
    fn fps_30() {
        let thirtieth = Delay::from_numer_denom_ms(1000, 30);
        let duration = Duration::from(thirtieth);
        assert_eq!(duration.as_secs(), 0);
        assert_eq!(duration.subsec_millis(), 33);
        assert_eq!(duration.subsec_nanos(), 33_333_333);
    }

    #[test]
    fn duration_outlier() {
        let oob = Duration::from_secs(0xFFFF_FFFF);
        let delay = Delay::from_saturating_duration(oob);
        assert_eq!(delay.numer_denom_ms(), (0xFFFF_FFFF, 1));
    }

    #[test]
    fn duration_approx() {
        let oob = Duration::from_millis(0xFFFF_FFFF) + Duration::from_micros(1);
        let delay = Delay::from_saturating_duration(oob);
        assert_eq!(delay.numer_denom_ms(), (0xFFFF_FFFF, 1));

        let inbounds = Duration::from_millis(0xFFFF_FFFF) - Duration::from_micros(1);
        let delay = Delay::from_saturating_duration(inbounds);
        assert_eq!(delay.numer_denom_ms(), (0xFFFF_FFFF, 1));

        let fine = Duration::from_millis(0xFFFF_FFFF/1000) + Duration::from_micros(0xFFFF_FFFF%1000);
        let delay = Delay::from_saturating_duration(fine);
        // Funnily, 0xFFFF_FFFF is divisble by 5, thus we compare with a `Ratio`.
        assert_eq!(delay.into_ratio(), Ratio::new(0xFFFF_FFFF, 1000));
    }

    #[test]
    fn precise() {
        // The ratio has only 32 bits in the numerator, too imprecise to get more than 11 digits
        // correct. But it may be expressed as 1_000_000/3 instead.
        let exceed = Duration::from_secs(333) + Duration::from_nanos(333_333_333);
        let delay = Delay::from_saturating_duration(exceed);
        assert_eq!(Duration::from(delay), exceed);
    }


    #[test]
    fn small() {
        // Not quite a delay of `1 ms`.
        let delay = Delay::from_numer_denom_ms(1 << 16, (1 << 16) + 1);
        let duration = Duration::from(delay);
        assert_eq!(duration.as_millis(), 0);
        // Not precisely the original but should be smaller than 0.
        let delay = Delay::from_saturating_duration(duration);
        assert_eq!(delay.into_ratio().to_integer(), 0);
    }
}
