use crate::constants::MAX_PRECISION;
use crate::{
    ops::array::{div_by_u32, is_all_zero, mul_by_u32},
    Decimal,
};
use core::{convert::TryInto, fmt};
use std::error;

#[derive(Debug, Clone)]
pub struct InvalidDecimal {
    inner: Option<String>,
}

impl fmt::Display for InvalidDecimal {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        if let Some(ref msg) = self.inner {
            fmt.write_fmt(format_args!("Invalid Decimal: {}", msg))
        } else {
            fmt.write_str("Invalid Decimal")
        }
    }
}

impl error::Error for InvalidDecimal {}

struct PostgresDecimal<D> {
    neg: bool,
    weight: i16,
    scale: u16,
    digits: D,
}

impl Decimal {
    fn from_postgres<D: ExactSizeIterator<Item = u16>>(
        PostgresDecimal {
            neg,
            scale,
            digits,
            weight,
        }: PostgresDecimal<D>,
    ) -> Self {
        let mut digits = digits.into_iter().collect::<Vec<_>>();

        let fractionals_part_count = digits.len() as i32 + (-weight as i32) - 1;
        let integers_part_count = weight as i32 + 1;

        let mut result = Decimal::ZERO;
        // adding integer part
        if integers_part_count > 0 {
            let (start_integers, last) = if integers_part_count > digits.len() as i32 {
                (integers_part_count - digits.len() as i32, digits.len() as i32)
            } else {
                (0, integers_part_count)
            };
            let integers: Vec<_> = digits.drain(..last as usize).collect();
            for digit in integers {
                result *= Decimal::from_i128_with_scale(10i128.pow(4), 0);
                result += Decimal::new(digit as i64, 0);
            }
            result *= Decimal::from_i128_with_scale(10i128.pow(4 * start_integers as u32), 0);
        }
        // adding fractional part
        if fractionals_part_count > 0 {
            let dec: Vec<_> = digits.into_iter().collect();
            let start_fractionals = if weight < 0 { (-weight as u32) - 1 } else { 0 };
            for (i, digit) in dec.into_iter().enumerate() {
                let fract_pow = 4 * (i as u32 + 1 + start_fractionals);
                if fract_pow <= MAX_PRECISION {
                    result += Decimal::new(digit as i64, 0) / Decimal::from_i128_with_scale(10i128.pow(fract_pow), 0);
                } else if fract_pow == MAX_PRECISION + 4 {
                    // rounding last digit
                    if digit >= 5000 {
                        result += Decimal::new(1_i64, 0) / Decimal::from_i128_with_scale(10i128.pow(MAX_PRECISION), 0);
                    }
                }
            }
        }

        result.set_sign_negative(neg);
        // Rescale to the postgres value, automatically rounding as needed.
        result.rescale(scale as u32);
        result
    }

    fn to_postgres(self) -> PostgresDecimal<Vec<i16>> {
        if self.is_zero() {
            return PostgresDecimal {
                neg: false,
                weight: 0,
                scale: 0,
                digits: vec![0],
            };
        }
        let scale = self.scale() as u16;

        let groups_diff = scale & 0x3; // groups_diff = scale % 4

        let mut mantissa = self.mantissa_array4();

        if groups_diff > 0 {
            let remainder = 4 - groups_diff;
            let power = 10u32.pow(u32::from(remainder));
            mul_by_u32(&mut mantissa, power);
        }

        // array to store max mantissa of Decimal in Postgres decimal format
        const MAX_GROUP_COUNT: usize = 8;
        let mut digits = Vec::with_capacity(MAX_GROUP_COUNT);

        while !is_all_zero(&mantissa) {
            let digit = div_by_u32(&mut mantissa, 10000) as u16;
            digits.push(digit.try_into().unwrap());
        }
        digits.reverse();
        let digits_after_decimal = (scale + 3) as u16 / 4;
        let weight = digits.len() as i16 - digits_after_decimal as i16 - 1;

        let unnecessary_zeroes = if weight >= 0 {
            let index_of_decimal = (weight + 1) as usize;
            digits
                .get(index_of_decimal..)
                .expect("enough digits exist")
                .iter()
                .rev()
                .take_while(|i| **i == 0)
                .count()
        } else {
            0
        };
        let relevant_digits = digits.len() - unnecessary_zeroes;
        digits.truncate(relevant_digits);

        PostgresDecimal {
            neg: self.is_sign_negative(),
            digits,
            scale,
            weight,
        }
    }
}

