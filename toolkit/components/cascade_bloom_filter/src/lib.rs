/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate nserror;
extern crate nsstring;
#[macro_use]
extern crate rental;
extern crate rust_cascade;
extern crate thin_vec;
#[macro_use]
extern crate xpcom;

use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_ERROR_NOT_INITIALIZED, NS_OK};
use nsstring::nsACString;
use rust_cascade::Cascade;
use std::cell::RefCell;
use thin_vec::ThinVec;
use xpcom::interfaces::nsICascadeFilter;
use xpcom::{xpcom_method, RefPtr};

// Cascade does not take ownership of the data, so we must own the data in order to pass its
// reference to Cascade.
rental! {
    mod rentals {
        use super::Cascade;

        #[rental]
        pub struct CascadeWithOwnedData {
            owndata: Box<[u8]>,
            cascade: Box<Cascade<'owndata>>,
        }
    }
}

#[derive(xpcom)]
#[xpimplements(nsICascadeFilter)]
#[refcnt = "nonatomic"]
pub struct InitCascadeFilter {
    filter: RefCell<Option<rentals::CascadeWithOwnedData>>,
}

impl CascadeFilter {
    fn new() -> RefPtr<CascadeFilter> {
        CascadeFilter::allocate(InitCascadeFilter {
            filter: RefCell::new(None),
        })
    }
    xpcom_method!(set_filter_data => SetFilterData(data: *const ThinVec<u8>));

    fn set_filter_data(&self, data: &ThinVec<u8>) -> Result<(), nsresult> {
        let filter = rentals::CascadeWithOwnedData::try_new_or_drop(
            Vec::from(data.as_slice()).into_boxed_slice(),
            |data| {
                Cascade::from_bytes(data)
                    .unwrap_or(None)
                    .ok_or(NS_ERROR_INVALID_ARG)
            },
        )?;
        self.filter.borrow_mut().replace(filter);
        Ok(())
    }

    xpcom_method!(has => Has(key: *const nsACString) -> bool);

    fn has(&self, key: &nsACString) -> Result<bool, nsresult> {
        match self.filter.borrow().as_ref() {
            None => Err(NS_ERROR_NOT_INITIALIZED),
            Some(filter) => Ok(filter.rent(|cascade| cascade.has(&*key))),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn cascade_filter_construct(result: &mut *const nsICascadeFilter) {
    let inst: RefPtr<CascadeFilter> = CascadeFilter::new();
    *result = inst.coerce::<nsICascadeFilter>();
    std::mem::forget(inst);
}
