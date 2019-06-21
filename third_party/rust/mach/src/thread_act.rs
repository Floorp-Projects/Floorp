//! This module corresponds to `mach/thread_act.defs`.

use kern_return::{kern_return_t};
use message::{mach_msg_type_number_t};
use thread_status::{thread_state_t, thread_state_flavor_t};
use types::{thread_act_t};

extern "C" {
    pub fn thread_get_state(target_act: thread_act_t,
                            flavor: thread_state_flavor_t,
                            new_state: thread_state_t,
                            new_state_count: *mut mach_msg_type_number_t)
        -> kern_return_t;
}