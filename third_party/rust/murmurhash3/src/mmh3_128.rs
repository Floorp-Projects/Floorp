use std::mem;

fn fmix64(mut k: u64) -> u64 {
    k ^= k >> 33;
    k = k.wrapping_mul(0xff51afd7ed558ccdu64);
    k ^= k >> 33;
    k = k.wrapping_mul(0xc4ceb9fe1a85ec53u64);
    k ^= k >> 33;

    return k;
}

fn get_128_block(bytes: &[u8], index: usize) -> (u64, u64) {
    let b64: &[u64] = unsafe { mem::transmute(bytes) };

    return (b64[index], b64[index + 1]);
}

pub fn murmurhash3_x64_128(bytes: &[u8], seed: u64) -> (u64, u64) {
    let c1 = 0x87c37b91114253d5u64;
    let c2 = 0x4cf5ad432745937fu64;
    let read_size = 16;
    let len = bytes.len() as u64;
    let block_count = len / read_size;

    let (mut h1, mut h2) = (seed, seed);


    for i in 0..block_count as usize {
        let (mut k1, mut k2) = get_128_block(bytes, i * 2);

        k1 = k1.wrapping_mul(c1);
        k1 = k1.rotate_left(31);
        k1 = k1.wrapping_mul(c2);
        h1 ^= k1;

        h1 = h1.rotate_left(27);
        h1 = h1.wrapping_add(h2);
        h1 = h1.wrapping_mul(5);
        h1 = h1.wrapping_add(0x52dce729);

        k2 = k2.wrapping_mul(c2);
        k2 = k2.rotate_left(33);
        k2 = k2.wrapping_mul(c1);
        h2 ^= k2;

        h2 = h2.rotate_left(31);
        h2 = h2.wrapping_add(h1);
        h2 = h2.wrapping_mul(5);
        h2 = h2.wrapping_add(0x38495ab5);
    }


    let (mut k1, mut k2) = (0u64, 0u64);

    if len & 15 == 15 { k2 ^= (bytes[(block_count * read_size) as usize + 14] as u64) << 48; }
    if len & 15 >= 14 { k2 ^= (bytes[(block_count * read_size) as usize + 13] as u64) << 40; }
    if len & 15 >= 13 { k2 ^= (bytes[(block_count * read_size) as usize + 12] as u64) << 32; }
    if len & 15 >= 12 { k2 ^= (bytes[(block_count * read_size) as usize + 11] as u64) << 24; }
    if len & 15 >= 11 { k2 ^= (bytes[(block_count * read_size) as usize + 10] as u64) << 16; }
    if len & 15 >= 10 { k2 ^= (bytes[(block_count * read_size) as usize +  9] as u64) <<  8; }
    if len & 15 >=  9 { k2 ^=  bytes[(block_count * read_size) as usize +  8] as u64;
        k2 = k2.wrapping_mul(c2);
        k2 = k2.rotate_left(33);
        k2 = k2.wrapping_mul(c1);
        h2 ^= k2;
    }

    if len & 15 >= 8 { k1 ^= (bytes[(block_count * read_size) as usize + 7] as u64) << 56; }
    if len & 15 >= 7 { k1 ^= (bytes[(block_count * read_size) as usize + 6] as u64) << 48; }
    if len & 15 >= 6 { k1 ^= (bytes[(block_count * read_size) as usize + 5] as u64) << 40; }
    if len & 15 >= 5 { k1 ^= (bytes[(block_count * read_size) as usize + 4] as u64) << 32; }
    if len & 15 >= 4 { k1 ^= (bytes[(block_count * read_size) as usize + 3] as u64) << 24; }
    if len & 15 >= 3 { k1 ^= (bytes[(block_count * read_size) as usize + 2] as u64) << 16; }
    if len & 15 >= 2 { k1 ^= (bytes[(block_count * read_size) as usize + 1] as u64) <<  8; }
    if len & 15 >= 1 { k1 ^=  bytes[(block_count * read_size) as usize + 0] as u64;
        k1 = k1.wrapping_mul(c1);
        k1 = k1.rotate_left(31);
        k1 = k1.wrapping_mul(c2);
        h1 ^= k1;
    }

    h1 ^= bytes.len() as u64;
    h2 ^= bytes.len() as u64;

    h1 = h1.wrapping_add(h2);
    h2 = h2.wrapping_add(h1);

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 = h1.wrapping_add(h2);
    h2 = h2.wrapping_add(h1);

    return (h1, h2);
}

