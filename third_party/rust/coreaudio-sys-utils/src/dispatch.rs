use coreaudio_sys::*;

use std::ffi::CString;
use std::mem;
use std::os::raw::c_void;
use std::panic;
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
#[cfg(test)]
use std::thread;
#[cfg(test)]
use std::time::Duration;
use std::time::Instant;

pub const DISPATCH_QUEUE_LABEL: &str = "org.mozilla.cubeb";

pub fn get_serial_queue_singleton() -> &'static Queue {
    static SERIAL_QUEUE: OnceLock<Queue> = OnceLock::new();
    SERIAL_QUEUE.get_or_init(|| Queue::new(DISPATCH_QUEUE_LABEL))
}

pub fn debug_assert_running_serially() {
    get_serial_queue_singleton().debug_assert_is_current();
}

pub fn debug_assert_not_running_serially() {
    get_serial_queue_singleton().debug_assert_is_not_current();
}

pub fn run_serially<F, B>(work: F) -> B
where
    F: FnOnce() -> B,
{
    get_serial_queue_singleton().run_sync(|| work()).unwrap()
}

pub fn run_serially_forward_panics<F, B>(work: F) -> B
where
    F: panic::UnwindSafe + FnOnce() -> B,
{
    match run_serially(|| panic::catch_unwind(|| work())) {
        Ok(res) => res,
        Err(e) => panic::resume_unwind(e),
    }
}

// Queue: A wrapper around `dispatch_queue_t` that is always serial.
// ------------------------------------------------------------------------------------------------
#[derive(Debug)]
pub struct Queue {
    queue: Mutex<dispatch_queue_t>,
    owned: AtomicBool,
}

impl Queue {
    pub fn new_with_target(label: &str, target: &Queue) -> Self {
        const DISPATCH_QUEUE_SERIAL: dispatch_queue_attr_t =
            ptr::null_mut::<dispatch_queue_attr_s>();
        let label = CString::new(label).unwrap();
        let c_string = label.as_ptr();
        let queue = {
            let target_guard = target.queue.lock().unwrap();
            Self {
                queue: Mutex::new(unsafe {
                    dispatch_queue_create_with_target(
                        c_string,
                        DISPATCH_QUEUE_SERIAL,
                        *target_guard,
                    )
                }),
                owned: AtomicBool::new(true),
            }
        };
        queue.set_should_cancel(Box::new(AtomicBool::new(false)));
        queue
    }

    pub fn new(label: &str) -> Self {
        Queue::new_with_target(label, &Queue::get_global_queue())
    }

    pub fn get_global_queue() -> Self {
        Self {
            queue: Mutex::new(unsafe { dispatch_get_global_queue(QOS_CLASS_DEFAULT as isize, 0) }),
            owned: AtomicBool::new(false),
        }
    }

    #[cfg(debug_assertions)]
    pub fn debug_assert_is_current(&self) {
        let guard = self.queue.lock().unwrap();
        unsafe {
            dispatch_assert_queue(*guard);
        }
    }

    #[cfg(not(debug_assertions))]
    pub fn debug_assert_is_current(&self) {}

    #[cfg(debug_assertions)]
    pub fn debug_assert_is_not_current(&self) {
        let guard = self.queue.lock().unwrap();
        unsafe {
            dispatch_assert_queue_not(*guard);
        }
    }

    #[cfg(not(debug_assertions))]
    pub fn debug_assert_is_not_current(&self) {}

    pub fn run_async<F>(&self, work: F)
    where
        F: Send + FnOnce(),
    {
        let guard = self.queue.lock().unwrap();
        let should_cancel = self.get_should_cancel(*guard);
        let (closure, executor) = Self::create_closure_and_executor(|| {
            if should_cancel.map_or(false, |v| v.load(Ordering::SeqCst)) {
                return;
            }
            work();
        });
        unsafe {
            dispatch_async_f(*guard, closure, executor);
        }
    }

    pub fn run_after<F>(&self, when: Instant, work: F)
    where
        F: Send + FnOnce(),
    {
        let now = Instant::now();
        if when <= now {
            return self.run_async(work);
        }
        let nanos = (when - now).as_nanos() as i64;
        let when = unsafe { dispatch_time(DISPATCH_TIME_NOW.into(), nanos) };
        let guard = self.queue.lock().unwrap();
        let should_cancel = self.get_should_cancel(*guard);
        let (closure, executor) = Self::create_closure_and_executor(|| {
            if should_cancel.map_or(false, |v| v.load(Ordering::SeqCst)) {
                return;
            }
            work();
        });
        unsafe {
            dispatch_after_f(when, *guard, closure, executor);
        }
    }

