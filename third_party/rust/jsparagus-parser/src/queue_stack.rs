//! This module implements a Stack, which is useful for implementing a parser
//! with variable lookahead, as it would allow to pop elements which are below
//! the top-element, and maintain a top counter which would be in charge of
//! moving these elements once shifted.
use std::ptr;

/// This container implements a stack and a queue in a single vector:
///   - stack: buf[..top]
///   - queue: buf[top + gap..]
///
/// This structure is meant to avoid moving data when the head of the queue is
/// transfered to the top of the stack. Also, sometimes we need to set items
/// aside from the top of a stack, and then push them back onto the stack later.
/// The queue is for storing these set-aside values. Since they live in the same
/// buffer as the stack, values can be "set aside" and "pushed back on" without
/// moving them at all.
///
/// In the context of an LR parser, the stack contains shifted elements, and the
/// queue contains the lookahead. If the lexer is completely independent of the
/// parser, all tokens could be queued before starting the parser.
///
/// The following statements describe how this structure is meant to be used and
/// is described as a stack and a queue displayed as follow:
///    [...stack...] <gap> [...queue...]
///
/// New elements are always inserted in the queue with `enqueue`:
///    [a, b] <no gap> []
///    * enqueue(c)
///    [a, b] <no gap> [c]
///
/// These elements are then moved to the stack with `shift`:
///    [a, b] <no gap> [c]
///    * shift()
///    [a, b, c] <no gap> []
///
/// The stack top can be set aside in the queue with `unshift`:
///    [a, b, c] <no gap> []
///    * unshift()
///    [a, b] <no gap> [c]
///
/// The stack top can be removed with `pop`:
///    [a, b] <no gap> [c]
///    * pop() -> b
///    [a] <gap: 1> [c]
///    * pop() -> a
///    [] <gap: 2> [c]
///
/// New elements can be added to the front of the queue with `push_next`, which
/// also moves the content of the queue to ensure that `shift` can be used
/// afterward:
///    [] <gap: 2> [c]
///    * push_next(d)
///    [] <no gap> [d, c]
///
/// These operations are used by LR parser, to add lookahead with `enqueue`, to
/// shift tokens with `shift`, to save tokens to be replayed with `unshift`, to
/// reduce a set of tokens and replace it by a non-terminal with `pop` and
/// `push_next`.
pub struct QueueStack<T> {
    /// Buffer containing the stack and the queue.
    ///
    ///   [a, b, c, d, e, f, g, h, i, j]
    ///    '-----------'<------>'-----'
    ///       stack     ^  gap   queue
    ///                 |
    ///            top -'
    buf: Vec<T>,
    /// Length of the stack, self.buf[top - 1] being the last element of the
    /// stack.
    top: usize,
    /// Length of the gap between the stack top and the queue head.
    gap: usize,
}

impl<T> QueueStack<T> {
    /// Create a queue and stack with the given number of reserved elements.
    pub fn with_capacity(n: usize) -> QueueStack<T> {
        QueueStack {
            buf: Vec::with_capacity(n),
            top: 0,
            gap: 0,
        }
    }

    /// Add an element to the back of the queue.
    pub fn enqueue(&mut self, value: T) {
        self.buf.push(value);
    }

    /// Add an element to the front of the queue.
    pub fn push_next(&mut self, value: T) {
        self.compact_with_gap(1);
        self.gap -= 1;
        unsafe {
            // Write over the gap without reading nor dropping the old entry.
            let ptr = self.buf.as_mut_ptr().add(self.top + self.gap);
            ptr.write(value);
        }
    }

    /// Whether elements can be shifted.
    pub fn can_shift(&self) -> bool {
        self.gap == 0 && !self.queue_empty()
    }

    /// Whether elements can be unshifted.
    pub fn can_unshift(&self) -> bool {
        self.gap == 0 && !self.stack_empty()
    }

