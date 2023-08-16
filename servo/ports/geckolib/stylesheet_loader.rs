/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use cssparser::SourceLocation;
use nsstring::nsCString;
use servo_arc::Arc;
use style::context::QuirksMode;
use style::gecko::data::GeckoStyleSheet;
use style::gecko_bindings::bindings;
use style::gecko_bindings::bindings::Gecko_LoadStyleSheet;
use style::gecko_bindings::structs::{Loader, LoaderReusableStyleSheets};
use style::gecko_bindings::structs::{
    SheetLoadData, SheetLoadDataHolder, StyleSheet as DomStyleSheet,
};
use style::gecko_bindings::sugar::refptr::RefPtr;
use style::global_style_data::GLOBAL_STYLE_DATA;
use style::media_queries::MediaList;
use style::parser::ParserContext;
use style::shared_lock::{Locked, SharedRwLock};
use style::stylesheets::import_rule::{ImportLayer, ImportSheet, ImportSupportsCondition};
use style::stylesheets::AllowImportRules;
use style::stylesheets::{ImportRule, Origin, StylesheetLoader as StyleStylesheetLoader};
use style::stylesheets::{StylesheetContents, UrlExtraData};
use style::use_counters::UseCounters;
use style::values::CssUrl;

pub struct StylesheetLoader(
    *mut Loader,
    *mut DomStyleSheet,
    *mut SheetLoadData,
    *mut LoaderReusableStyleSheets,
);

impl StylesheetLoader {
    pub fn new(
        loader: *mut Loader,
        parent: *mut DomStyleSheet,
        parent_load_data: *mut SheetLoadData,
        reusable_sheets: *mut LoaderReusableStyleSheets,
    ) -> Self {
        StylesheetLoader(loader, parent, parent_load_data, reusable_sheets)
    }
}

impl StyleStylesheetLoader for StylesheetLoader {
    fn request_stylesheet(
        &self,
        url: CssUrl,
        source_location: SourceLocation,
        _context: &ParserContext,
        lock: &SharedRwLock,
        media: Arc<Locked<MediaList>>,
        supports: Option<ImportSupportsCondition>,
        layer: ImportLayer,
    ) -> Arc<Locked<ImportRule>> {
        // Ensure the supports conditions for this @import are true, if not, refuse to load
        if !supports.as_ref().map_or(true, |s| s.enabled) {
            return Arc::new(lock.wrap(ImportRule {
                url,
                stylesheet: ImportSheet::new_refused(),
                supports,
                layer,
                source_location,
            }));
        }

        // After we get this raw pointer ImportRule will be moved into a lock and Arc
        // and so the Arc<Url> pointer inside will also move,
        // but the Url it points to or the allocating backing the String inside that Url won’t,
        // so this raw pointer will still be valid.

        let child_sheet =
            unsafe { Gecko_LoadStyleSheet(self.0, self.1, self.2, self.3, &url, media.into()) };

        debug_assert!(
            !child_sheet.is_null(),
            "Import rules should always have a strong sheet"
        );
        let sheet = unsafe { GeckoStyleSheet::from_addrefed(child_sheet) };
        let stylesheet = ImportSheet::new(sheet);
        Arc::new(lock.wrap(ImportRule {
            url,
            stylesheet,
            supports,
            layer,
            source_location,
        }))
    }
}

pub struct AsyncStylesheetParser {
    load_data: RefPtr<SheetLoadDataHolder>,
    extra_data: UrlExtraData,
    bytes: nsCString,
    origin: Origin,
    quirks_mode: QuirksMode,
    should_record_use_counters: bool,
    allow_import_rules: AllowImportRules,
}

impl AsyncStylesheetParser {
    pub fn new(
        load_data: RefPtr<SheetLoadDataHolder>,
        extra_data: UrlExtraData,
        bytes: nsCString,
        origin: Origin,
        quirks_mode: QuirksMode,
        should_record_use_counters: bool,
        allow_import_rules: AllowImportRules,
    ) -> Self {
        AsyncStylesheetParser {
            load_data,
            extra_data,
            bytes,
            origin,
            quirks_mode,
            should_record_use_counters,
            allow_import_rules,
        }
    }

    pub fn parse(self) {
        let global_style_data = &*GLOBAL_STYLE_DATA;
        let input: &str = unsafe { (*self.bytes).as_str_unchecked() };

        let use_counters = if self.should_record_use_counters {
            Some(Box::new(UseCounters::default()))
        } else {
            None
        };

        // Note: Parallel CSS parsing doesn't report CSS errors. When errors are
        // being logged, Gecko prevents the parallel parsing path from running.
        let sheet = StylesheetContents::from_str(
            input,
            self.extra_data.clone(),
            self.origin,
            &global_style_data.shared_lock,
            Some(&self),
            None,
            self.quirks_mode.into(),
            use_counters.as_deref(),
            self.allow_import_rules,
            /* sanitized_output = */ None,
        );

        unsafe {
            bindings::Gecko_StyleSheet_FinishAsyncParse(
                self.load_data.get(),
                sheet.into(),
                use_counters.map_or(std::ptr::null_mut(), Box::into_raw),
            );
        }
    }
}

impl StyleStylesheetLoader for AsyncStylesheetParser {
    fn request_stylesheet(
        &self,
        url: CssUrl,
        source_location: SourceLocation,
        _context: &ParserContext,
        lock: &SharedRwLock,
        media: Arc<Locked<MediaList>>,
        supports: Option<ImportSupportsCondition>,
        layer: ImportLayer,
    ) -> Arc<Locked<ImportRule>> {
        // Ensure the supports conditions for this @import are true, if not, refuse to load
        if !supports.as_ref().map_or(true, |s| s.enabled) {
            return Arc::new(lock.wrap(ImportRule {
                url: url.clone(),
                stylesheet: ImportSheet::new_refused(),
                supports,
                layer,
                source_location,
            }));
        }

        let stylesheet = ImportSheet::new_pending();
        let rule = Arc::new(lock.wrap(ImportRule {
            url: url.clone(),
            stylesheet,
            supports,
            layer,
            source_location,
        }));

        unsafe {
            bindings::Gecko_LoadStyleSheetAsync(
                self.load_data.get(),
                &url,
                media.into(),
                rule.clone().into(),
            );
        }

        rule
    }
}