    pub fn run_sync<F, B>(&self, work: F) -> Option<B>
    where
        F: FnOnce() -> B,
    {
        let queue: Option<dispatch_queue_t>;
        let mut res: Option<B> = None;
        let cex: Option<(*mut c_void, dispatch_function_t)>;
        {
            let guard = self.queue.lock().unwrap();
            queue = Some(*guard);
            let should_cancel = self.get_should_cancel(*guard);
            cex = Some(Self::create_closure_and_executor(|| {
                if should_cancel.map_or(false, |v| v.load(Ordering::SeqCst)) {
                    return;
                }
                res = Some(work());
            }));
        }
        let (closure, executor) = cex.unwrap();
        unsafe {
            dispatch_sync_f(queue.unwrap(), closure, executor);
        }
        res
    }

    pub fn run_final<F, B>(&self, work: F) -> Option<B>
    where
        F: FnOnce() -> B,
    {
        assert!(
            self.owned.load(Ordering::SeqCst),
            "Doesn't make sense to finalize global queue"
        );
        let queue: Option<dispatch_queue_t>;
        let mut res: Option<B> = None;
        let cex: Option<(*mut c_void, dispatch_function_t)>;
        {
            let guard = self.queue.lock().unwrap();
            queue = Some(*guard);
            let should_cancel = self.get_should_cancel(*guard);
            debug_assert!(
                should_cancel.is_some(),
                "dispatch context should be allocated!"
            );
            cex = Some(Self::create_closure_and_executor(|| {
                res = Some(work());
                should_cancel
                    .expect("dispatch context should be allocated!")
                    .store(true, Ordering::SeqCst);
            }));
        }
        let (closure, executor) = cex.unwrap();
        unsafe {
            dispatch_sync_f(queue.unwrap(), closure, executor);
        }
        res
    }

    fn get_should_cancel(&self, queue: dispatch_queue_t) -> Option<&mut AtomicBool> {
        if !self.owned.load(Ordering::SeqCst) {
            return None;
        }
        unsafe {
            let context =
                dispatch_get_context(mem::transmute::<dispatch_queue_t, dispatch_object_t>(queue))
                    as *mut AtomicBool;
            context.as_mut()
        }
    }

    fn set_should_cancel(&self, context: Box<AtomicBool>) {
        assert!(self.owned.load(Ordering::SeqCst));
        unsafe {
            let guard = self.queue.lock().unwrap();
            let queue = mem::transmute::<dispatch_queue_t, dispatch_object_t>(*guard);
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
        let guard = self.queue.lock().unwrap();
        let queue = *guard;
        unsafe {
            // This will release the inner `dispatch_queue_t` asynchronously.
            // TODO: It's incredibly unsafe to call `transmute` directly.
            //       Find another way to release the queue.
            dispatch_release(mem::transmute::<dispatch_queue_t, dispatch_object_t>(queue));
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
        if self.owned.load(Ordering::SeqCst) {
            self.release();
        }
    }
}

impl Clone for Queue {
    fn clone(&self) -> Self {
        assert!(
            self.owned.load(Ordering::SeqCst),
            "No need to clone a static queue"
        );
        let guard = self.queue.lock().unwrap();
        let queue = *guard;
        // TODO: It's incredibly unsafe to call `transmute` directly.
        //       Find another way to release the queue.
        unsafe {
            dispatch_retain(mem::transmute::<dispatch_queue_t, dispatch_object_t>(queue));
        }
        Self {
            queue: Mutex::new(queue),
            owned: AtomicBool::new(true),
        }
    }
}

unsafe impl Send for Queue {}
unsafe impl Sync for Queue {}

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

    queue.run_sync(|| visit(1, ptr));
    queue.run_sync(|| visit(2, ptr));
    queue.run_async(|| visit(3, ptr));
    queue.run_async(|| visit(4, ptr));
    // Call sync here to block the current thread and make sure all the tasks are done.
    queue.run_sync(|| visit(5, ptr));

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

        queue.run_sync(|| visit(1, ptr));
        queue.run_async(|| visit(2, ptr));
        queue.run_final(|| visit(3, ptr));
        queue.run_async(|| visit(4, ptr));
        queue.run_sync(|| visit(5, ptr));
    }
    // `queue` will be dropped asynchronously and then the `finalizer` of the `queue`
    // should be fired to clean up the `context` set in the `queue`.

    assert_eq!(visited, vec![1, 2, 3]);
}

#[test]
fn sync_return_value() {
    let q = Queue::new("Test queue");
    assert_eq!(q.run_sync(|| 42), Some(42));
    assert_eq!(q.run_final(|| "foo"), Some("foo"));
    assert_eq!(q.run_sync(|| Ok::<(), u32>(())), None);
}

#[test]
fn run_after() {
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

        queue.run_async(|| visit(1, ptr));
        queue.run_after(Instant::now() + Duration::from_millis(10), || visit(2, ptr));
        queue.run_after(Instant::now() + Duration::from_secs(1), || visit(3, ptr));
        queue.run_async(|| visit(4, ptr));
        thread::sleep(Duration::from_millis(100));
        queue.run_final(|| visit(5, ptr));
    }
    // `queue` will be dropped asynchronously and then the `finalizer` of the `queue`
    // should be fired to clean up the `context` set in the `queue`.

    assert_eq!(visited, vec![1, 4, 2, 5]);
}