    /// Transfer an element from the top of the stack to the front of the queue.
    ///
    /// The gap must be empty. This does not move the value from one address to
    /// another in memory; it just adjusts the boundary between the stack and
    /// the queue.
    ///
    /// # Panics
    /// If the stack is empty or there is a gap.
    pub fn unshift(&mut self) {
        assert!(self.can_unshift());
        self.top -= 1;
    }

    /// Transfer an element from the front of the queue to the top of the stack.
    ///
    /// The gap must be empty. This does not move the value from one address to
    /// another in memory; it just adjusts the boundary between the stack and
    /// the queue.
    ///
    /// # Panics
    /// If the queue is empty or there is a gap.
    pub fn shift(&mut self) {
        assert!(self.can_shift());
        self.top += 1;
    }

    /// Remove the top element of the stack and return it, or None if the stack
    /// is empty.
    ///
    /// This increases the gap size by 1.
    pub fn pop(&mut self) -> Option<T> {
        if self.top == 0 {
            None
        } else {
            self.top -= 1;
            self.gap += 1;
            unsafe {
                // Take ownership of the content.
                let ptr = self.buf.as_mut_ptr().add(self.top);
                Some(ptr.read())
            }
        }
    }

    /// Set the gap size to `new_gap`, memmove-ing the contents of the queue as
    /// needed.
    fn compact_with_gap(&mut self, new_gap: usize) {
        assert!(new_gap <= (std::isize::MAX as usize));
        assert!(self.gap <= (std::isize::MAX as usize));
        let diff = new_gap as isize - self.gap as isize;
        if diff == 0 {
            return;
        }
        // Ensure there is enough capacity.
        if diff > 0 {
            self.buf.reserve(diff as usize);
        }
        // Number of elements to be copied.
        let count = self.queue_len();
        let new_len = self.top + new_gap + count;
        assert!(new_len < self.buf.capacity());
        unsafe {
            let src_ptr = self.buf.as_mut_ptr().add(self.top + self.gap);
            let dst_ptr = src_ptr.offset(diff);

            // Shift everything down/up to have the expected gap.
            ptr::copy(src_ptr, dst_ptr, count);

            // Update the buffer length to newly copied elements.
            self.buf.set_len(new_len);
            // Update the gap to the new gap value.
            self.gap = new_gap;
        }
        debug_assert_eq!(self.queue_len(), count);
    }

    /// Returns a reference to the front element of the queue.
    pub fn next(&self) -> Option<&T> {
        if self.queue_empty() {
            None
        } else {
            Some(&self.buf[self.top + self.gap])
        }
    }

    /// Returns a reference to the top element of the stack.
    #[allow(dead_code)]
    pub fn top(&self) -> Option<&T> {
        if self.top == 0 {
            None
        } else {
            Some(&self.buf[self.top - 1])
        }
    }

    /// Returns a mutable reference to the top of the stack.
    #[allow(dead_code)]
    pub fn top_mut(&mut self) -> Option<&mut T> {
        if self.top == 0 {
            None
        } else {
            Some(&mut self.buf[self.top - 1])
        }
    }

    /// Number of elements in the stack.
    pub fn stack_len(&self) -> usize {
        self.top
    }

    /// Number of elements in the queue.
    pub fn queue_len(&self) -> usize {
        self.buf.len() - self.top - self.gap
    }

    /// Whether the stack is empty.
    pub fn stack_empty(&self) -> bool {
        self.top == 0
    }

    /// Whether the queue is empty.
    pub fn queue_empty(&self) -> bool {
        self.top == self.buf.len()
    }

    /// Create a slice which corresponds the stack.
    pub fn stack_slice(&self) -> &[T] {
        &self.buf[..self.top]
    }

    /// Create a slice which corresponds the queue.
    #[allow(dead_code)]
    pub fn queue_slice(&self) -> &[T] {
        &self.buf[self.top + self.gap..]
    }
}

impl<T> Drop for QueueStack<T> {
    fn drop(&mut self) {
        // QueueStack contains a gap of non-initialized values, before releasing
        // the vector, we move all initialized values from the queue into the
        // remaining gap.
        self.compact_with_gap(0);
    }
}
