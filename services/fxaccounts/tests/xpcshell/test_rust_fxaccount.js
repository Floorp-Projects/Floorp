/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the RustFxAccounts bridging class.
// Note that we do not test everything, as this
// "should" be covered by the Rust tests.
// We are simply assessing that the Rust XPCOM
// and the Viaduct plumbing work!

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { RustFxAccount } = ChromeUtils.import(
  "resource://gre/modules/RustFxAccount.js"
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_RUST_FXA_CLIENT,
  },
  async function test_authenticated_get_request() {
    let serverCalls = 0;
    const server = httpd_setup({
      "/.well-known/openid-configuration": function(request, response) {
        response.setStatusLine(request.httpVersion, 200, "OK");
        const body = JSON.stringify({
          authorization_endpoint: "https://foo.bar/authorization",
          introspection_endpoint: "https://oauth.foo.bar/v1/introspect",
          issuer: "https://foo.bar",
          jwks_uri: "https://oauth.foo.bar/v1/jwks",
          revocation_endpoint: "https://oauth.foo.bar/v1/destroy",
          token_endpoint: "https://oauth.foo.bar/v1/token",
          userinfo_endpoint: "https://profile.foo.bar/v1/profile",
        });
        response.bodyOutputStream.write(body, body.length);
        serverCalls++;
      },
      "/.well-known/fxa-client-configuration": function(request, response) {
        response.setStatusLine(request.httpVersion, 200, "OK");
        const body = JSON.stringify({
          auth_server_base_url: "https://api.foo.bar",
          oauth_server_base_url: "https://oauth.foo.bar",
          pairing_server_base_uri: "wss://channelserver.services.mozilla.com",
          profile_server_base_url: "https://profile.foo.bar",
          sync_tokenserver_base_url: "https://token.services.mozilla.com",
        });
        response.bodyOutputStream.write(body, body.length);
        serverCalls++;
      },
    });

    const fxa = new RustFxAccount({
      fxaServer: server.baseURI,
      clientId: "abcd",
      redirectUri: "https://foo.bar/1234",
    });
    const oauthFlowUrl = await fxa.beginOAuthFlow(["profile"]);
    Assert.equal(2, serverCalls);
    Assert.ok(oauthFlowUrl.startsWith("https://foo.bar/authorization"));

    await promiseStopServer(server);
  }
);
