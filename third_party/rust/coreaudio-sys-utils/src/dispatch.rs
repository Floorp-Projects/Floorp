use coreaudio_sys::*;

use std::ffi::CString;
use std::mem;
use std::os::raw::c_void;
use std::ptr;

pub const DISPATCH_QUEUE_SERIAL: dispatch_queue_attr_t = ptr::null_mut::<dispatch_queue_attr_s>();

pub fn create_dispatch_queue(
    label: &'static str,
    queue_attr: dispatch_queue_attr_t,
) -> dispatch_queue_t {
    let label = CString::new(label).unwrap();
    let c_string = label.as_ptr();
    unsafe { dispatch_queue_create(c_string, queue_attr) }
}

pub fn release_dispatch_queue(queue: dispatch_queue_t) {
    // TODO: This is incredibly unsafe. Find another way to release the queue.
    unsafe {
        dispatch_release(mem::transmute::<dispatch_queue_t, dispatch_object_t>(queue));
    }
}

pub fn async_dispatch<F>(queue: dispatch_queue_t, work: F)
where
    F: Send + FnOnce(),
{
    let (closure, executor) = create_closure_and_executor(work);
    unsafe {
        dispatch_async_f(queue, closure, executor);
    }
}

pub fn sync_dispatch<F>(queue: dispatch_queue_t, work: F)
where
    F: Send + FnOnce(),
{
    let (closure, executor) = create_closure_and_executor(work);
    unsafe {
        dispatch_sync_f(queue, closure, executor);
    }
}

// Return an raw pointer to a (unboxed) closure and an executor that
// will run the closure (after re-boxing the closure) when it's called.
fn create_closure_and_executor<F>(closure: F) -> (*mut c_void, dispatch_function_t)
where
    F: FnOnce(),
{
    extern "C" fn closure_executer<F>(unboxed_closure: *mut c_void)
    where
        F: FnOnce(),
    {
        // Retake the leaked closure.
        let closure: Box<F> = unsafe { Box::from_raw(unboxed_closure as *mut F) };
        // Execute the closure.
        (*closure)();
        // closure is released after finishiing this function call.
    }

    let closure: Box<F> = Box::new(closure); // Allocate closure on heap.
    let executor: dispatch_function_t = Some(closure_executer::<F>);

    (
        Box::into_raw(closure) as *mut c_void, // Leak the closure.
        executor,
    )
}

#[cfg(test)]
mod test {
    use super::*;
    use std::sync::{Arc, Mutex};
    const COUNT: u32 = 10;

    #[test]
    fn test_async_dispatch() {
        use std::sync::mpsc::channel;

        get_queue_and_resource("Run with async dispatch api wrappers", |queue, resource| {
            let (tx, rx) = channel();
            for i in 0..COUNT {
                let (res, tx) = (Arc::clone(&resource), tx.clone());
                async_dispatch(queue, move || {
                    let mut res = res.lock().unwrap();
                    assert_eq!(res.last_touched, if i == 0 { None } else { Some(i - 1) });
                    assert_eq!(res.touched_count, i);
                    res.touch(i);
                    if i == COUNT - 1 {
                        tx.send(()).unwrap();
                    }
                });
            }
            rx.recv().unwrap(); // Wait until it's touched COUNT times.
            let resource = resource.lock().unwrap();
            assert_eq!(resource.touched_count, COUNT);
            assert_eq!(resource.last_touched.unwrap(), COUNT - 1);
        });
    }

    #[test]
    fn test_sync_dispatch() {
        get_queue_and_resource("Run with sync dispatch api wrappers", |queue, resource| {
            for i in 0..COUNT {
                let res = Arc::clone(&resource);
                sync_dispatch(queue, move || {
                    let mut res = res.lock().unwrap();
                    assert_eq!(res.last_touched, if i == 0 { None } else { Some(i - 1) });
                    assert_eq!(res.touched_count, i);
                    res.touch(i);
                });
            }
            let resource = resource.lock().unwrap();
            assert_eq!(resource.touched_count, COUNT);
            assert_eq!(resource.last_touched.unwrap(), COUNT - 1);
        });
    }

    struct Resource {
        last_touched: Option<u32>,
        touched_count: u32,
    }

    impl Resource {
        fn new() -> Self {
            Resource {
                last_touched: None,
                touched_count: 0,
            }
        }
        fn touch(&mut self, who: u32) {
            self.last_touched = Some(who);
            self.touched_count += 1;
        }
    }

    fn get_queue_and_resource<F>(label: &'static str, callback: F)
    where
        F: FnOnce(dispatch_queue_t, Arc<Mutex<Resource>>),
    {
        let queue = create_dispatch_queue(label, DISPATCH_QUEUE_SERIAL);
        let resource = Arc::new(Mutex::new(Resource::new()));

        callback(queue, resource);

        // Release the queue.
        release_dispatch_queue(queue);
    }
}
