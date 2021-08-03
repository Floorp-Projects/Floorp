use super::{BundleAdapter, L10nRegistry, L10nRegistryLocked};
use crate::env::ErrorReporter;
use crate::errors::L10nRegistryError;
use crate::fluent::{FluentBundle, FluentError};
use crate::solver::{SerialProblemSolver, SyncTester};
use fluent_fallback::generator::BundleIterator;

use unic_langid::LanguageIdentifier;

impl<'a, B> L10nRegistryLocked<'a, B> {
    pub(crate) fn bundle_from_order<P>(
        &self,
        locale: LanguageIdentifier,
        source_order: &[usize],
        res_ids: &[String],
        error_reporter: &P,
    ) -> Option<Result<FluentBundle, (FluentBundle, Vec<FluentError>)>>
    where
        P: ErrorReporter,
        B: BundleAdapter,
    {
        let mut bundle = FluentBundle::new(vec![locale.clone()]);

        if let Some(bundle_adapter) = self.bundle_adapter {
            bundle_adapter.adapt_bundle(&mut bundle);
        }

        let mut errors = vec![];

        for (&source_idx, path) in source_order.iter().zip(res_ids.iter()) {
            let source = self.source_idx(source_idx);
            if let Some(res) = source.fetch_file_sync(&locale, path, /* overload */ true) {
                if source.options.allow_override {
                    bundle.add_resource_overriding(res);
                } else if let Err(err) = bundle.add_resource(res) {
                    errors.extend(err.into_iter().map(|error| {
                        L10nRegistryError::FluentError {
                            path: path.clone(),
                            loc: None,
                            error,
                        }
                    }));
                }
            } else {
                return None;
            }
        }

        if !errors.is_empty() {
            error_reporter.report_errors(errors);
        }
        Some(Ok(bundle))
    }
}

impl<P, B> L10nRegistry<P, B>
where
    P: Clone,
    B: Clone,
{
    pub fn generate_bundles_for_lang_sync(
        &self,
        langid: LanguageIdentifier,
        resource_ids: Vec<String>,
    ) -> GenerateBundlesSync<P, B> {
        let lang_ids = vec![langid];

        GenerateBundlesSync::new(self.clone(), lang_ids.into_iter(), resource_ids)
    }

    pub fn generate_bundles_sync(
        &self,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<String>,
    ) -> GenerateBundlesSync<P, B> {
        GenerateBundlesSync::new(self.clone(), locales, resource_ids)
    }
}

enum State {
    Empty,
    Locale(LanguageIdentifier),
    Solver {
        locale: LanguageIdentifier,
        solver: SerialProblemSolver,
    },
}

impl Default for State {
    fn default() -> Self {
        Self::Empty
    }
}

impl State {
    fn get_locale(&self) -> &LanguageIdentifier {
        match self {
            Self::Locale(locale) => locale,
            Self::Solver { locale, .. } => locale,
            Self::Empty => unreachable!(),
        }
    }

    fn take_solver(&mut self) -> SerialProblemSolver {
        replace_with::replace_with_or_default_and_return(self, |self_| match self_ {
            Self::Solver { locale, solver } => (solver, Self::Locale(locale)),
            _ => unreachable!(),
        })
    }

    fn put_back_solver(&mut self, solver: SerialProblemSolver) {
        replace_with::replace_with_or_default(self, |self_| match self_ {
            Self::Locale(locale) => Self::Solver { locale, solver },
            _ => unreachable!(),
        })
    }
}

pub struct GenerateBundlesSync<P, B> {
    reg: L10nRegistry<P, B>,
    locales: std::vec::IntoIter<LanguageIdentifier>,
    res_ids: Vec<String>,
    state: State,
}

impl<P, B> GenerateBundlesSync<P, B> {
    fn new(
        reg: L10nRegistry<P, B>,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        res_ids: Vec<String>,
    ) -> Self {
        Self {
            reg,
            locales,
            res_ids,
            state: State::Empty,
        }
    }
}

impl<P, B> SyncTester for GenerateBundlesSync<P, B> {
    fn test_sync(&self, res_idx: usize, source_idx: usize) -> bool {
        let locale = self.state.get_locale();
        let res = &self.res_ids[res_idx];
        self.reg
            .lock()
            .source_idx(source_idx)
            .fetch_file_sync(locale, res, /* overload */ true)
            .is_some()
    }
}

impl<P, B> BundleIterator for GenerateBundlesSync<P, B>
where
    P: ErrorReporter,
{
    fn prefetch_sync(&mut self) {
        if let State::Solver { .. } = self.state {
            let mut solver = self.state.take_solver();
            if let Err(idx) = solver.try_next(self, true) {
                self.reg
                    .shared
                    .provider
                    .report_errors(vec![L10nRegistryError::MissingResource {
                        locale: self.state.get_locale().clone(),
                        res_id: self.res_ids[idx].clone(),
                    }]);
            }
            self.state.put_back_solver(solver);
            return;
        }

        if let Some(locale) = self.locales.next() {
            let mut solver = SerialProblemSolver::new(self.res_ids.len(), self.reg.lock().len());
            self.state = State::Locale(locale.clone());
            if let Err(idx) = solver.try_next(self, true) {
                self.reg
                    .shared
                    .provider
                    .report_errors(vec![L10nRegistryError::MissingResource {
                        locale,
                        res_id: self.res_ids[idx].clone(),
                    }]);
            }
            self.state.put_back_solver(solver);
        }
    }
}

impl<P, B> Iterator for GenerateBundlesSync<P, B>
where
    P: ErrorReporter,
    B: BundleAdapter,
{
    type Item = Result<FluentBundle, (FluentBundle, Vec<FluentError>)>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let State::Solver { .. } = self.state {
                let mut solver = self.state.take_solver();
                match solver.try_next(self, false) {
                    Ok(Some(order)) => {
                        let locale = self.state.get_locale();
                        let bundle = self.reg.lock().bundle_from_order(
                            locale.clone(),
                            order,
                            &self.res_ids,
                            &self.reg.shared.provider,
                        );
                        self.state.put_back_solver(solver);
                        if bundle.is_some() {
                            return bundle;
                        } else {
                            continue;
                        }
                    }
                    Ok(None) => {}
                    Err(idx) => {
                        self.reg.shared.provider.report_errors(vec![
                            L10nRegistryError::MissingResource {
                                locale: self.state.get_locale().clone(),
                                res_id: self.res_ids[idx].clone(),
                            },
                        ]);
                    }
                }
                self.state = State::Empty;
            }

            let locale = self.locales.next()?;
            let solver = SerialProblemSolver::new(self.res_ids.len(), self.reg.lock().len());
            self.state = State::Solver { locale, solver };
        }
    }
}
