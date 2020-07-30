#[macro_export]
macro_rules! alloc_tests {
    ( $TestRegion:path ) => {
        use libc::c_void;
        use std::sync::Arc;
        use $TestRegion as TestRegion;
        use $crate::alloc::Limits;
        use $crate::context::{Context, ContextHandle};
        use $crate::instance::InstanceInternal;
        use $crate::module::{GlobalValue, HeapSpec, MockModuleBuilder};
        use $crate::region::Region;
        use $crate::val::Val;

        const LIMITS_HEAP_MEM_SIZE: usize = 16 * 64 * 1024;
        const LIMITS_HEAP_ADDRSPACE_SIZE: usize = 8 * 1024 * 1024;
        const LIMITS_STACK_SIZE: usize = 64 * 1024;
        const LIMITS_GLOBALS_SIZE: usize = 4 * 1024;

        const LIMITS: Limits = Limits {
            heap_memory_size: LIMITS_HEAP_MEM_SIZE,
            heap_address_space_size: LIMITS_HEAP_ADDRSPACE_SIZE,
            stack_size: LIMITS_STACK_SIZE,
            globals_size: LIMITS_GLOBALS_SIZE,
            ..Limits::default()
        };

        const SPEC_HEAP_RESERVED_SIZE: u64 = LIMITS_HEAP_ADDRSPACE_SIZE as u64 / 2;
        const SPEC_HEAP_GUARD_SIZE: u64 = LIMITS_HEAP_ADDRSPACE_SIZE as u64 / 2;

        // one wasm page, not host page
        const ONEPAGE_INITIAL_SIZE: u64 = 64 * 1024;
        const ONEPAGE_MAX_SIZE: u64 = 64 * 1024;

        const ONE_PAGE_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE,
            initial_size: ONEPAGE_INITIAL_SIZE,
            max_size: Some(ONEPAGE_MAX_SIZE),
        };

        const THREEPAGE_INITIAL_SIZE: u64 = 64 * 1024;
        const THREEPAGE_MAX_SIZE: u64 = 3 * 64 * 1024;

        const THREE_PAGE_MAX_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: 0,
            initial_size: THREEPAGE_INITIAL_SIZE,
            max_size: Some(THREEPAGE_MAX_SIZE),
        };

        /// This test shows an `AllocHandle` passed to `Region::allocate_runtime` will have its heap
        /// and stack of the correct size and read/writability.
        #[test]
        fn allocate_runtime_works() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let mut inst = region
                .new_instance(
                    MockModuleBuilder::new()
                        .with_heap_spec(ONE_PAGE_HEAP)
                        .build(),
                )
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, ONEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[heap_len - 1], 0);
            heap[heap_len - 1] = 0xFF;
            assert_eq!(heap[heap_len - 1], 0xFF);

            let stack = unsafe { inst.alloc_mut().stack_mut() };
            assert_eq!(stack.len(), LIMITS_STACK_SIZE);

            assert_eq!(stack[0], 0);
            stack[0] = 0xFF;
            assert_eq!(stack[0], 0xFF);

            assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0);
            stack[LIMITS_STACK_SIZE - 1] = 0xFF;
            assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0xFF);
        }

        /// This test shows the heap works properly after a single expand.
        #[test]
        fn expand_heap_once() {
            expand_heap_once_template(THREE_PAGE_MAX_HEAP)
        }

        fn expand_heap_once_template(heap_spec: HeapSpec) {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(heap_spec.clone())
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, heap_spec.initial_size as usize);

            let new_heap_area = inst
                .alloc_mut()
                .expand_heap(64 * 1024, module.as_ref())
                .expect("expand_heap succeeds");
            assert_eq!(heap_len, new_heap_area as usize);

            let new_heap_len = inst.alloc().heap_len();
            assert_eq!(new_heap_len, heap_len + (64 * 1024));

            let heap = unsafe { inst.alloc_mut().heap_mut() };
            assert_eq!(heap[new_heap_len - 1], 0);
            heap[new_heap_len - 1] = 0xFF;
            assert_eq!(heap[new_heap_len - 1], 0xFF);
        }

        /// This test shows the heap works properly after two expands.
        #[test]
        fn expand_heap_twice() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(THREE_PAGE_MAX_HEAP)
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let new_heap_area = inst
                .alloc_mut()
                .expand_heap(64 * 1024, module.as_ref())
                .expect("expand_heap succeeds");
            assert_eq!(heap_len, new_heap_area as usize);

            let new_heap_len = inst.alloc().heap_len();
            assert_eq!(new_heap_len, heap_len + (64 * 1024));

            let second_new_heap_area = inst
                .alloc_mut()
                .expand_heap(64 * 1024, module.as_ref())
                .expect("expand_heap succeeds");
            assert_eq!(new_heap_len, second_new_heap_area as usize);

            let second_new_heap_len = inst.alloc().heap_len();
            assert_eq!(second_new_heap_len as u64, THREEPAGE_MAX_SIZE);

            let heap = unsafe { inst.alloc_mut().heap_mut() };
            assert_eq!(heap[new_heap_len - 1], 0);
            heap[new_heap_len - 1] = 0xFF;
            assert_eq!(heap[new_heap_len - 1], 0xFF);
        }

        /// This test shows that if you try to expand past the max given by the heap spec, the
        /// expansion fails, but the existing heap can still be used. This test uses a region with
        /// multiple slots in order to exercise more edge cases with adjacent managed memory.
        #[test]
        fn expand_past_spec_max() {
            let region = TestRegion::create(10, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(THREE_PAGE_MAX_HEAP)
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let new_heap_area = inst
                .alloc_mut()
                .expand_heap(THREEPAGE_MAX_SIZE as u32, module.as_ref());
            assert!(new_heap_area.is_err(), "heap expansion past spec fails");

            let new_heap_len = inst.alloc().heap_len();
            assert_eq!(new_heap_len, heap_len);

            let heap = unsafe { inst.alloc_mut().heap_mut() };
            assert_eq!(heap[new_heap_len - 1], 0);
            heap[new_heap_len - 1] = 0xFF;
            assert_eq!(heap[new_heap_len - 1], 0xFF);
        }

        const EXPANDPASTLIMIT_INITIAL_SIZE: u64 = LIMITS_HEAP_MEM_SIZE as u64 - (64 * 1024);
        const EXPANDPASTLIMIT_MAX_SIZE: u64 = LIMITS_HEAP_MEM_SIZE as u64 + (64 * 1024);
        const EXPAND_PAST_LIMIT_SPEC: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE,
            initial_size: EXPANDPASTLIMIT_INITIAL_SIZE,
            max_size: Some(EXPANDPASTLIMIT_MAX_SIZE),
        };

        /// This test shows that a heap refuses to grow past the alloc limits, even if the runtime
        /// spec says it can grow bigger. This test uses a region with multiple slots in order to
        /// exercise more edge cases with adjacent managed memory.
        #[test]
        fn expand_past_heap_limit() {
            let region = TestRegion::create(10, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(EXPAND_PAST_LIMIT_SPEC)
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, EXPANDPASTLIMIT_INITIAL_SIZE as usize);

            let new_heap_area = inst
                .alloc_mut()
                .expand_heap(64 * 1024, module.as_ref())
                .expect("expand_heap succeeds");
            assert_eq!(heap_len, new_heap_area as usize);

            let new_heap_len = inst.alloc().heap_len();
            assert_eq!(new_heap_len, LIMITS_HEAP_MEM_SIZE);

            let past_limit_heap_area = inst.alloc_mut().expand_heap(64 * 1024, module.as_ref());
            assert!(
                past_limit_heap_area.is_err(),
                "heap expansion past limit fails"
            );

            let still_heap_len = inst.alloc().heap_len();
            assert_eq!(still_heap_len, LIMITS_HEAP_MEM_SIZE);

            let heap = unsafe { inst.alloc_mut().heap_mut() };
            assert_eq!(heap[new_heap_len - 1], 0);
            heap[new_heap_len - 1] = 0xFF;
            assert_eq!(heap[new_heap_len - 1], 0xFF);
        }

        const INITIAL_OVERSIZE_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE,
            initial_size: SPEC_HEAP_RESERVED_SIZE + (64 * 1024),
            max_size: None,
        };

        /// This test shows that a heap refuses to grow past the alloc limits, even if the runtime
        /// spec says it can grow bigger. This test uses a region with multiple slots in order to
        /// exercise more edge cases with adjacent managed memory.
        #[test]
        fn reject_initial_oversize_heap() {
            let region = TestRegion::create(10, &LIMITS).expect("region created");
            let res = region.new_instance(
                MockModuleBuilder::new()
                    .with_heap_spec(INITIAL_OVERSIZE_HEAP)
                    .build(),
            );
            assert!(res.is_err(), "new_instance fails");
        }

        /// This test shows that we reject limits with a larger memory size than address space size
        #[test]
        fn reject_undersized_address_space() {
            const LIMITS: Limits = Limits {
                heap_memory_size: LIMITS_HEAP_ADDRSPACE_SIZE + 4096,
                heap_address_space_size: LIMITS_HEAP_ADDRSPACE_SIZE,
                stack_size: LIMITS_STACK_SIZE,
                globals_size: LIMITS_GLOBALS_SIZE,
                ..Limits::default()
            };
            let res = TestRegion::create(10, &LIMITS);
            assert!(res.is_err(), "region creation fails");
        }

        const SMALL_GUARD_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE - 1,
            initial_size: LIMITS_HEAP_MEM_SIZE as u64,
            max_size: None,
        };

        /// This test shows that a heap spec with a guard size smaller than the limits is
        /// allowed.
        #[test]
        fn accept_small_guard_heap() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let _inst = region
                .new_instance(
                    MockModuleBuilder::new()
                        .with_heap_spec(SMALL_GUARD_HEAP)
                        .build(),
                )
                .expect("new_instance succeeds");
        }

        const LARGE_GUARD_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE + 1,
            initial_size: ONEPAGE_INITIAL_SIZE,
            max_size: None,
        };

        /// This test shows that a `HeapSpec` with a guard size larger than the limits is not
        /// allowed.
        #[test]
        fn reject_large_guard_heap() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let res = region.new_instance(
                MockModuleBuilder::new()
                    .with_heap_spec(LARGE_GUARD_HEAP)
                    .build(),
            );
            assert!(res.is_err(), "new_instance fails");
        }

        /// This test shows that a `Slot` can be reused after an `AllocHandle` is dropped, and that
        /// its memory is reset.
        #[test]
        fn reuse_slot_works() {
            fn peek_n_poke(region: &Arc<TestRegion>) {
                let mut inst = region
                    .new_instance(
                        MockModuleBuilder::new()
                            .with_heap_spec(ONE_PAGE_HEAP)
                            .build(),
                    )
                    .expect("new_instance succeeds");

                let heap_len = inst.alloc().heap_len();
                assert_eq!(heap_len, ONEPAGE_INITIAL_SIZE as usize);

                let heap = unsafe { inst.alloc_mut().heap_mut() };

                assert_eq!(heap[0], 0);
                heap[0] = 0xFF;
                assert_eq!(heap[0], 0xFF);

                assert_eq!(heap[heap_len - 1], 0);
                heap[heap_len - 1] = 0xFF;
                assert_eq!(heap[heap_len - 1], 0xFF);

                let stack = unsafe { inst.alloc_mut().stack_mut() };
                assert_eq!(stack.len(), LIMITS_STACK_SIZE);

                assert_eq!(stack[0], 0);
                stack[0] = 0xFF;
                assert_eq!(stack[0], 0xFF);

                assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0);
                stack[LIMITS_STACK_SIZE - 1] = 0xFF;
                assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0xFF);

                let globals = unsafe { inst.alloc_mut().globals_mut() };
                assert_eq!(
                    globals.len(),
                    LIMITS_GLOBALS_SIZE / std::mem::size_of::<GlobalValue>()
                );

                unsafe {
                    assert_eq!(globals[0].i_64, 0);
                    globals[0].i_64 = 0xFF;
                    assert_eq!(globals[0].i_64, 0xFF);
                }

                unsafe {
                    assert_eq!(globals[globals.len() - 1].i_64, 0);
                    globals[globals.len() - 1].i_64 = 0xFF;
                    assert_eq!(globals[globals.len() - 1].i_64, 0xFF);
                }

                let sigstack = unsafe { inst.alloc_mut().sigstack_mut() };
                assert_eq!(sigstack.len(), LIMITS.signal_stack_size);

                assert_eq!(sigstack[0], 0);
                sigstack[0] = 0xFF;
                assert_eq!(sigstack[0], 0xFF);

                assert_eq!(sigstack[sigstack.len() - 1], 0);
                sigstack[sigstack.len() - 1] = 0xFF;
                assert_eq!(sigstack[sigstack.len() - 1], 0xFF);
            }

            // with a region size of 1, the slot must be reused
            let region = TestRegion::create(1, &LIMITS).expect("region created");

            peek_n_poke(&region);
            peek_n_poke(&region);
        }

        /// This test shows that the reset method clears the heap and resets its protections.
        #[test]
        fn alloc_reset() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(THREE_PAGE_MAX_HEAP)
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[heap_len - 1], 0);
            heap[heap_len - 1] = 0xFF;
            assert_eq!(heap[heap_len - 1], 0xFF);

            // Making a new mock module here because the borrow checker doesn't like referencing
            // `inst.module` while `inst.alloc()` is borrowed mutably. The `Instance` tests don't have
            // this weirdness
            inst.alloc_mut()
                .reset_heap(module.as_ref())
                .expect("reset succeeds");

            let reset_heap_len = inst.alloc().heap_len();
            assert_eq!(reset_heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[reset_heap_len - 1], 0);
            heap[reset_heap_len - 1] = 0xFF;
            assert_eq!(heap[reset_heap_len - 1], 0xFF);
        }

        /// This test shows that the reset method clears the heap and restores it to the spec
        /// initial size after growing the heap.
        #[test]
        fn alloc_grow_reset() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let module = MockModuleBuilder::new()
                .with_heap_spec(THREE_PAGE_MAX_HEAP)
                .build();
            let mut inst = region
                .new_instance(module.clone())
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[heap_len - 1], 0);
            heap[heap_len - 1] = 0xFF;
            assert_eq!(heap[heap_len - 1], 0xFF);

            let new_heap_area = inst
                .alloc_mut()
                .expand_heap(
                    (THREEPAGE_MAX_SIZE - THREEPAGE_INITIAL_SIZE) as u32,
                    module.as_ref(),
                )
                .expect("expand_heap succeeds");
            assert_eq!(heap_len, new_heap_area as usize);

            let new_heap_len = inst.alloc().heap_len();
            assert_eq!(new_heap_len, THREEPAGE_MAX_SIZE as usize);

            // Making a new mock module here because the borrow checker doesn't like referencing
            // `inst.module` while `inst.alloc()` is borrowed mutably. The `Instance` tests don't have
            // this weirdness
            inst.alloc_mut()
                .reset_heap(module.as_ref())
                .expect("reset succeeds");

            let reset_heap_len = inst.alloc().heap_len();
            assert_eq!(reset_heap_len, THREEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[reset_heap_len - 1], 0);
            heap[reset_heap_len - 1] = 0xFF;
            assert_eq!(heap[reset_heap_len - 1], 0xFF);
        }

        const GUARDLESS_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: 0,
            initial_size: ONEPAGE_INITIAL_SIZE,
            max_size: None,
        };

        /// This test shows the alloc works even with a zero guard size.
        #[test]
        fn guardless_heap_create() {
            let region = TestRegion::create(1, &LIMITS).expect("region created");
            let mut inst = region
                .new_instance(
                    MockModuleBuilder::new()
                        .with_heap_spec(GUARDLESS_HEAP)
                        .build(),
                )
                .expect("new_instance succeeds");

            let heap_len = inst.alloc().heap_len();
            assert_eq!(heap_len, ONEPAGE_INITIAL_SIZE as usize);

            let heap = unsafe { inst.alloc_mut().heap_mut() };

            assert_eq!(heap[0], 0);
            heap[0] = 0xFF;
            assert_eq!(heap[0], 0xFF);

            assert_eq!(heap[heap_len - 1], 0);
            heap[heap_len - 1] = 0xFF;
            assert_eq!(heap[heap_len - 1], 0xFF);

            let stack = unsafe { inst.alloc_mut().stack_mut() };
            assert_eq!(stack.len(), LIMITS_STACK_SIZE);

            assert_eq!(stack[0], 0);
            stack[0] = 0xFF;
            assert_eq!(stack[0], 0xFF);

            assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0);
            stack[LIMITS_STACK_SIZE - 1] = 0xFF;
            assert_eq!(stack[LIMITS_STACK_SIZE - 1], 0xFF);
        }

        /// This test shows a guardless heap works properly after a single expand.
        #[test]
        fn guardless_expand_heap_once() {
            expand_heap_once_template(GUARDLESS_HEAP)
        }

        const INITIAL_EMPTY_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: SPEC_HEAP_GUARD_SIZE,
            initial_size: 0,
            max_size: None,
        };

        /// This test shows an initially-empty heap works properly after a single expand.
        #[test]
        fn initial_empty_expand_heap_once() {
            expand_heap_once_template(INITIAL_EMPTY_HEAP)
        }

        const INITIAL_EMPTY_GUARDLESS_HEAP: HeapSpec = HeapSpec {
            reserved_size: SPEC_HEAP_RESERVED_SIZE,
            guard_size: 0,
            initial_size: 0,
            max_size: None,
        };

        /// This test shows an initially-empty, guardless heap works properly after a single
        /// expand.
        #[test]
        fn initial_empty_guardless_expand_heap_once() {
            expand_heap_once_template(INITIAL_EMPTY_GUARDLESS_HEAP)
        }

        const CONTEXT_TEST_LIMITS: Limits = Limits {
            heap_memory_size: 4096,
            heap_address_space_size: 2 * 4096,
            stack_size: 4096,
            globals_size: 4096,
            ..Limits::default()
        };
        const CONTEXT_TEST_INITIAL_SIZE: u64 = 4096;
        const CONTEXT_TEST_HEAP: HeapSpec = HeapSpec {
            reserved_size: 4096,
            guard_size: 4096,
            initial_size: CONTEXT_TEST_INITIAL_SIZE,
            max_size: Some(4096),
        };

        /// This test shows that alloced memory will create a heap and a stack that child context
        /// code can use.
        #[test]
        fn context_alloc_child() {
            extern "C" fn heap_touching_child(heap: *mut u8) {
                let heap = unsafe {
                    std::slice::from_raw_parts_mut(heap, CONTEXT_TEST_INITIAL_SIZE as usize)
                };
                heap[0] = 123;
                heap[4095] = 45;
            }

            let region = TestRegion::create(1, &CONTEXT_TEST_LIMITS).expect("region created");
            let mut inst = region
                .new_instance(
                    MockModuleBuilder::new()
                        .with_heap_spec(CONTEXT_TEST_HEAP)
                        .build(),
                )
                .expect("new_instance succeeds");

            let mut parent = ContextHandle::new();
            unsafe {
                let heap_ptr = inst.alloc_mut().heap_mut().as_ptr() as *mut c_void;
                let child = ContextHandle::create_and_init(
                    inst.alloc_mut().stack_u64_mut(),
                    &mut parent,
                    heap_touching_child as usize,
                    &[Val::CPtr(heap_ptr)],
                )
                .expect("context init succeeds");
                Context::swap(&mut parent, &child);
                assert_eq!(inst.alloc().heap()[0], 123);
                assert_eq!(inst.alloc().heap()[4095], 45);
            }
        }

        /// This test shows that an alloced memory will create a heap and stack, the child code can
        /// write a pattern to that stack, and we can read back that same pattern after it is done
        /// running.
        #[test]
        fn context_stack_pattern() {
            const STACK_PATTERN_LENGTH: usize = 1024;
            extern "C" fn stack_pattern_child(heap: *mut u64) {
                let heap = unsafe {
                    std::slice::from_raw_parts_mut(heap, CONTEXT_TEST_INITIAL_SIZE as usize / 8)
                };
                let mut onthestack = [0u8; STACK_PATTERN_LENGTH];
                for i in 0..STACK_PATTERN_LENGTH {
                    onthestack[i] = (i % 256) as u8;
                }
                heap[0] = onthestack.as_ptr() as u64;
            }

            let region = TestRegion::create(1, &CONTEXT_TEST_LIMITS).expect("region created");
            let mut inst = region
                .new_instance(
                    MockModuleBuilder::new()
                        .with_heap_spec(CONTEXT_TEST_HEAP)
                        .build(),
                )
                .expect("new_instance succeeds");

            let mut parent = ContextHandle::new();
            unsafe {
                let heap_ptr = inst.alloc_mut().heap_mut().as_ptr() as *mut c_void;
                let child = ContextHandle::create_and_init(
                    inst.alloc_mut().stack_u64_mut(),
                    &mut parent,
                    stack_pattern_child as usize,
                    &[Val::CPtr(heap_ptr)],
                )
                .expect("context init succeeds");
                Context::swap(&mut parent, &child);

                let stack_pattern = inst.alloc().heap_u64()[0] as usize;
                assert!(stack_pattern > inst.alloc().slot().stack as usize);
                assert!(
                    stack_pattern + STACK_PATTERN_LENGTH < inst.alloc().slot().stack_top() as usize
                );
                let stack_pattern =
                    std::slice::from_raw_parts(stack_pattern as *const u8, STACK_PATTERN_LENGTH);
                for i in 0..STACK_PATTERN_LENGTH {
                    assert_eq!(stack_pattern[i], (i % 256) as u8);
                }
            }
        }

        #[test]
        fn drop_region_first() {
            let region = TestRegion::create(1, &Limits::default()).expect("region can be created");
            let inst = region
                .new_instance(MockModuleBuilder::new().build())
                .expect("new_instance succeeds");
            drop(region);
            drop(inst);
        }
    };
}

#[cfg(test)]
alloc_tests!(crate::region::mmap::MmapRegion);
