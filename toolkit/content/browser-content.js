/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* eslint no-unused-vars: ["error", {args: "none"}] */

/* exported Services */
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var { ActorManagerChild } = ChromeUtils.import(
  "resource://gre/modules/ActorManagerChild.jsm"
);

ActorManagerChild.attach(this);