#[cfg(feature = "diesel")]
mod diesel {
    use super::*;
    use ::diesel::{
        deserialize::{self, FromSql},
        pg::data_types::PgNumeric,
        pg::Pg,
        serialize::{self, Output, ToSql},
        sql_types::Numeric,
    };
    use core::convert::{TryFrom, TryInto};
    use std::io::Write;

    impl<'a> TryFrom<&'a PgNumeric> for Decimal {
        type Error = Box<dyn error::Error + Send + Sync>;

        fn try_from(numeric: &'a PgNumeric) -> deserialize::Result<Self> {
            let (neg, weight, scale, digits) = match *numeric {
                PgNumeric::Positive {
                    weight,
                    scale,
                    ref digits,
                } => (false, weight, scale, digits),
                PgNumeric::Negative {
                    weight,
                    scale,
                    ref digits,
                } => (true, weight, scale, digits),
                PgNumeric::NaN => return Err(Box::from("NaN is not supported in Decimal")),
            };

            Ok(Self::from_postgres(PostgresDecimal {
                neg,
                weight,
                scale,
                digits: digits.iter().copied().map(|v| v.try_into().unwrap()),
            }))
        }
    }

    impl TryFrom<PgNumeric> for Decimal {
        type Error = Box<dyn error::Error + Send + Sync>;

        fn try_from(numeric: PgNumeric) -> deserialize::Result<Self> {
            (&numeric).try_into()
        }
    }

    impl<'a> From<&'a Decimal> for PgNumeric {
        fn from(decimal: &'a Decimal) -> Self {
            let PostgresDecimal {
                neg,
                weight,
                scale,
                digits,
            } = decimal.to_postgres();

            if neg {
                PgNumeric::Negative { digits, scale, weight }
            } else {
                PgNumeric::Positive { digits, scale, weight }
            }
        }
    }

    impl From<Decimal> for PgNumeric {
        fn from(bigdecimal: Decimal) -> Self {
            (&bigdecimal).into()
        }
    }

    impl ToSql<Numeric, Pg> for Decimal {
        fn to_sql<W: Write>(&self, out: &mut Output<W, Pg>) -> serialize::Result {
            let numeric = PgNumeric::from(self);
            ToSql::<Numeric, Pg>::to_sql(&numeric, out)
        }
    }

    impl FromSql<Numeric, Pg> for Decimal {
        fn from_sql(numeric: Option<&[u8]>) -> deserialize::Result<Self> {
            PgNumeric::from_sql(numeric)?.try_into()
        }
    }

    #[cfg(test)]
    mod pg_tests {
        use super::*;
        use core::str::FromStr;

        #[test]
        fn test_unnecessary_zeroes() {
            fn extract(value: &str) -> Decimal {
                Decimal::from_str(value).unwrap()
            }

            let tests = &[
                ("0.000001660"),
                ("41.120255926293000"),
                ("0.5538973300"),
                ("08883.55986854293100"),
                ("0.0000_0000_0016_6000_00"),
                ("0.00000166650000"),
                ("1666500000000"),
                ("1666500000000.0000054500"),
                ("8944.000000000000"),
            ];

            for &value in tests {
                let value = extract(value);
                let pg = PgNumeric::from(value);
                let dec = Decimal::try_from(pg).unwrap();
                assert_eq!(dec, value);
            }
        }

