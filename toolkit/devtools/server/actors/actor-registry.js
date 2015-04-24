/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/server/protocol");
const { method, custom, Arg, Option, RetVal } = protocol;

const { Cu, CC, components } = require("chrome");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const Services = require("Services");
const { DebuggerServer } = require("devtools/server/main");
const ActorRegistryUtils = require("devtools/server/actors/utils/actor-registry-utils");
const { registerActor, unregisterActor } = ActorRegistryUtils;

loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

/**
 * The ActorActor gives you a handle to an actor you've dynamically
 * registered and allows you to unregister it.
 */
const ActorActor = protocol.ActorClass({
  typeName: "actorActor",

  initialize: function (conn, options) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.options = options;
  },

  unregister: method(function () {
    unregisterActor(this.options);
  }, {
    request: {},
    response: {}
  })
});

const ActorActorFront = protocol.FrontClass(ActorActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.ActorActorFront = ActorActorFront;

/*
 * The ActorRegistryActor allows clients to define new actors on the
 * server. This is particularly useful for addons.
 */
const ActorRegistryActor = protocol.ActorClass({
  typeName: "actorRegistry",

  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  registerActor: method(function (sourceText, fileName, options) {
    registerActor(sourceText, fileName, options);

    let { constructor, type } = options;

    return ActorActor(this.conn, {
      name: constructor,
      tab: type.tab,
      global: type.global
    });
  }, {
    request: {
      sourceText: Arg(0, "string"),
      filename: Arg(1, "string"),
      options: Arg(2, "json")
    },

    response: {
      actorActor: RetVal("actorActor")
    }
  })
});

exports.ActorRegistryActor = ActorRegistryActor;

function request(uri) {
  return new Promise((resolve, reject) => {
    try {
      uri = Services.io.newURI(uri, null, null);
    } catch (e) {
      reject(e);
    }

    NetUtil.asyncFetch({
      uri,
      loadUsingSystemPrincipal: true,
     }, (stream, status, req) => {
      if (!components.isSuccessCode(status)) {
        reject(new Error("Request failed with status code = "
                         + status
                         + " after NetUtil.asyncFetch for url = "
                         + uri));
        return;
      }

      let source = NetUtil.readInputStreamToString(stream, stream.available());
      stream.close();
      resolve(source);
    });
  });
}

const ActorRegistryFront = protocol.FrontClass(ActorRegistryActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client,
      { actor: form.actorRegistryActor });

    this.manage(this);
  },

  registerActor: custom(function (uri, options) {
    return request(uri, options)
      .then(sourceText => {
        return this._registerActor(sourceText, uri, options);
      });
  }, {
    impl: "_registerActor"
  })
});
exports.ActorRegistryFront = ActorRegistryFront;
