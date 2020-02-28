extern crate bumpalo;

use bumpalo::Bump;
use std::alloc::Layout;
use std::mem;
use std::slice;
use std::usize;

#[test]
fn can_iterate_over_allocated_things() {
    let mut bump = Bump::new();

    const MAX: u64 = 131072;

    let mut chunks = vec![];
    let mut last = None;

    for i in 0..MAX {
        let this = bump.alloc(i);
        assert_eq!(*this, i);
        let this = this as *const _ as usize;

        if match last {
            Some(last) if last + mem::size_of::<u64>() == this => false,
            _ => true,
        } {
            println!("new chunk @ 0x{:x}", this);
            assert!(
                !chunks.contains(&this),
                "should not have already allocated this chunk"
            );
            chunks.push(this);
        }

        last = Some(this);
    }

    let mut seen = vec![false; MAX as usize];
    chunks.reverse();

    // Safe because we always allocated objects of the same type in this arena,
    // and their size >= their align.
    unsafe {
        bump.each_allocated_chunk(|ch| {
            let ch_usize = ch.as_ptr() as usize;
            println!("iter chunk @ 0x{:x}", ch_usize);
            assert_eq!(
                chunks.pop().unwrap(),
                ch_usize,
                "should iterate over each chunk once, in order they were allocated in"
            );

            let ch: &[u64] =
                slice::from_raw_parts(ch.as_ptr() as *mut u64, ch.len() / mem::size_of::<u64>());
            for i in ch {
                assert!(*i < MAX, "{} < {} (aka {:x} < {:x})", i, MAX, i, MAX);
                seen[*i as usize] = true;
            }
        });
    }

    assert!(seen.iter().all(|s| *s));
}

#[test]
#[should_panic(expected = "out of memory")]
fn oom_instead_of_bump_pointer_overflow() {
    let bump = Bump::new();
    let x = bump.alloc(0_u8);
    let p = x as *mut u8 as usize;

    // A size guaranteed to overflow the bump pointer.
    let size = usize::MAX - p + 1;
    let align = 1;
    let layout = match Layout::from_size_align(size, align) {
        Err(e) => {
            // Return on error so that we don't panic and the test fails.
            eprintln!("Layout::from_size_align errored: {}", e);
            return;
        }
        Ok(l) => l,
    };

    // This should panic.
    bump.alloc_layout(layout);
}

#[test]
fn force_new_chunk_fits_well() {
    let b = Bump::new();

    // Use the first chunk for something
    b.alloc_layout(Layout::from_size_align(1, 1).unwrap());

    // Next force allocation of some new chunks.
    b.alloc_layout(Layout::from_size_align(100_001, 1).unwrap());
    b.alloc_layout(Layout::from_size_align(100_003, 1).unwrap());
}

#[test]
fn alloc_with_strong_alignment() {
    let b = Bump::new();

    // 64 is probably the strongest alignment we'll see in practice
    // e.g. AVX-512 types, or cache line padding optimizations
    b.alloc_layout(Layout::from_size_align(4096, 64).unwrap());
}

#[test]
fn alloc_slice_copy() {
    let b = Bump::new();

    let src: &[u16] = &[0xFEED, 0xFACE, 0xA7, 0xCAFE];
    let dst = b.alloc_slice_copy(src);

    assert_eq!(src, dst);
}

#[test]
fn alloc_slice_clone() {
    let b = Bump::new();

    let src = vec![vec![0], vec![1,2], vec![3,4,5], vec![6,7,8,9]];
    let dst = b.alloc_slice_clone(&src);

    assert_eq!(src, dst);
}