        #[test]
        fn decimal_to_pgnumeric_converts_digits_to_base_10000() {
            let decimal = Decimal::from_str("1").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 0,
                digits: vec![1],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("10").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 0,
                digits: vec![10],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("10000").unwrap();
            let expected = PgNumeric::Positive {
                weight: 1,
                scale: 0,
                digits: vec![1, 0],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("10001").unwrap();
            let expected = PgNumeric::Positive {
                weight: 1,
                scale: 0,
                digits: vec![1, 1],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("100000000").unwrap();
            let expected = PgNumeric::Positive {
                weight: 2,
                scale: 0,
                digits: vec![1, 0, 0],
            };
            assert_eq!(expected, decimal.into());
        }

        #[test]
        fn decimal_to_pg_numeric_properly_adjusts_scale() {
            let decimal = Decimal::from_str("1").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 0,
                digits: vec![1],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("1.0").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 1,
                digits: vec![1],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("1.1").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 1,
                digits: vec![1, 1000],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("1.10").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 2,
                digits: vec![1, 1000],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("100000000.0001").unwrap();
            let expected = PgNumeric::Positive {
                weight: 2,
                scale: 4,
                digits: vec![1, 0, 0, 1],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("0.1").unwrap();
            let expected = PgNumeric::Positive {
                weight: -1,
                scale: 1,
                digits: vec![1000],
            };
            assert_eq!(expected, decimal.into());
        }

        #[test]
        #[cfg(feature = "unstable")]
        fn decimal_to_pg_numeric_retains_sign() {
            let decimal = Decimal::from_str("123.456").unwrap();
            let expected = PgNumeric::Positive {
                weight: 0,
                scale: 3,
                digits: vec![123, 4560],
            };
            assert_eq!(expected, decimal.into());

            let decimal = Decimal::from_str("-123.456").unwrap();
            let expected = PgNumeric::Negative {
                weight: 0,
                scale: 3,
                digits: vec![123, 4560],
            };
            assert_eq!(expected, decimal.into());
        }

        #[test]
        fn pg_numeric_to_decimal_works() {
            let expected = Decimal::from_str("50").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 0,
                digits: vec![50],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res, expected);
            let expected = Decimal::from_str("123.456").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 3,
                digits: vec![123, 4560],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res, expected);

            let expected = Decimal::from_str("-56.78").unwrap();
            let pg_numeric = PgNumeric::Negative {
                weight: 0,
                scale: 2,
                digits: vec![56, 7800],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res, expected);

            // Verify no trailing zeroes are lost.

            let expected = Decimal::from_str("1.100").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 3,
                digits: vec![1, 1000],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());

            // To represent 5.00, Postgres can return either [5, 0] as the list of digits.
            let expected = Decimal::from_str("5.00").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 2,

                digits: vec![5, 0],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());

            // To represent 5.00, Postgres can return [5] as the list of digits.
            let expected = Decimal::from_str("5.00").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 2,
                digits: vec![5],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());

            let expected = Decimal::from_str("3.1415926535897932384626433833").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 30,
                digits: vec![3, 1415, 9265, 3589, 7932, 3846, 2643, 3832, 7950, 2800],
            };
            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());

            let expected = Decimal::from_str("3.1415926535897932384626433833").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 34,
                digits: vec![3, 1415, 9265, 3589, 7932, 3846, 2643, 3832, 7950, 2800],
            };

            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());

            let expected = Decimal::from_str("1.2345678901234567890123456790").unwrap();
            let pg_numeric = PgNumeric::Positive {
                weight: 0,
                scale: 34,
                digits: vec![1, 2345, 6789, 0123, 4567, 8901, 2345, 6789, 5000, 0],
            };

            let res: Decimal = pg_numeric.try_into().unwrap();
            assert_eq!(res.to_string(), expected.to_string());
        }
    }
}

#[cfg(feature = "postgres")]
mod postgres {
    use super::*;
    use ::postgres::types::{to_sql_checked, FromSql, IsNull, ToSql, Type};
    use byteorder::{BigEndian, ReadBytesExt};
    use bytes::{BufMut, BytesMut};
    use std::io::Cursor;

