/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

trait Logger {
    fn log(&self, message: String);
    fn finished(&self);
}

fn log_even_numbers(logger: Box<dyn Logger>, items: Vec<i32>) {
    for i in items {
        if i % 2 == 0 {
            logger.log(format!("Saw even number: {i}"))
        }
    }
    logger.finished();
}

fn log_even_numbers_main_thread(logger: Box<dyn Logger>, items: Vec<i32>) {
    log_even_numbers(logger, items)
}

include!(concat!(env!("OUT_DIR"), "/callbacks.uniffi.rs"));
