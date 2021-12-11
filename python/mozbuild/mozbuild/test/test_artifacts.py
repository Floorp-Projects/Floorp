# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import mozunit

from unittest import TestCase

from mozbuild.artifacts import ArtifactJob, ThunderbirdMixin


class FakeArtifactJob(ArtifactJob):
    package_re = r""


class TestArtifactJob(TestCase):
    def _assert_candidate_trees(self, version_display, expected_trees):
        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = version_display

        job = FakeArtifactJob()
        self.assertGreater(len(job.candidate_trees), 0)
        self.assertEqual(job.candidate_trees, expected_trees)

    def test_candidate_trees_with_empty_file(self):
        self._assert_candidate_trees(
            version_display="", expected_trees=ArtifactJob.default_candidate_trees
        )

    def test_candidate_trees_with_beta_version(self):
        self._assert_candidate_trees(
            version_display="92.1b2", expected_trees=ArtifactJob.beta_candidate_trees
        )

    def test_candidate_trees_with_esr_version(self):
        self._assert_candidate_trees(
            version_display="91.3.0esr", expected_trees=ArtifactJob.esr_candidate_trees
        )

    def test_candidate_trees_with_nightly_version(self):
        self._assert_candidate_trees(
            version_display="95.0a1", expected_trees=ArtifactJob.nightly_candidate_trees
        )

    def test_candidate_trees_with_release_version(self):
        self._assert_candidate_trees(
            version_display="93.0.1", expected_trees=ArtifactJob.default_candidate_trees
        )

    def test_candidate_trees_with_newline_before_version(self):
        self._assert_candidate_trees(
            version_display="\n\n91.3.0esr",
            expected_trees=ArtifactJob.esr_candidate_trees,
        )

    def test_property_is_cached(self):
        job = FakeArtifactJob()
        expected_trees = ArtifactJob.esr_candidate_trees

        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = "91.3.0.esr"
        self.assertEqual(job.candidate_trees, expected_trees)
        # Because the property is cached, changing the
        # `MOZ_APP_VERSION_DISPLAY` won't have any impact.
        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = ""
        self.assertEqual(job.candidate_trees, expected_trees)


class FakeThunderbirdJob(ThunderbirdMixin, FakeArtifactJob):
    pass


class TestThunderbirdMixin(TestCase):
    def _assert_candidate_trees(self, version_display, expected_trees):
        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = version_display

        job = FakeThunderbirdJob()
        self.assertGreater(len(job.candidate_trees), 0)
        self.assertEqual(job.candidate_trees, expected_trees)

    def test_candidate_trees_with_beta_version(self):
        self._assert_candidate_trees(
            version_display="92.1b2",
            expected_trees=ThunderbirdMixin.beta_candidate_trees,
        )

    def test_candidate_trees_with_esr_version(self):
        self._assert_candidate_trees(
            version_display="91.3.0esr",
            expected_trees=ThunderbirdMixin.esr_candidate_trees,
        )

    def test_candidate_trees_with_nightly_version(self):
        self._assert_candidate_trees(
            version_display="95.0a1",
            expected_trees=ThunderbirdMixin.nightly_candidate_trees,
        )

    def test_property_is_cached(self):
        job = FakeThunderbirdJob()
        expected_trees = ThunderbirdMixin.esr_candidate_trees

        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = "91.3.0.esr"
        self.assertEqual(job.candidate_trees, expected_trees)
        # Because the property is cached, changing the
        # `MOZ_APP_VERSION_DISPLAY` won't have any impact.
        buildconfig.substs["MOZ_APP_VERSION_DISPLAY"] = ""
        self.assertEqual(job.candidate_trees, expected_trees)


if __name__ == "__main__":
    mozunit.main()