#[cfg(test)]
mod test {
    use super::murmurhash3_x64_128;

    #[test]
    fn test_empty_string() {
        assert!(murmurhash3_x64_128("".as_bytes(), 0) == (0, 0));
    }

    #[test]
    fn test_tail_lengths() {
        assert!(murmurhash3_x64_128("1".as_bytes(), 0)
            == (8213365047359667313, 10676604921780958775));
        assert!(murmurhash3_x64_128("12".as_bytes(), 0)
            == (5355690773644049813, 9855895140584599837));
        assert!(murmurhash3_x64_128("123".as_bytes(), 0)
            == (10978418110857903978, 4791445053355511657));
        assert!(murmurhash3_x64_128("1234".as_bytes(), 0)
            == (619023178690193332, 3755592904005385637));
        assert!(murmurhash3_x64_128("12345".as_bytes(), 0)
            == (2375712675693977547, 17382870096830835188));
        assert!(murmurhash3_x64_128("123456".as_bytes(), 0)
            == (16435832985690558678, 5882968373513761278));
        assert!(murmurhash3_x64_128("1234567".as_bytes(), 0)
            == (3232113351312417698, 4025181827808483669));
        assert!(murmurhash3_x64_128("12345678".as_bytes(), 0)
            == (4272337174398058908, 10464973996478965079));
        assert!(murmurhash3_x64_128("123456789".as_bytes(), 0)
            == (4360720697772133540, 11094893415607738629));
        assert!(murmurhash3_x64_128("123456789a".as_bytes(), 0)
            == (12594836289594257748, 2662019112679848245));
        assert!(murmurhash3_x64_128("123456789ab".as_bytes(), 0)
            == (6978636991469537545, 12243090730442643750));
        assert!(murmurhash3_x64_128("123456789abc".as_bytes(), 0)
            == (211890993682310078, 16480638721813329343));
        assert!(murmurhash3_x64_128("123456789abcd".as_bytes(), 0)
            == (12459781455342427559, 3193214493011213179));
        assert!(murmurhash3_x64_128("123456789abcde".as_bytes(), 0)
            == (12538342858731408721, 9820739847336455216));
        assert!(murmurhash3_x64_128("123456789abcdef".as_bytes(), 0)
            == (9165946068217512774, 2451472574052603025));
        assert!(murmurhash3_x64_128("123456789abcdef1".as_bytes(), 0)
            == (9259082041050667785, 12459473952842597282));
    }

    #[test]
    fn test_large_data() {
        assert!(murmurhash3_x64_128("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam at consequat massa. Cras eleifend pellentesque ex, at dignissim libero maximus ut. Sed eget nulla felis".as_bytes(), 0)
            == (9455322759164802692, 17863277201603478371));
    }

    #[cfg(feature="nightly")]
    mod bench {
        extern crate rand;
        extern crate test;

        use std::iter::FromIterator;
        use self::rand::Rng;
        use self::test::{Bencher, black_box};

        use super::super::murmurhash3_x64_128;

        fn run_bench(b: &mut Bencher, size: u64) {
            let mut data: Vec<u8> = FromIterator::from_iter((0..size).map(|_| 0u8));
            rand::thread_rng().fill_bytes(&mut data);

            b.bytes = size;
            b.iter(|| {
                black_box(murmurhash3_x64_128(&data, 0));
            });
        }

        #[bench]
        fn bench_random_256k(b: &mut Bencher) {
            run_bench(b, 256 * 1024);
        }

        #[bench]
        fn bench_random_16b(b: &mut Bencher) {
            run_bench(b, 16);
        }

    }
}
