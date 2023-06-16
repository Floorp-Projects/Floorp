/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ExtensionTestUtils } from "resource://testing-common/ExtensionXPCShellUtils.sys.mjs";

import { AddonTestUtils } from "resource://testing-common/AddonTestUtils.sys.mjs";

export const CookieXPCShellUtils = {
  init(scope) {
    AddonTestUtils.maybeInit(scope);
    ExtensionTestUtils.init(scope);
  },

  createServer(args) {
    const server = AddonTestUtils.createHttpServer(args);
    server.registerPathHandler("/", (metadata, response) => {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html", false);

      let body = "<body><h1>Hello world!</h1></body>";
      response.bodyOutputStream.write(body, body.length);
    });
    return server;
  },

  async loadContentPage(uri, options = {}) {
    return ExtensionTestUtils.loadContentPage(uri, options);
  },

  async getCookieStringFromDocument(uri, options = {}) {
    const contentPage = await this.loadContentPage(uri, options);
    const cookies = await contentPage.spawn(
      [],
      // eslint-disable-next-line no-undef
      () => content.document.cookie
    );
    await contentPage.close();
    return cookies;
  },

  async setCookieToDocument(uri, set, options = {}) {
    const contentPage = await this.loadContentPage(uri, options);
    await contentPage.spawn(
      [set],
      // eslint-disable-next-line no-undef
      cookies => (content.document.cookie = cookies)
    );
    await contentPage.close();
  },
};
