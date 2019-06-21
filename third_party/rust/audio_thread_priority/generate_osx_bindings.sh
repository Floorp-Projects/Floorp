#!/bin/sh

bindgen /usr/include/mach/thread_policy.h --no-layout-tests --whitelist-type "(thread_policy_flavor_t|thread_policy_t|thread_extended_policy_data_t|thread_time_constraint_policy_data_t|thread_precedence_policy_data_t|thread_t)" --whitelist-var "(THREAD_TIME_CONSTRAINT_POLICY|THREAD_EXTENDED_POLICY|THREAD_PRECEDENCE_POLICY)" > src/mach_sys.rs
