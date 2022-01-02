use fluent_bundle::FluentArgs;
use fluent_fallback::Localization;
use fluent_testing::get_scenarios;
use l10nregistry::fluent::FluentBundle;
use l10nregistry::registry::BundleAdapter;
use l10nregistry::testing::{RegistrySetup, TestFileFetcher};

#[derive(Clone)]
struct ScenarioBundleAdapter {}

impl BundleAdapter for ScenarioBundleAdapter {
    fn adapt_bundle(&self, bundle: &mut FluentBundle) {
        bundle.set_use_isolating(false);
        bundle
            .add_function("PLATFORM", |_positional, _named| "linux".into())
            .expect("Failed to add a function to the bundle.");
    }
}

#[test]
fn scenarios_test() {
    let fetcher = TestFileFetcher::new();

    let scenarios = get_scenarios();

    let adapter = ScenarioBundleAdapter {};

    for scenario in scenarios {
        println!("scenario: {}", scenario.name);
        let setup: RegistrySetup = (&scenario).into();
        let (env, reg) = fetcher.get_registry_and_environment_with_adapter(setup, adapter.clone());

        let loc = Localization::with_env(scenario.res_ids.clone(), true, env.clone(), reg);
        let bundles = loc.bundles();
        let mut errors = vec![];

        for query in scenario.queries.iter() {
            let args = query.input.args.as_ref().map(|args| {
                let mut result = FluentArgs::new();
                for arg in args.as_slice() {
                    result.set(arg.id.clone(), arg.value.clone());
                }
                result
            });
            if let Some(output) = &query.output {
                if let Some(value) = &output.value {
                    let v = bundles.format_value_sync(&query.input.id, args.as_ref(), &mut errors);
                    assert_eq!(v.unwrap().unwrap(), value.as_str());
                }
            }
            assert_eq!(errors, vec![]);
        }
        assert_eq!(env.errors(), vec![]);
    }
}
