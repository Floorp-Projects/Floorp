/// Value that either holds a single A or B, or both.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum EitherOrBoth<A, B> {
    /// Both values are present.
    Both(A, B),
    /// Only the left value of type `A` is present.
    Left(A),
    /// Only the right value of type `B` is present.
    Right(B),
}
