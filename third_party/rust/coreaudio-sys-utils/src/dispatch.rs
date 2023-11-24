use coreaudio_sys::*;

use std::ffi::CString;
use std::mem;
use std::os::raw::c_void;
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

// Queue: A wrapper around `dispatch_queue_t`.
// ------------------------------------------------------------------------------------------------
#[derive(Debug)]
pub struct Queue(dispatch_queue_t);

impl Queue {
    pub fn new(label: &str) -> Self {
        const DISPATCH_QUEUE_SERIAL: dispatch_queue_attr_t =
            ptr::null_mut::<dispatch_queue_attr_s>();
        let label = CString::new(label).unwrap();
        let c_string = label.as_ptr();
        let queue = Self(unsafe { dispatch_queue_create(c_string, DISPATCH_QUEUE_SERIAL) });
        queue.set_should_cancel(Box::new(AtomicBool::new(false)));
        queue
    }

    #[cfg(debug_assertions)]
    pub fn debug_assert_is_current(&self) {
        unsafe {
            dispatch_assert_queue(self.0);
        }
    }

    #[cfg(not(debug_assertions))]
    pub fn debug_assert_is_current(&self) {}

    pub fn run_async<F>(&self, work: F)
    where
        F: Send + FnOnce(),
    {
        let should_cancel = self.get_should_cancel();
        let (closure, executor) = Self::create_closure_and_executor(|| {
            if should_cancel.map_or(false, |v| v.load(Ordering::SeqCst)) {
                return;
            }
            work();
        });
        unsafe {
            dispatch_async_f(self.0, closure, executor);
        }
    }

    pub fn run_sync<F>(&self, work: F)
    where
        F: Send + FnOnce(),
    {
        let should_cancel = self.get_should_cancel();
        let (closure, executor) = Self::create_closure_and_executor(|| {
            if should_cancel.map_or(false, |v| v.load(Ordering::SeqCst)) {
                return;
            }
            work();
        });
        unsafe {
            dispatch_sync_f(self.0, closure, executor);
        }
    }

    pub fn run_final<F>(&self, work: F)
    where
        F: Send + FnOnce(),
    {
        let should_cancel = self.get_should_cancel();
        let (closure, executor) = Self::create_closure_and_executor(|| {
            work();
            should_cancel
                .expect("dispatch context should be allocated!")
                .store(true, Ordering::SeqCst);
        });
        unsafe {
            dispatch_sync_f(self.0, closure, executor);
        }
    }

    fn get_should_cancel(&self) -> Option<&mut AtomicBool> {
        unsafe {
            let context = dispatch_get_context(
                mem::transmute::<dispatch_queue_t, dispatch_object_t>(self.0),
            ) as *mut AtomicBool;
            context.as_mut()
        }
    }

    fn set_should_cancel(&self, context: Box<AtomicBool>) {
        unsafe {
            let queue = mem::transmute::<dispatch_queue_t, dispatch_object_t>(self.0);
            // Leak the context from Box.
            dispatch_set_context(queue, Box::into_raw(context) as *mut c_void);

            extern "C" fn finalizer(context: *mut c_void) {
                // Retake the leaked context into box and then drop it.
                let _ = unsafe { Box::from_raw(context as *mut AtomicBool) };
            }

            // The `finalizer` is only run if the `context` in `queue` is set by `dispatch_set_context`.
            dispatch_set_finalizer_f(queue, Some(finalizer));
        }
    }

    fn release(&self) {
        unsafe {
            // This will release the inner `dispatch_queue_t` asynchronously.
            // TODO: It's incredibly unsafe to call `transmute` directly.
            //       Find another way to release the queue.
            dispatch_release(mem::transmute::<dispatch_queue_t, dispatch_object_t>(
                self.0,
            ));
        }
    }

    fn create_closure_and_executor<F>(closure: F) -> (*mut c_void, dispatch_function_t)
    where
        F: FnOnce(),
    {
        extern "C" fn closure_executer<F>(unboxed_closure: *mut c_void)
        where
            F: FnOnce(),
        {
            // Retake the leaked closure.
            let closure = unsafe { Box::from_raw(unboxed_closure as *mut F) };
            // Execute the closure.
            (*closure)();
            // closure is released after finishing this function call.
        }

        let closure = Box::new(closure); // Allocate closure on heap.
        let executor: dispatch_function_t = Some(closure_executer::<F>);

        (
            Box::into_raw(closure) as *mut c_void, // Leak the closure.
            executor,
        )
    }
}

impl Drop for Queue {
    fn drop(&mut self) {
        self.release();
    }
}

impl Clone for Queue {
    fn clone(&self) -> Self {
        // TODO: It's incredibly unsafe to call `transmute` directly.
        //       Find another way to release the queue.
        unsafe {
            dispatch_retain(mem::transmute::<dispatch_queue_t, dispatch_object_t>(
                self.0,
            ));
        }
        Self(self.0)
    }
}

#[test]
fn run_tasks_in_order() {
    let mut visited = Vec::<u32>::new();

    // Rust compilter doesn't allow a pointer to be passed across threads.
    // A hacky way to do that is to cast the pointer into a value, then
    // the value, which is actually an address, can be copied into threads.
    let ptr = &mut visited as *mut Vec<u32> as usize;

    fn visit(v: u32, visited_ptr: usize) {
        let visited = unsafe { &mut *(visited_ptr as *mut Vec<u32>) };
        visited.push(v);
    }

    let queue = Queue::new("Run tasks in order");

    queue.run_sync(move || visit(1, ptr));
    queue.run_sync(move || visit(2, ptr));
    queue.run_async(move || visit(3, ptr));
    queue.run_async(move || visit(4, ptr));
    // Call sync here to block the current thread and make sure all the tasks are done.
    queue.run_sync(move || visit(5, ptr));

    assert_eq!(visited, vec![1, 2, 3, 4, 5]);
}

#[test]
fn run_final_task() {
    let mut visited = Vec::<u32>::new();

    {
        // Rust compilter doesn't allow a pointer to be passed across threads.
        // A hacky way to do that is to cast the pointer into a value, then
        // the value, which is actually an address, can be copied into threads.
        let ptr = &mut visited as *mut Vec<u32> as usize;

        fn visit(v: u32, visited_ptr: usize) {
            let visited = unsafe { &mut *(visited_ptr as *mut Vec<u32>) };
            visited.push(v);
        }

        let queue = Queue::new("Task after run_final will be cancelled");

        queue.run_sync(move || visit(1, ptr));
        queue.run_async(move || visit(2, ptr));
        queue.run_final(move || visit(3, ptr));
        queue.run_async(move || visit(4, ptr));
        queue.run_sync(move || visit(5, ptr));
    }
    // `queue` will be dropped asynchronously and then the `finalizer` of the `queue`
    // should be fired to clean up the `context` set in the `queue`.

    assert_eq!(visited, vec![1, 2, 3]);
}
