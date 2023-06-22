use core::{convert::TryInto, fmt::Debug};
use digest::ExtendableOutput;

pub(crate) fn turbo_shake_test<D, F>(
    input: &[u8],
    output: &[u8],
    truncate_output: usize,
    new: F,
) -> Option<&'static str>
where
    D: ExtendableOutput + Debug + Clone,
    F: Fn() -> D,
{
    let mut hasher = new();
    let mut buf = [0u8; 16 * 1024];
    let buf = &mut buf[..truncate_output + output.len()];
    // Test that it works when accepting the message all at once
    hasher.update(input);
    let mut hasher2 = hasher.clone();
    hasher.finalize_xof_into(buf);
    if &buf[truncate_output..] != output {
        return Some("whole message");
    }
    buf.iter_mut().for_each(|b| *b = 0);

    // Test that it works when accepting the message in chunks
    for n in 1..core::cmp::min(17, input.len()) {
        let mut hasher = new();
        for chunk in input.chunks(n) {
            hasher.update(chunk);
            hasher2.update(chunk);
        }
        hasher.finalize_xof_into(buf);
        if &buf[truncate_output..] != output {
            return Some("message in chunks");
        }
        buf.iter_mut().for_each(|b| *b = 0);
    }

    None
}

macro_rules! new_turbo_shake_test {
    ($name:ident, $test_name:expr, $hasher:ty, $hasher_core:ty, $test_func:ident $(,)?) => {
        #[test]
        fn $name() {
            use digest::dev::blobby::Blob5Iterator;
            let data = include_bytes!(concat!("data/", $test_name, ".blb"));

            for (i, row) in Blob5Iterator::new(data).unwrap().enumerate() {
                let [domain_separation, input, input_pattern_length, output, truncate_output] =
                    row.unwrap();

                let input = if (input_pattern_length.len() == 0) {
                    input.to_vec()
                } else if (input.len() == 0) {
                    let pattern_length =
                        u64::from_be_bytes(input_pattern_length.try_into().unwrap());
                    let mut input = Vec::<u8>::new();
                    for value in 0..pattern_length {
                        input.push((value % 0xFB).try_into().unwrap());
                    }
                    input
                } else {
                    panic!(
                        "\
                        failed to read tests data\n\
                         input:\t{:02X?}\n\
                         input_pattern_length:\t{:02X?}\n",
                        input, input_pattern_length,
                    );
                };

                if let Some(desc) = $test_func(
                    &input,
                    output,
                    u64::from_be_bytes(truncate_output.try_into().unwrap())
                        .try_into()
                        .unwrap(),
                    || <$hasher>::from_core(<$hasher_core>::new(domain_separation[0])),
                ) {
                    panic!(
                        "\n\
                         Failed test №{}: {}\n\
                         input:\t{:02X?}\n\
                         output:\t{:02X?}\n",
                        i, desc, &input, output,
                    );
                }
            }
        }
    };
}

new_turbo_shake_test!(
    turboshake128,
    "turboshake128",
    sha3::TurboShake128,
    sha3::TurboShake128Core,
    turbo_shake_test,
);
new_turbo_shake_test!(
    turboshake256,
    "turboshake256",
    sha3::TurboShake256,
    sha3::TurboShake256Core,
    turbo_shake_test,
);
