use url::ParseError;

pub trait ErrorCode {
  fn error_code(&self) -> i32;
}

impl<T: ErrorCode> ErrorCode for Result<(), T> {
  fn error_code(&self) -> i32 {
    match *self {
      Ok(_) => 0,
      Err(ref error) => error.error_code(),
    }
  }
}

impl ErrorCode for () {
    fn error_code(&self) -> i32 {
        return -1;
    }
}
impl ErrorCode for ParseError {
  fn error_code(&self) -> i32 {
      return -1;
//    match *self {
//      ParseError::EmptyHost                              =>  -1,
//      ParseError::InvalidScheme                          =>  -2,
//      ParseError::InvalidPort                            =>  -3,
//      ParseError::InvalidIpv6Address                     =>  -4,
//      ParseError::InvalidDomainCharacter                 =>  -5,
//      ParseError::InvalidCharacter                       =>  -6,
//      ParseError::InvalidBackslash                       =>  -7,
//      ParseError::InvalidPercentEncoded                  =>  -8,
//      ParseError::InvalidAtSymbolInUser                  =>  -9,
//      ParseError::ExpectedTwoSlashes                     => -10,
//      ParseError::ExpectedInitialSlash                   => -11,
//      ParseError::NonUrlCodePoint                        => -12,
//      ParseError::RelativeUrlWithScheme                  => -13,
//      ParseError::RelativeUrlWithoutBase                 => -14,
//      ParseError::RelativeUrlWithNonRelativeBase         => -15,
//      ParseError::NonAsciiDomainsNotSupportedYet         => -16,
//      ParseError::CannotSetJavascriptFragment            => -17,
//      ParseError::CannotSetPortWithFileLikeScheme        => -18,
//      ParseError::CannotSetUsernameWithNonRelativeScheme => -19,
//      ParseError::CannotSetPasswordWithNonRelativeScheme => -20,
//      ParseError::CannotSetHostPortWithNonRelativeScheme => -21,
//      ParseError::CannotSetHostWithNonRelativeScheme     => -22,
//      ParseError::CannotSetPortWithNonRelativeScheme     => -23,
//      ParseError::CannotSetPathWithNonRelativeScheme     => -24,
//    }
  }
}

pub enum NSError {
  OK,
  InvalidArg,
  Failure,
}

impl ErrorCode for NSError {
  #[allow(overflowing_literals)]
  fn error_code(&self) -> i32 {
    match *self {
      NSError::OK => 0,
      NSError::InvalidArg => 0x80070057,
      NSError::Failure => 0x80004005
    }
  }
}
