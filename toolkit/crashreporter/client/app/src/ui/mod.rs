/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The UI model, UI implementations, and functions using them.
//!
//! UIs must implement:
//! * a `fn run_loop(&self, app: model::Application)` method which should display the UI and block while
//!   handling events until the application terminates,
//! * a `fn invoke(&self, f: model::InvokeFn)` method which invokes the given function
//!   asynchronously (without blocking) on the UI loop thread.

use crate::std::{rc::Rc, sync::Arc};
use crate::{
    async_task::AsyncTask, config::Config, data, logic::ReportCrash, settings::Settings, std,
    thread_bound::ThreadBound,
};
use model::{ui, Application};
use ui_impl::UI;

mod model;

#[cfg(all(not(test), any(target_os = "linux", target_os = "windows")))]
mod icon {
    // Must be DWORD-aligned for Win32 CreateIconFromResource.
    #[repr(align(4))]
    struct Aligned<Bytes: ?Sized>(Bytes);
    static PNG_DATA_ALIGNMENT: &'static Aligned<[u8]> =
        &Aligned(*include_bytes!("crashreporter.png"));
    pub static PNG_DATA: &'static [u8] = &PNG_DATA_ALIGNMENT.0;
}

#[cfg(test)]
pub mod test {
    pub mod model {
        pub use crate::ui::model::*;
    }
}

cfg_if::cfg_if! {
    if #[cfg(test)] {
        #[path = "test.rs"]
        pub mod ui_impl;
    } else if #[cfg(target_os = "linux")] {
        #[path = "gtk.rs"]
        mod ui_impl;
    } else if #[cfg(target_os = "windows")] {
        #[path = "windows/mod.rs"]
        mod ui_impl;
    } else if #[cfg(target_os = "macos")] {
        #[path = "macos/mod.rs"]
        mod ui_impl;
    } else {
        mod ui_impl {
            #[derive(Default)]
            pub struct UI;

            impl UI {
                pub fn run_loop(&self, _app: super::model::Application) {
                    unimplemented!();
                }

                pub fn invoke(&self, _f: super::model::InvokeFn) {
                    unimplemented!();
                }
            }
        }
    }
}

/// Display an error dialog with the given message.
#[cfg_attr(mock, allow(unused))]
pub fn error_dialog<M: std::fmt::Display>(config: &Config, message: M) {
    let close = data::Event::default();
    // Config may not have localized strings
    let string_or = |name, fallback: &str| {
        if config.strings.is_none() {
            fallback.into()
        } else {
            config.string(name)
        }
    };

    let details = if config.strings.is_none() {
        format!("Details: {}", message)
    } else {
        config
            .build_string("crashreporter-error-details")
            .arg("details", message.to_string())
            .get()
    };

    let window = ui! {
        Window title(string_or("crashreporter-branded-title", "Firefox Crash Reporter")) hsize(600) vsize(400)
            close_when(&close) halign(Alignment::Fill) valign(Alignment::Fill) {
            VBox margin(10) spacing(10) halign(Alignment::Fill) valign(Alignment::Fill) {
                Label text(string_or(
                        "crashreporter-error",
                        "The application had a problem and crashed. \
                        Unfortunately, the crash reporter is unable to submit a report for the crash."
                )),
                Label text(details),
                Button["close"] halign(Alignment::End) on_click(move || close.fire(&())) {
                    Label text(string_or("crashreporter-button-close", "Close"))
                }
            }
        }
    };

    UI::default().run_loop(Application {
        windows: vec![window],
        rtl: config.is_rtl(),
    });
}

#[derive(Default, Debug, PartialEq, Eq)]
pub enum SubmitState {
    #[default]
    Initial,
    InProgress,
    Success,
    Failure,
}

/// The UI for the main crash reporter windows.
pub struct ReportCrashUI {
    state: Arc<ThreadBound<ReportCrashUIState>>,
    ui: Arc<UI>,
    config: Arc<Config>,
    logic: Rc<AsyncTask<ReportCrash>>,
}

/// The state of the creash UI.
pub struct ReportCrashUIState {
    pub send_report: data::Synchronized<bool>,
    pub include_address: data::Synchronized<bool>,
    pub show_details: data::Synchronized<bool>,
    pub details: data::Synchronized<String>,
    pub comment: data::OnDemand<String>,
    pub submit_state: data::Synchronized<SubmitState>,
    pub close_window: data::Event<()>,
}

impl ReportCrashUI {
    pub fn new(
        initial_settings: &Settings,
        config: Arc<Config>,
        logic: AsyncTask<ReportCrash>,
    ) -> Self {
        let send_report = data::Synchronized::new(initial_settings.submit_report);
        let include_address = data::Synchronized::new(initial_settings.include_url);

        ReportCrashUI {
            state: Arc::new(ThreadBound::new(ReportCrashUIState {
                send_report,
                include_address,
                show_details: Default::default(),
                details: Default::default(),
                comment: Default::default(),
                submit_state: Default::default(),
                close_window: Default::default(),
            })),
            ui: Default::default(),
            config,
            logic: Rc::new(logic),
        }
    }