    impl<'a> FromSql<'a> for Decimal {
        // Decimals are represented as follows:
        // Header:
        //  u16 numGroups
        //  i16 weightFirstGroup (10000^weight)
        //  u16 sign (0x0000 = positive, 0x4000 = negative, 0xC000 = NaN)
        //  i16 dscale. Number of digits (in base 10) to print after decimal separator
        //
        //  Pseudo code :
        //  const Decimals [
        //          0.0000000000000000000000000001,
        //          0.000000000000000000000001,
        //          0.00000000000000000001,
        //          0.0000000000000001,
        //          0.000000000001,
        //          0.00000001,
        //          0.0001,
        //          1,
        //          10000,
        //          100000000,
        //          1000000000000,
        //          10000000000000000,
        //          100000000000000000000,
        //          1000000000000000000000000,
        //          10000000000000000000000000000
        //  ]
        //  overflow = false
        //  result = 0
        //  for i = 0, weight = weightFirstGroup + 7; i < numGroups; i++, weight--
        //    group = read.u16
        //    if weight < 0 or weight > MaxNum
        //       overflow = true
        //    else
        //       result += Decimals[weight] * group
        //  sign == 0x4000 ? -result : result

        // So if we were to take the number: 3950.123456
        //
        //  Stored on Disk:
        //    00 03 00 00 00 00 00 06 0F 6E 04 D2 15 E0
        //
        //  Number of groups: 00 03
        //  Weight of first group: 00 00
        //  Sign: 00 00
        //  DScale: 00 06
        //
        // 0F 6E = 3950
        //   result = result + 3950 * 1;
        // 04 D2 = 1234
        //   result = result + 1234 * 0.0001;
        // 15 E0 = 5600
        //   result = result + 5600 * 0.00000001;
        //

        fn from_sql(_: &Type, raw: &[u8]) -> Result<Decimal, Box<dyn error::Error + 'static + Sync + Send>> {
            let mut raw = Cursor::new(raw);
            let num_groups = raw.read_u16::<BigEndian>()?;
            let weight = raw.read_i16::<BigEndian>()?; // 10000^weight
                                                       // Sign: 0x0000 = positive, 0x4000 = negative, 0xC000 = NaN
            let sign = raw.read_u16::<BigEndian>()?;
            // Number of digits (in base 10) to print after decimal separator
            let scale = raw.read_u16::<BigEndian>()?;

            // Read all of the groups
            let mut groups = Vec::new();
            for _ in 0..num_groups as usize {
                groups.push(raw.read_u16::<BigEndian>()?);
            }

            Ok(Self::from_postgres(PostgresDecimal {
                neg: sign == 0x4000,
                weight,
                scale,
                digits: groups.into_iter(),
            }))
        }

