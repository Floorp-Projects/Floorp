import pytest

@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_experiment_unenrolls_via_studies_toggle(setup_experiment, gradlewbuild, load_branches):
    setup_experiment(load_branches)
    gradlewbuild.test("GenericExperimentIntegrationTest#disableStudiesViaStudiesToggle")
    gradlewbuild.test("GenericExperimentIntegrationTest#testExperimentUnenrolls")

@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_experiment_unenrolls_via_secret_menu(setup_experiment, gradlewbuild, load_branches):
    setup_experiment(load_branches)
    gradlewbuild.test("GenericExperimentIntegrationTest#testExperimentUnenrollsViaSecretMenu")
