#[macro_export]
macro_rules! sparse_page_data_tests {
    ( $TestRegion:path ) => {
        use std::sync::Arc;
        use $TestRegion as TestRegion;
        use $crate::alloc::{host_page_size, Limits};
        use $crate::instance::InstanceInternal;
        use $crate::module::{MockModuleBuilder, Module};
        use $crate::region::Region;

        const FIRST_MESSAGE: &'static [u8] = b"hello from mock_sparse_module!";
        const SECOND_MESSAGE: &'static [u8] = b"hello again from mock_sparse_module!";

        fn mock_sparse_module() -> Arc<dyn Module> {
            let mut initial_heap = FIRST_MESSAGE.to_vec();
            // zero remainder of the first page, and the whole second page
            initial_heap.resize(4096 * 2, 0);
            let mut third_page = SECOND_MESSAGE.to_vec();
            third_page.resize(4096, 0);
            initial_heap.append(&mut third_page);
            MockModuleBuilder::new()
                .with_initial_heap(initial_heap.as_slice())
                .build()
        }

        #[test]
        fn valid_sparse_page_data() {
            let module = mock_sparse_module();

            assert_eq!(module.sparse_page_data_len(), 3);

            let mut first_page_expected: Vec<u8> = FIRST_MESSAGE.to_vec();
            first_page_expected.resize(host_page_size(), 0);
            let mut third_page_expected: Vec<u8> = SECOND_MESSAGE.to_vec();
            third_page_expected.resize(host_page_size(), 0);

            let first_page: &[u8] = module.get_sparse_page_data(0).unwrap();
            assert_eq!(first_page, first_page_expected.as_slice());

            assert!(module.get_sparse_page_data(1).is_none());

            let third_page: &[u8] = module.get_sparse_page_data(2).unwrap();
            assert_eq!(third_page, third_page_expected.as_slice());
        }

        #[test]
        fn instantiate_valid_sparse_data() {
            let module = mock_sparse_module();
            let region = TestRegion::create(1, &Limits::default()).expect("region can be created");
            let inst = region
                .new_instance(module)
                .expect("instance can be created");

            // The test data initializers result in two strings getting copied into linear memory; see
            // `lucet-runtime-c/test/data_segment/valid_data_seg.c` for details
            let heap = unsafe { inst.alloc().heap() };
            assert_eq!(&heap[0..FIRST_MESSAGE.len()], FIRST_MESSAGE.as_ref());
            let second_message_start = 2 * host_page_size();
            assert_eq!(
                &heap[second_message_start..second_message_start + SECOND_MESSAGE.len()],
                SECOND_MESSAGE.as_ref()
            );
        }
    };
}

#[cfg(test)]
mod tests {
    sparse_page_data_tests!(crate::region::mmap::MmapRegion);
}