        fn accepts(ty: &Type) -> bool {
            matches!(*ty, Type::NUMERIC)
        }
    }

    impl ToSql for Decimal {
        fn to_sql(
            &self,
            _: &Type,
            out: &mut BytesMut,
        ) -> Result<IsNull, Box<dyn error::Error + 'static + Sync + Send>> {
            let PostgresDecimal {
                neg,
                weight,
                scale,
                digits,
            } = self.to_postgres();

            let num_digits = digits.len();

            // Reserve bytes
            out.reserve(8 + num_digits * 2);

            // Number of groups
            out.put_u16(num_digits.try_into().unwrap());
            // Weight of first group
            out.put_i16(weight);
            // Sign
            out.put_u16(if neg { 0x4000 } else { 0x0000 });
            // DScale
            out.put_u16(scale);
            // Now process the number
            for digit in digits[0..num_digits].iter() {
                out.put_i16(*digit);
            }

            Ok(IsNull::No)
        }

        fn accepts(ty: &Type) -> bool {
            matches!(*ty, Type::NUMERIC)
        }

        to_sql_checked!();
    }

    #[cfg(test)]
    mod test {
        use super::*;
        use ::postgres::{Client, NoTls};
        use core::str::FromStr;

        /// Gets the URL for connecting to PostgreSQL for testing. Set the POSTGRES_URL
        /// environment variable to change from the default of "postgres://postgres@localhost".
        fn get_postgres_url() -> String {
            if let Ok(url) = std::env::var("POSTGRES_URL") {
                return url;
            }
            "postgres://postgres@localhost".to_string()
        }

        pub static TEST_DECIMALS: &[(u32, u32, &str, &str)] = &[
            // precision, scale, sent, expected
            (35, 6, "3950.123456", "3950.123456"),
            (35, 2, "3950.123456", "3950.12"),
            (35, 2, "3950.1256", "3950.13"),
            (10, 2, "3950.123456", "3950.12"),
            (35, 6, "3950", "3950.000000"),
            (4, 0, "3950", "3950"),
            (35, 6, "0.1", "0.100000"),
            (35, 6, "0.01", "0.010000"),
            (35, 6, "0.001", "0.001000"),
            (35, 6, "0.0001", "0.000100"),
            (35, 6, "0.00001", "0.000010"),
            (35, 6, "0.000001", "0.000001"),
            (35, 6, "1", "1.000000"),
            (35, 6, "-100", "-100.000000"),
            (35, 6, "-123.456", "-123.456000"),
            (35, 6, "119996.25", "119996.250000"),
            (35, 6, "1000000", "1000000.000000"),
            (35, 6, "9999999.99999", "9999999.999990"),
            (35, 6, "12340.56789", "12340.567890"),
            // Scale is only 28 since that is the maximum we can represent.
            (65, 30, "1.2", "1.2000000000000000000000000000"),
            // Pi - rounded at scale 28
            (
                65,
                30,
                "3.141592653589793238462643383279",
                "3.1415926535897932384626433833",
            ),
            (
                65,
                34,
                "3.1415926535897932384626433832795028",
                "3.1415926535897932384626433833",
            ),
            // Unrounded number
            (
                65,
                34,
                "1.234567890123456789012345678950000",
                "1.2345678901234567890123456790",
            ),
            (
                65,
                34, // No rounding due to 49999 after significant digits
                "1.234567890123456789012345678949999",
                "1.2345678901234567890123456789",
            ),
            // 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF (96 bit)
            (35, 0, "79228162514264337593543950335", "79228162514264337593543950335"),
            // 0x0FFF_FFFF_FFFF_FFFF_FFFF_FFFF (95 bit)
            (35, 1, "4951760157141521099596496895", "4951760157141521099596496895.0"),
            // 0x1000_0000_0000_0000_0000_0000
            (35, 1, "4951760157141521099596496896", "4951760157141521099596496896.0"),
            (35, 6, "18446744073709551615", "18446744073709551615.000000"),
            (35, 6, "-18446744073709551615", "-18446744073709551615.000000"),
            (35, 6, "0.10001", "0.100010"),
            (35, 6, "0.12345", "0.123450"),
        ];

        #[test]
        fn test_null() {
            let mut client = match Client::connect(&get_postgres_url(), NoTls) {
                Ok(x) => x,
                Err(err) => panic!("{:#?}", err),
            };

            // Test NULL
            let result: Option<Decimal> = match client.query("SELECT NULL::numeric", &[]) {
                Ok(x) => x.iter().next().unwrap().get(0),
                Err(err) => panic!("{:#?}", err),
            };
            assert_eq!(None, result);
        }

        #[tokio::test]
        #[cfg(feature = "tokio-pg")]
        async fn async_test_null() {
            use futures::future::FutureExt;
            use tokio_postgres::connect;

            let (client, connection) = connect(&get_postgres_url(), NoTls).await.unwrap();
            let connection = connection.map(|e| e.unwrap());
            tokio::spawn(connection);

            let statement = client.prepare(&"SELECT NULL::numeric").await.unwrap();
            let rows = client.query(&statement, &[]).await.unwrap();
            let result: Option<Decimal> = rows.iter().next().unwrap().get(0);

            assert_eq!(None, result);
        }

        #[test]
        fn read_numeric_type() {
            let mut client = match Client::connect(&get_postgres_url(), NoTls) {
                Ok(x) => x,
                Err(err) => panic!("{:#?}", err),
            };
            for &(precision, scale, sent, expected) in TEST_DECIMALS.iter() {
                let result: Decimal =
                    match client.query(&*format!("SELECT {}::NUMERIC({}, {})", sent, precision, scale), &[]) {
                        Ok(x) => x.iter().next().unwrap().get(0),
                        Err(err) => panic!("SELECT {}::NUMERIC({}, {}), error - {:#?}", sent, precision, scale, err),
                    };
                assert_eq!(
                    expected,
                    result.to_string(),
                    "NUMERIC({}, {}) sent: {}",
                    precision,
                    scale,
                    sent
                );
            }
        }

        #[tokio::test]
        #[cfg(feature = "tokio-pg")]
        async fn async_read_numeric_type() {
            use futures::future::FutureExt;
            use tokio_postgres::connect;

            let (client, connection) = connect(&get_postgres_url(), NoTls).await.unwrap();
            let connection = connection.map(|e| e.unwrap());
            tokio::spawn(connection);
            for &(precision, scale, sent, expected) in TEST_DECIMALS.iter() {
                let statement = client
                    .prepare(&*format!("SELECT {}::NUMERIC({}, {})", sent, precision, scale))
                    .await
                    .unwrap();
                let rows = client.query(&statement, &[]).await.unwrap();
                let result: Decimal = rows.iter().next().unwrap().get(0);

                assert_eq!(expected, result.to_string(), "NUMERIC({}, {})", precision, scale);
            }
        }

        #[test]
        fn write_numeric_type() {
            let mut client = match Client::connect(&get_postgres_url(), NoTls) {
                Ok(x) => x,
                Err(err) => panic!("{:#?}", err),
            };
            for &(precision, scale, sent, expected) in TEST_DECIMALS.iter() {
                let number = Decimal::from_str(sent).unwrap();
                let result: Decimal =
                    match client.query(&*format!("SELECT $1::NUMERIC({}, {})", precision, scale), &[&number]) {
                        Ok(x) => x.iter().next().unwrap().get(0),
                        Err(err) => panic!("{:#?}", err),
                    };
                assert_eq!(expected, result.to_string(), "NUMERIC({}, {})", precision, scale);
            }
        }

        #[tokio::test]
        #[cfg(feature = "tokio-pg")]
        async fn async_write_numeric_type() {
            use futures::future::FutureExt;
            use tokio_postgres::connect;

            let (client, connection) = connect(&get_postgres_url(), NoTls).await.unwrap();
            let connection = connection.map(|e| e.unwrap());
            tokio::spawn(connection);

            for &(precision, scale, sent, expected) in TEST_DECIMALS.iter() {
                let statement = client
                    .prepare(&*format!("SELECT $1::NUMERIC({}, {})", precision, scale))
                    .await
                    .unwrap();
                let number = Decimal::from_str(sent).unwrap();
                let rows = client.query(&statement, &[&number]).await.unwrap();
                let result: Decimal = rows.iter().next().unwrap().get(0);

                assert_eq!(expected, result.to_string(), "NUMERIC({}, {})", precision, scale);
            }
        }

        #[test]
        fn numeric_overflow() {
            let tests = [(4, 4, "3950.1234")];
            let mut client = match Client::connect(&get_postgres_url(), NoTls) {
                Ok(x) => x,
                Err(err) => panic!("{:#?}", err),
            };
            for &(precision, scale, sent) in tests.iter() {
                match client.query(&*format!("SELECT {}::NUMERIC({}, {})", sent, precision, scale), &[]) {
                    Ok(_) => panic!(
                        "Expected numeric overflow for {}::NUMERIC({}, {})",
                        sent, precision, scale
                    ),
                    Err(err) => {
                        assert_eq!("22003", err.code().unwrap().code(), "Unexpected error code");
                    }
                };
            }
        }

        #[tokio::test]
        #[cfg(feature = "tokio-pg")]
        async fn async_numeric_overflow() {
            use futures::future::FutureExt;
            use tokio_postgres::connect;

            let tests = [(4, 4, "3950.1234")];
            let (client, connection) = connect(&get_postgres_url(), NoTls).await.unwrap();
            let connection = connection.map(|e| e.unwrap());
            tokio::spawn(connection);

            for &(precision, scale, sent) in tests.iter() {
                let statement = client
                    .prepare(&*format!("SELECT {}::NUMERIC({}, {})", sent, precision, scale))
                    .await
                    .unwrap();

                match client.query(&statement, &[]).await {
                    Ok(_) => panic!(
                        "Expected numeric overflow for {}::NUMERIC({}, {})",
                        sent, precision, scale
                    ),
                    Err(err) => assert_eq!("22003", err.code().unwrap().code(), "Unexpected error code"),
                }
            }
        }
    }
}
