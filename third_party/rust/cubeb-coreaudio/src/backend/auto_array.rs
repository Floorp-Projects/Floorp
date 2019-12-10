use std::fmt::Debug;
use std::os::raw::c_void;
use std::ptr;
use std::slice;

pub trait AutoArrayWrapper: Debug {
    fn push(&mut self, data: *const c_void, elements: usize);
    fn push_zeros(&mut self, elements: usize);
    fn pop(&mut self, elements: usize) -> bool;
    fn clear(&mut self);
    fn elements(&self) -> usize;
    fn as_ptr(&self) -> *const c_void;
    fn as_mut_ptr(&mut self) -> *mut c_void;
}

#[derive(Debug)]
pub struct AutoArrayImpl<T: Clone + Debug + Zero> {
    ar: Vec<T>,
}

impl<T: Clone + Debug + Zero> AutoArrayImpl<T> {
    pub fn new(size: usize) -> Self {
        AutoArrayImpl {
            ar: Vec::<T>::with_capacity(size),
        }
    }
}

impl<T: Clone + Debug + Zero> AutoArrayWrapper for AutoArrayImpl<T> {
    fn push(&mut self, data: *const c_void, elements: usize) {
        let slice = unsafe { slice::from_raw_parts(data as *const T, elements) };
        self.ar.extend_from_slice(slice);
    }

    fn push_zeros(&mut self, elements: usize) {
        let len = self.ar.len();
        self.ar.resize(len + elements, T::zero());
    }

    fn pop(&mut self, elements: usize) -> bool {
        if elements > self.ar.len() {
            return false;
        }
        self.ar.drain(0..elements);
        true
    }

    fn clear(&mut self) {
        self.ar.clear();
    }

    fn elements(&self) -> usize {
        self.ar.len()
    }

    fn as_ptr(&self) -> *const c_void {
        if self.ar.is_empty() {
            return ptr::null();
        }
        self.ar.as_ptr() as *const c_void
    }

    fn as_mut_ptr(&mut self) -> *mut c_void {
        if self.ar.is_empty() {
            return ptr::null_mut();
        }
        self.ar.as_mut_ptr() as *mut c_void
    }
}

// Define the zero values for the different types.
// With Zero trait, AutoArrayImpl can be constructed
// only with the limited types.
pub trait Zero {
    fn zero() -> Self;
}

impl Zero for f32 {
    fn zero() -> Self {
        0.0
    }
}

impl Zero for i16 {
    fn zero() -> Self {
        0
    }
}

#[cfg(test)]
fn test_auto_array_impl<T: Clone + Debug + PartialEq + Zero>(buf: &[T]) {
    let mut auto_array = AutoArrayImpl::<T>::new(5);
    assert_eq!(auto_array.elements(), 0);
    assert!(auto_array.as_ptr().is_null());

    // Check if push works.
    auto_array.push(buf.as_ptr() as *const c_void, buf.len());
    assert_eq!(auto_array.elements(), buf.len());

    let data = auto_array.as_ptr() as *const T;
    for (idx, item) in buf.iter().enumerate() {
        unsafe {
            assert_eq!(*data.add(idx), *item);
        }
    }

    // Check if pop works.
    assert!(!auto_array.pop(buf.len() + 1));
    const POP: usize = 3;
    assert!(POP < buf.len());
    assert!(auto_array.pop(POP));
    assert_eq!(auto_array.elements(), buf.len() - POP);

    let data = auto_array.as_ptr() as *const T;
    for i in 0..buf.len() - POP {
        unsafe {
            assert_eq!(*data.add(i), buf[POP + i]);
        }
    }

    // Check if extend_with_value works.
    const ZEROS: usize = 5;
    let len = auto_array.elements();
    auto_array.push_zeros(ZEROS);
    assert_eq!(auto_array.elements(), len + ZEROS);
    let data = auto_array.as_ptr() as *const T;
    for i in len..len + ZEROS {
        unsafe {
            assert_eq!(*data.add(i), T::zero());
        }
    }
}

#[cfg(test)]
fn test_auto_array_wrapper<T: Clone + Debug + PartialEq + Zero>(buf: &[T]) {
    let mut auto_array: Option<Box<dyn AutoArrayWrapper>> = None;
    auto_array = Some(Box::new(AutoArrayImpl::<T>::new(5)));

    assert_eq!(auto_array.as_ref().unwrap().elements(), 0);
    let data = auto_array.as_ref().unwrap().as_ptr() as *const T;
    assert!(data.is_null());

    // Check if push works.
    auto_array
        .as_mut()
        .unwrap()
        .push(buf.as_ptr() as *const c_void, buf.len());
    assert_eq!(auto_array.as_ref().unwrap().elements(), buf.len());

    let data = auto_array.as_ref().unwrap().as_ptr() as *const T;
    for i in 0..buf.len() {
        unsafe {
            assert_eq!(*data.add(i), buf[i]);
        }
    }

    // Check if pop works.
    assert!(!auto_array.as_mut().unwrap().pop(buf.len() + 1));
    const POP: usize = 3;
    assert!(POP < buf.len());
    assert!(auto_array.as_mut().unwrap().pop(POP));
    assert_eq!(auto_array.as_ref().unwrap().elements(), buf.len() - POP);

    let data = auto_array.as_ref().unwrap().as_ptr() as *const T;
    for i in 0..buf.len() - POP {
        unsafe {
            assert_eq!(*data.add(i), buf[POP + i]);
        }
    }

    // Check if push_zeros works.
    const ZEROS: usize = 5;
    let len = auto_array.as_ref().unwrap().elements();
    auto_array.as_mut().unwrap().push_zeros(ZEROS);
    assert_eq!(auto_array.as_ref().unwrap().elements(), len + ZEROS);
    let data = auto_array.as_ref().unwrap().as_ptr() as *const T;
    for i in len..len + ZEROS {
        unsafe {
            assert_eq!(*data.add(i), T::zero());
        }
    }
}

#[test]
fn test_auto_array() {
    let buf_f32 = [1.0_f32, 2.1, 3.2, 4.3, 5.4];
    test_auto_array_impl(&buf_f32);
    test_auto_array_wrapper(&buf_f32);

    let buf_i16 = [5_i16, 8, 13, 21, 34, 55, 89, 144];
    test_auto_array_impl(&buf_i16);
    test_auto_array_wrapper(&buf_i16);
}
