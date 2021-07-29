use crate::errors::L10nRegistryError;

pub trait ErrorReporter {
    fn report_errors(&self, errors: Vec<L10nRegistryError>);
}
