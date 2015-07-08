import json
import os
import site

# SelfServeMixin {{{1
class SelfServeMixin(object):
    def _get_session(self):
        site_packages_path = self.query_python_site_packages_path()
        site.addsitedir(site_packages_path)
        import requests
        session = requests.Session()
        adapter = requests.adapters.HTTPAdapter(max_retries=5)
        session.mount("http://", adapter)
        session.mount("https://", adapter)
        return session

    def _get_base_url(self):
        return self.config["selfserve_url"].rstrip("/")

    def trigger_nightly_builds(self, branch, revision, auth):
        session = self._get_session()

        selfserve_base = self._get_base_url()
        url = "%s/%s/rev/%s/nightly" % (selfserve_base, branch, revision)

        data = {
            "revision": revision,
        }
        self.info("Triggering nightly builds via %s" % url)
        return session.post(url, data=data, auth=auth).raise_for_status()

    def trigger_arbitrary_job(self, builder, branch, revision, auth, files=None):
        session = self._get_session()

        selfserve_base = self._get_base_url()
        url = "%s/%s/builders/%s/%s" % (selfserve_base, branch, builder, revision)

        data = {
            "properties": json.dumps({
                "branch": branch,
                "revision": revision
            }),
        }
        if files:
            data["files"] = json.dumps(files)

        self.info("Triggering arbritrary job at %s" % url)
        return session.post(url, data=data, auth=auth).raise_for_status()
