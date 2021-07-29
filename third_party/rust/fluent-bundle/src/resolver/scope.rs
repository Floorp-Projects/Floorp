use crate::bundle::FluentBundleBase;
use crate::memoizer::MemoizerKind;
use crate::resolver::{ResolveValue, ResolverError, WriteValue};
use crate::types::FluentValue;
use crate::{FluentArgs, FluentError, FluentResource};
use fluent_syntax::ast;
use std::borrow::Borrow;
use std::fmt;

/// State for a single `ResolveValue::to_value` call.
pub struct Scope<'scope, 'errors, R, M> {
    /// The current `FluentBundleBase` instance.
    pub bundle: &'scope FluentBundleBase<R, M>,
    /// The current arguments passed by the developer.
    pub(super) args: Option<&'scope FluentArgs<'scope>>,
    /// Local args
    pub(super) local_args: Option<FluentArgs<'scope>>,
    /// The running count of resolved placeables. Used to detect the Billion
    /// Laughs and Quadratic Blowup attacks.
    pub(super) placeables: u8,
    /// Tracks hashes to prevent infinite recursion.
    travelled: smallvec::SmallVec<[&'scope ast::Pattern<&'scope str>; 2]>,
    /// Track errors accumulated during resolving.
    pub errors: Option<&'errors mut Vec<FluentError>>,
    /// Makes the resolver bail.
    pub dirty: bool,
}

impl<'scope, 'errors, R, M: MemoizerKind> Scope<'scope, 'errors, R, M> {
    pub fn new(
        bundle: &'scope FluentBundleBase<R, M>,
        args: Option<&'scope FluentArgs>,
        errors: Option<&'errors mut Vec<FluentError>>,
    ) -> Self {
        Scope {
            bundle,
            args,
            local_args: None,
            placeables: 0,
            travelled: Default::default(),
            errors,
            dirty: false,
        }
    }

    pub fn add_error(&mut self, error: ResolverError) {
        if let Some(errors) = self.errors.as_mut() {
            errors.push(error.into());
        }
    }

    // This method allows us to lazily add Pattern on the stack,
    // only if the Pattern::resolve has been called on an empty stack.
    //
    // This is the case when pattern is called from Bundle and it
    // allows us to fast-path simple resolutions, and only use the stack
    // for placeables.
    pub fn maybe_track<W>(
        &mut self,
        w: &mut W,
        pattern: &'scope ast::Pattern<&str>,
        exp: &'scope ast::Expression<&str>,
    ) -> fmt::Result
    where
        R: Borrow<FluentResource>,
        W: fmt::Write,
    {
        if self.travelled.is_empty() {
            self.travelled.push(pattern);
        }
        exp.write(w, self)?;
        if self.dirty {
            w.write_char('{')?;
            exp.write_error(w)?;
            w.write_char('}')
        } else {
            Ok(())
        }
    }

    pub fn track<W>(
        &mut self,
        w: &mut W,
        pattern: &'scope ast::Pattern<&str>,
        exp: &ast::InlineExpression<&str>,
    ) -> fmt::Result
    where
        R: Borrow<FluentResource>,
        W: fmt::Write,
    {
        if self.travelled.contains(&pattern) {
            self.add_error(ResolverError::Cyclic);
            w.write_char('{')?;
            exp.write_error(w)?;
            w.write_char('}')
        } else {
            self.travelled.push(pattern);
            let result = pattern.write(w, self);
            self.travelled.pop();
            result
        }
    }

    pub fn write_ref_error<W>(
        &mut self,
        w: &mut W,
        exp: &ast::InlineExpression<&str>,
    ) -> fmt::Result
    where
        W: fmt::Write,
    {
        self.add_error(exp.into());
        w.write_char('{')?;
        exp.write_error(w)?;
        w.write_char('}')
    }

    pub fn get_arguments(
        &mut self,
        arguments: &'scope Option<ast::CallArguments<&'scope str>>,
    ) -> (Vec<FluentValue<'scope>>, FluentArgs<'scope>)
    where
        R: Borrow<FluentResource>,
    {
        let mut resolved_positional_args = Vec::new();
        let mut resolved_named_args = FluentArgs::new();

        if let Some(ast::CallArguments { named, positional }) = arguments {
            for expression in positional {
                resolved_positional_args.push(expression.resolve(self));
            }

            for arg in named {
                resolved_named_args.add(arg.name.name, arg.value.resolve(self));
            }
        }

        (resolved_positional_args, resolved_named_args)
    }
}
