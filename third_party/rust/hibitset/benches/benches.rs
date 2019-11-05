#![feature(test)]

extern crate hibitset;
extern crate test;

mod bitset {
    use hibitset::BitSet;
    use test;

    #[bench]
    fn add(b: &mut test::Bencher) {
        let mut bitset = BitSet::with_capacity(1_000_000);
        let mut range = (0..1_000_000).cycle();
        b.iter(|| range.next().map(|i| bitset.add(i)))
    }

    #[bench]
    fn remove_set(b: &mut test::Bencher) {
        let mut bitset = BitSet::with_capacity(1_000_000);
        let mut range = (0..1_000_000).cycle();
        for i in 0..1_000_000 {
            bitset.add(i);
        }
        b.iter(|| range.next().map(|i| bitset.remove(i)))
    }

    #[bench]
    fn remove_clear(b: &mut test::Bencher) {
        let mut bitset = BitSet::with_capacity(1_000_000);
        let mut range = (0..1_000_000).cycle();
        b.iter(|| range.next().map(|i| bitset.remove(i)))
    }

    #[bench]
    fn contains(b: &mut test::Bencher) {
        let mut bitset = BitSet::with_capacity(1_000_000);
        let mut range = (0..1_000_000).cycle();
        for i in 0..500_000 {
            // events are set, odds are to keep the branch
            // prediction from getting to aggressive
            bitset.add(i * 2);
        }
        b.iter(|| range.next().map(|i| bitset.contains(i)))
    }
}

mod atomic_bitset {
    use hibitset::AtomicBitSet;
    use test;

    #[bench]
    fn add(b: &mut test::Bencher) {
        let mut bitset = AtomicBitSet::new();
        let mut range = (0..1_000_000).cycle();
        b.iter(|| range.next().map(|i| bitset.add(i)))
    }

    #[bench]
    fn add_atomic(b: &mut test::Bencher) {
        let bitset = AtomicBitSet::new();
        let mut range = (0..1_000_000).cycle();
        b.iter(|| range.next().map(|i| bitset.add_atomic(i)))
    }

    #[bench]
    fn remove_set(b: &mut test::Bencher) {
        let mut bitset = AtomicBitSet::new();
        let mut range = (0..1_000_000).cycle();
        for i in 0..1_000_000 {
            bitset.add(i);
        }
        b.iter(|| range.next().map(|i| bitset.remove(i)))
    }

    #[bench]
    fn remove_clear(b: &mut test::Bencher) {
        let mut bitset = AtomicBitSet::new();
        let mut range = (0..1_000_000).cycle();
        b.iter(|| range.next().map(|i| bitset.remove(i)))
    }

    #[bench]
    fn contains(b: &mut test::Bencher) {
        let mut bitset = AtomicBitSet::new();
        let mut range = (0..1_000_000).cycle();
        for i in 0..500_000 {
            // events are set, odds are to keep the branch
            // prediction from getting to aggressive
            bitset.add(i * 2);
        }
        b.iter(|| range.next().map(|i| bitset.contains(i)))
    }
}
