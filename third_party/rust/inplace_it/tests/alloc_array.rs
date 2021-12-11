#[test]
fn inplace_or_alloc_from_iter_works_fine() {
    // Don't forget to update 4096 and related constants

    // 0 and 4096 cases should work also fine
    for count in (0..8192).step_by(128) {
        let result = ::inplace_it::inplace_or_alloc_from_iter(0..count, |mem| {
            assert_eq!(mem.len(), count);
            assert!(mem.iter().cloned().eq(0..count));
            return mem.len() * 2;
        });
        assert_eq!(result, count * 2);
    }
}
