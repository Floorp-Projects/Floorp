# Application error handling support

This crate provides support for other crates to effectively report errors.
Because app-services components get embedded in various apps, written in
multiple languages, we face several challenges:

  - Rust stacktraces are generally not available so we often need to provide
    extra context to help debug where an error is occurring.
  - Without stack traces, Sentry and other error reporting systems don't do a
    great job at auto-grouping errors together, so we need to manually group them.
  - We can't hook directly into the error reporting system or even depend on a
    particular error reporting system to be in use.  This means the system
    needs to be simple and flexible enough to plug in to multiple systems.

## Breadcrumbs as context

We use a breadcrumb system as the basis for adding context to errors.
Breadcrumbs are individual messages that form a log-style stream, and the most
recent breadcrumbs get attached to each error reported.  There are a lot of
other ways to provide context to an error, but we just use breadcrumbs for
everything because it's a relatively simple system that's easy to hook up to
error reporting platforms.

## Basic error reporting tools

Basic error reporting is handled using several macros:

  - `report_error!()` creates an error report.  It inputs a `type_name` as the
    first parameter, and `format!` style arguments afterwards. `type_name` is
    used to group errors together and show up as error titles/headers.  Use the
    format-style args to create is a long-form description for the issue.  Most
    of the time you won't need to call this directly, since can automatically
    do it when converting internal results to public ones. However, it can be
    useful in the case where you see an error that you want
    to report, but want to try to recover rather than returning an error.
  - `breadcrumb!()` creates a new breadcrumb that will show up on future errors.
  - `trace_error!()` inputs a `Result<>` and creates a breadcrumb if it's an
    `Err`.  This is useful if when you're trying to track down where an error
    originated from, since you can wrap each possible source of the error with
    `trace_error!()`.  `trace_error!()` returns the result passed in to it,
    which makes wrapping function calls easy.


## Public/Internal errors and converting between them

Our components generally create 2 error enums: one for internal use and one for
the public API.  They are typically named `Error` and
`[ComponentName]ApiError`.  The internal error typically carries a lot of
low-level details and lots of variants which is useful to us app-services
developers.  The public error typically has less variants with the variants
often just storing a reason string rather than low-level error codes.  There
are also two `Result<>` types that correspond to these two errors, typically
named `Result` and `ApiResult`.

This means we need to convert from internal errors to public errors, which has
the nice side benefit of giving us a centralized spot to make error reports for
selected public errors.  This is done with the `ErrorHandling` type and
`GetErrorHandling` trait in `src/handling.rs`.  The basic system is that you
convert between one error to another and choose if you want to report the error
and/or log a warning.  When reporting an error you can choose a type name to
group the error with.  This system is extremely flexible, since you can inspect
the internal error and use error codes or other data to determine if it should
be reported or not, which type name to report it with, etc. Eventually we also
hope to allow expected errors to be counted in telemetry (think things like
network errors, shutdown errors, etc.).

To assist this conversion, the `handle_error` procedural macro can be used to
automatically convert between `Result` and `ApiResult` using
`GetErrorHandling`.  Note that this depends on having the `Result` type
imported in your module with a `use` statement.

See the `logins::errors` and `logins::store` modules for an example of how this
all fits together.

## ⚠️  Personally Identifiable Information ⚠️

When converting internal errors to public errors, we should ensure that there
is no personally identifying information (PII) in any error reports.  We should
also ensure that no PII is contained in the public error enum, since consumers
may end up uses those for their own error reports.

We operate on a best-effort basis to ensure this.  Our error details often come
from an error from one of our dependencies, which makes it very difficult to be
completely sure though. For example, `rusqlite::Error` could include data from
a user's database in their errors, which would then appear in our error
variants. However, we've never seen that in practice so we are comfortable
including the `rusqlite` error message in our error reports, without attempting
to sanitize them.

