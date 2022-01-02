# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit

from telemetry_harness.ping_filters import (
    ANY_PING,
    DELETION_REQUEST_PING,
    MAIN_SHUTDOWN_PING,
)


def test_deletion_request_ping(browser, helpers):
    """Test the "deletion-request" ping behaviour across sessions"""
    # Get the client_id after installing an addon
    client_id = helpers.wait_for_ping(browser.install_addon, ANY_PING)["clientId"]

    # Make sure it's a valid UUID
    helpers.assert_is_valid_uuid(client_id)

    # Trigger a "deletion-request" ping.
    ping = helpers.wait_for_ping(browser.disable_telemetry, DELETION_REQUEST_PING)

    assert "clientId" in ping
    assert "payload" in ping
    assert "environment" not in ping["payload"]

    # Close Firefox cleanly.
    browser.quit(in_app=True)

    # Start Firefox.
    browser.start_session()

    # Trigger an environment change, which isn't allowed to send a ping.
    browser.install_addon()

    # Ensure we've sent no pings since "optout".
    assert browser.ping_server.pings[-1] == ping

    # Turn Telemetry back on.
    browser.enable_telemetry()

    # Close Firefox cleanly, collecting its "main"/"shutdown" ping.
    main_ping = helpers.wait_for_ping(browser.restart, MAIN_SHUTDOWN_PING)

    # Ensure the "main" ping has changed its client id.
    assert "clientId" in main_ping
    new_client_id = main_ping["clientId"]
    helpers.assert_is_valid_uuid(new_client_id)
    assert new_client_id != client_id

    # Ensure we note in the ping that the user opted in.
    parent_scalars = main_ping["payload"]["processes"]["parent"]["scalars"]

    assert "telemetry.data_upload_optin" in parent_scalars
    assert parent_scalars["telemetry.data_upload_optin"] is True

    # Ensure all pings sent during this test don't have the c0ffee client id.
    for ping in browser.ping_server.pings:
        if "clientId" in ping:
            helpers.assert_is_valid_uuid(ping["clientId"])


if __name__ == "__main__":
    mozunit.main()
