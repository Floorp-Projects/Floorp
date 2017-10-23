import os
import sys
import base64
from OpenSSL import crypto

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.script import BaseScript
from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.mozilla.signed_certificate_timestamp import SignedCertificateTimestamp


class CTSubmitter(BaseScript, VirtualenvMixin):
    config_options = virtualenv_config_options

    config_options = [
        [["--chain"], {
            "dest": "chain",
            "help": "URL from which to download the cert chain to be submitted to CT (in PEM format)"
        }],
        [["--log"], {
            "dest": "log",
            "help": "URL for the log to which the chain should be submitted"
        }],
        [["--sct"], {
            "dest": "sct",
            "help": "File where the SCT from the log should be written"
        }],
    ]

    def __init__(self):
        BaseScript.__init__(self,
            config_options=self.config_options,
            config={
                "virtualenv_modules": [
                    "pem",
                    "redo",
                    "requests",
                ],
                "virtualenv_path": "venv",
            },
            require_config_file=False,
            all_actions=["add-chain"],
            default_actions=["add-chain"],
        )

        self.chain_url = self.config["chain"]
        self.log_url = self.config["log"]
        self.sct_filename = self.config["sct"]

    def add_chain(self):
        from redo import retry
        import requests
        import pem

        def get_chain():
            r = requests.get(self.chain_url)
            r.raise_for_status()
            return r.text

        chain = retry(get_chain)

        req = { "chain": [] }
        chain = pem.parse(chain)
        for i in range(len(chain)):
            cert = crypto.load_certificate(crypto.FILETYPE_PEM, str(chain[i]))
            der = crypto.dump_certificate(crypto.FILETYPE_ASN1, cert)
            req["chain"].append(base64.b64encode(der))

        def post_chain():
            r = requests.post(self.log_url + '/ct/v1/add-chain', json=req)
            r.raise_for_status()
            return r.json()

        resp = retry(post_chain)
        sct = SignedCertificateTimestamp(resp)
        self.write_to_file(self.sct_filename, sct.to_rfc6962())

if __name__ == "__main__":
    myScript = CTSubmitter()
    myScript.run_and_exit()