    pub fn async_task(&self) -> AsyncTask<ReportCrashUIState> {
        let state = self.state.clone();
        let ui = Arc::downgrade(&self.ui);
        AsyncTask::new(move |f| {
            let Some(ui) = ui.upgrade() else { return };
            ui.invoke(Box::new(cc! { (state) move || {
                f(state.borrow());
            }}));
        })
    }

    pub fn run(&self) {
        let ReportCrashUI {
            state,
            ui,
            config,
            logic,
        } = self;
        let ReportCrashUIState {
            send_report,
            include_address,
            show_details,
            details,
            comment,
            submit_state,
            close_window,
        } = state.borrow();

        send_report.on_change(cc! { (logic) move |v| {
            let v = *v;
            logic.push(move |s| s.settings.borrow_mut().submit_report = v);
        }});
        include_address.on_change(cc! { (logic) move |v| {
            let v = *v;
            logic.push(move |s| s.settings.borrow_mut().include_url = v);
        }});

        let input_enabled = submit_state.mapped(|s| s == &SubmitState::Initial);
        let send_report_and_input_enabled =
            data::Synchronized::join(send_report, &input_enabled, |s, e| *s && *e);

        let submit_status_text = submit_state.mapped(cc! { (config) move |s| {
            config.string(match s {
                SubmitState::Initial => "crashreporter-submit-status",
                SubmitState::InProgress => "crashreporter-submit-in-progress",
                SubmitState::Success => "crashreporter-submit-success",
                SubmitState::Failure => "crashreporter-submit-failure",
            })
        }});

        let progress_visible = submit_state.mapped(|s| s == &SubmitState::InProgress);

        let details_window = ui! {
            Window["crash-details-window"] title(config.string("crashreporter-view-report-title"))
                visible(show_details) modal(true) hsize(600) vsize(400)
                halign(Alignment::Fill) valign(Alignment::Fill)
            {
                VBox margin(10) spacing(10) halign(Alignment::Fill) valign(Alignment::Fill) {
                    Scroll halign(Alignment::Fill) valign(Alignment::Fill) {
                        TextBox["details-text"] content(details) halign(Alignment::Fill) valign(Alignment::Fill)
                    },
                    Button["close-details"] halign(Alignment::End) on_click(cc! { (show_details) move || *show_details.borrow_mut() = false }) {
                        Label text(config.string("crashreporter-button-ok"))
                    }
                }
            }
        };

        let main_window = ui! {
            Window title(config.string("crashreporter-branded-title")) hsize(600) vsize(400)
                halign(Alignment::Fill) valign(Alignment::Fill) close_when(close_window)
                child_window(details_window)
            {
                VBox margin(10) spacing(10) halign(Alignment::Fill) valign(Alignment::Fill) {
                    Label text(config.string("crashreporter-apology")) bold(true),
                    Label text(config.string("crashreporter-crashed-and-restore")),
                    Label text(config.string("crashreporter-plea")),
                    Checkbox["send"] checked(send_report) label(config.string("crashreporter-send-report"))
                        enabled(&input_enabled),
                    VBox margin_start(20) spacing(5) halign(Alignment::Fill) valign(Alignment::Fill) {
                        Button["details"] enabled(&send_report_and_input_enabled) on_click(cc! { (config, details, show_details, logic) move || {
                                // Immediately display the window to feel responsive, even if forming
                                // the details string takes a little while (it really shouldn't
                                // though).
                                *details.borrow_mut() = config.string("crashreporter-loading-details");
                                logic.push(|s| s.update_details());
                                *show_details.borrow_mut() = true;
                            }})
                        {
                            Label text(config.string("crashreporter-button-details"))
                        },
                        Scroll halign(Alignment::Fill) valign(Alignment::Fill) {
                            TextBox["comment"] placeholder(config.string("crashreporter-comment-prompt"))
                                content(comment)
                                editable(true)
                                enabled(&send_report_and_input_enabled)
                                halign(Alignment::Fill) valign(Alignment::Fill)
                        },
                        Checkbox["include-url"] checked(include_address)
                            label(config.string("crashreporter-include-url")) enabled(&send_report_and_input_enabled),
                        Label text(&submit_status_text) margin_top(20),
                        Progress halign(Alignment::Fill) visible(&progress_visible),
                    },
                    HBox valign(Alignment::End) halign(Alignment::End) spacing(10) affirmative_order(true)
                    {
                        Button["restart"] visible(config.restart_command.is_some())
                            on_click(cc! { (logic) move || logic.push(|s| s.restart()) })
                            enabled(&input_enabled) hsize(160)
                        {
                            Label text(config.string("crashreporter-button-restart"))
                        },
                        Button["quit"] on_click(cc! { (logic) move || logic.push(|s| s.quit()) })
                            enabled(&input_enabled) hsize(160)
                        {
                            Label text(config.string("crashreporter-button-quit"))
                        }
                    }
                }
            }
        };

        ui.run_loop(Application {
            windows: vec![main_window],
            rtl: config.is_rtl(),
        });
    }
}
