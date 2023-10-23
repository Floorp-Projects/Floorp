import pytest

@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_experiment_unenrolls_via_studies_toggle(setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment):
    setup_experiment(load_branches)
    gradlewbuild.test("GenericExperimentIntegrationTest#disableStudiesViaStudiesToggle")
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])
    gradlewbuild.test("GenericExperimentIntegrationTest#testExperimentUnenrolled")
    assert check_ping_for_experiment(reason="unenrollment", branch=load_branches[0])

@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_experiment_unenrolls_via_secret_menu(setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment):
    setup_experiment(load_branches)
    gradlewbuild.test("GenericExperimentIntegrationTest#testExperimentUnenrolledViaSecretMenu")
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])
    gradlewbuild.test("GenericExperimentIntegrationTest#testExperimentUnenrolled")
    assert check_ping_for_experiment(reason="unenrollment", branch=load_branches[0])
