# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


__all__ = ["WebAuthn"]


class WebAuthn(object):
    def __init__(self, marionette):
        self.marionette = marionette

    def add_virtual_authenticator(self, config):
        body = {
            "protocol": config["protocol"],
            "transport": config["transport"],
            "hasResidentKey": config.get("hasResidentKey", False),
            "hasUserVerification": config.get("hasUserVerification", False),
            "isUserConsenting": config.get("isUserConsenting", True),
            "isUserVerified": config.get("isUserVerified", False),
        }
        return self.marionette._send_message(
            "WebAuthn:AddVirtualAuthenticator", body, key="value"
        )

    def remove_virtual_authenticator(self, authenticator_id):
        body = {"authenticatorId": authenticator_id}
        return self.marionette._send_message(
            "WebAuthn:RemoveVirtualAuthenticator", body
        )

    def add_credential(self, authenticator_id, credential):
        body = {
            "authenticatorId": authenticator_id,
            "credentialId": credential["credentialId"],
            "isResidentCredential": credential["isResidentCredential"],
            "rpId": credential["rpId"],
            "privateKey": credential["privateKey"],
            "userHandle": credential.get("userHandle"),
            "signCount": credential.get("signCount", 0),
        }
        return self.marionette._send_message("WebAuthn:AddCredential", body)

    def get_credentials(self, authenticator_id):
        body = {"authenticatorId": authenticator_id}
        return self.marionette._send_message(
            "WebAuthn:GetCredentials", body, key="value"
        )

    def remove_credential(self, authenticator_id, credential_id):
        body = {"authenticatorId": authenticator_id, "credentialId": credential_id}
        return self.marionette._send_message("WebAuthn:RemoveCredential", body)

    def remove_all_credentials(self, authenticator_id):
        body = {"authenticatorId": authenticator_id}
        return self.marionette._send_message("WebAuthn:RemoveAllCredentials", body)

    def set_user_verified(self, authenticator_id, uv):
        body = {
            "authenticatorId": authenticator_id,
            "isUserVerified": uv["isUserVerified"],
        }
        return self.marionette._send_message("WebAuthn:SetUserVerified", body)
