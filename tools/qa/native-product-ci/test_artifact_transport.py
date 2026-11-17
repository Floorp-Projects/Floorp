# SPDX-License-Identifier: MPL-2.0
import unittest
import urllib.request
import artifact_transport as t
import test_artifact_preflight as fixtures

class TransportTests(unittest.TestCase):
    def test_lock_must_match_approved_producer(self):
        f=fixtures.PreflightTests();f.setUp()
        lock={'schemaVersion':1,'source':{'repository':t.preflight.RUNTIME,'commit':f.pin['runtime_sha']}}
        t.verify_pr_lock(lock,f.pin)
        lock['source']['commit']='d'*40
        with self.assertRaises(ValueError):t.verify_pr_lock(lock,f.pin)
    def test_credential_only_on_fixed_api_request(self):
        req=t.api_request('/repos/Floorp-Projects/Floorp-Runtime/actions/artifacts/1/zip','fixture-token')
        self.assertEqual(urllib.parse.urlsplit(req.full_url).hostname,'api.github.com')
        self.assertEqual(req.get_header('Authorization'),'Bearer fixture-token')
        with self.assertRaises(ValueError):t.api_request('https://example.invalid','fixture-token')
        with self.assertRaises(ValueError):t.api_request('/repos/example\nHost: other','fixture-token')
    def test_authenticated_request_never_follows_redirect(self):
        req=t.api_request('/repos/Floorp-Projects/Floorp-Runtime/actions/artifacts/1/zip','fixture-token')
        self.assertIsNone(t.NoRedirect().redirect_request(req,None,302,'',{},'https://example.invalid/signed'))
if __name__=='__main__':unittest.main()
