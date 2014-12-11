/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Set of actors that expose the Web Animations API to devtools protocol clients.
 *
 * The |Animations| actor is the main entry point. It is used to discover
 * animation players on given nodes.
 * There should only be one instance per debugger server.
 *
 * The |AnimationPlayer| actor provides attributes and methods to inspect an
 * animation as well as pause/resume/seek it.
 *
 * The Web Animation spec implementation is ongoing in Gecko, and so this set
 * of actors should evolve when the implementation progresses.
 *
 * References:
 * - WebAnimation spec:
 *   http://w3c.github.io/web-animations/
 * - WebAnimation WebIDL files:
 *   /dom/webidl/Animation*.webidl
 */

const {ActorClass, Actor,
       FrontClass, Front,
       Arg, method, RetVal} = require("devtools/server/protocol");
const {NodeActor} = require("devtools/server/actors/inspector");

/**
 * The AnimationPlayerActor provides information about a given animation: its
 * startTime, currentTime, current state, etc.
 *
 * Since the state of a player changes as the animation progresses it is often
 * useful to call getCurrentState at regular intervals to get the current state.
 *
 * This actor also allows playing and pausing the animation.
 */
let AnimationPlayerActor = ActorClass({
  typeName: "animationplayer",

  /**
   * @param {AnimationsActor} The main AnimationsActor instance
   * @param {AnimationPlayer} The player object returned by getAnimationPlayers
   * @param {DOMNode} The node targeted by this player
   * @param {Number} Temporary work-around used to retrieve duration and
   * iteration count from computed-style rather than from waapi. This is needed
   * to know which duration to get, in case there are multiple css animations
   * applied to the same node.
   */
  initialize: function(animationsActor, player, node, playerIndex) {
    this.player = player;
    this.node = node;
    this.playerIndex = playerIndex;
    this.styles = node.ownerDocument.defaultView.getComputedStyle(node);
    Actor.prototype.initialize.call(this, animationsActor.conn);
  },

  destroy: function() {
    this.player = this.node = this.styles = null;
    Actor.prototype.destroy.call(this);
  },

  /**
   * Release the actor, when it isn't needed anymore.
   * Protocol.js uses this release method to call the destroy method.
   */
  release: method(function() {}, {release: true}),

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let data = this.getCurrentState();
    data.actor = this.actorID;

    return data;
  },

  /**
   * Get the animation duration from this player, in milliseconds.
   * Note that the Web Animations API doesn't yet offer a way to retrieve this
   * directly from the AnimationPlayer object, so for now, a duration is only
   * returned if found in the node's computed styles.
   * @return {Number}
   */
  getDuration: function() {
    let durationText;
    if (this.styles.animationDuration !== "0s") {
      durationText = this.styles.animationDuration;
    } else if (this.styles.transitionDuration !== "0s") {
      durationText = this.styles.transitionDuration;
    } else {
      return null;
    }

    if (durationText.indexOf(",") !== -1) {
      durationText = durationText.split(",")[this.playerIndex];
    }

    return parseFloat(durationText) * 1000;
  },

  /**
   * Get the animation iteration count for this player. That is, how many times
   * is the animation scheduled to run.
   * Note that the Web Animations API doesn't yet offer a way to retrieve this
   * directly from the AnimationPlayer object, so for now, check for
   * animationIterationCount in the node's computed styles, and return that.
   * This style property defaults to 1 anyway.
   * @return {Number}
   */
  getIterationCount: function() {
    let iterationText = this.styles.animationIterationCount;
    if (iterationText.indexOf(",") !== -1) {
      iterationText = iterationText.split(",")[this.playerIndex];
    }

    return parseInt(iterationText, 10);
  },

  /**
   * Get the current state of the AnimationPlayer (currentTime, playState, ...).
   * Note that the initial state is returned as the form of this actor when it
   * is initialized.
   * @return {Object}
   */
  getCurrentState: method(function() {
    return {
      /**
       * Return the player's current startTime value.
       * Will be null whenever the animation is paused or waiting to start.
       */
      startTime: this.player.startTime,
      currentTime: this.player.currentTime,
      playState: this.player.playState,
      name: this.player.source.effect.name,
      duration: this.getDuration(),
      iterationCount: this.getIterationCount(),
      /**
       * Is the animation currently running on the compositor. This is important for
       * developers to know if their animation is hitting the fast path or not.
       * Currently this will only be true for Firefox OS though (where we have
       * compositor animations enabled).
       * Returns false whenever the animation is paused as it is taken off the
       * compositor then.
       */
      isRunningOnCompositor: this.player.isRunningOnCompositor
    };
  }, {
    request: {},
    response: {
      data: RetVal("json")
    }
  }),

  /**
   * Pause the player.
   */
  pause: method(function() {
    this.player.pause();
  }, {
    request: {},
    response: {}
  }),

  /**
   * Play the player.
   */
  play: method(function() {
    this.player.play();
  }, {
    request: {},
    response: {}
  })
});

let AnimationPlayerFront = FrontClass(AnimationPlayerActor, {
  initialize: function(conn, form, detail, ctx) {
    Front.prototype.initialize.call(this, conn, form, detail, ctx);
  },

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
  },

  destroy: function() {
    Front.prototype.destroy.call(this);
  },

  /**
   * Getter for the initial state of the player. Up to date states can be
   * retrieved by calling the getCurrentState method.
   */
  get initialState() {
    return {
      startTime: this._form.startTime,
      currentTime: this._form.currentTime,
      playState: this._form.playState,
      name: this._form.name,
      duration: this._form.duration,
      iterationCount: this._form.iterationCount,
      isRunningOnCompositor: this._form.isRunningOnCompositor
    }
  }
});

/**
 * The Animations actor lists animation players for a given node.
 */
let AnimationsActor = exports.AnimationsActor = ActorClass({
  typeName: "animations",

  initialize: function(conn, tabActor) {
    Actor.prototype.initialize.call(this, conn);
  },

  destroy: function() {
    Actor.prototype.destroy.call(this);
  },

  /**
   * Since AnimationsActor doesn't have a protocol.js parent actor that takes
   * care of its lifetime, implementing disconnect is required to cleanup.
   */
  disconnect: function() {
    this.destroy();
  },

  /**
   * Retrieve the list of AnimationPlayerActor actors corresponding to
   * currently running animations for a given node.
   * @param {NodeActor} nodeActor The NodeActor type is defined in
   * /toolkit/devtools/server/actors/inspector
   */
  getAnimationPlayersForNode: method(function(nodeActor) {
    let players = nodeActor.rawNode.getAnimationPlayers();

    let actors = [];
    for (let i = 0; i < players.length; i ++) {
      // XXX: for now the index is passed along as the AnimationPlayerActor uses
      // it to retrieve animation information from CSS.
      actors.push(AnimationPlayerActor(this, players[i], nodeActor.rawNode, i));
    }

    return actors;
  }, {
    request: {
      actorID: Arg(0, "domnode")
    },
    response: {
      players: RetVal("array:animationplayer")
    }
  })
});

let AnimationsFront = exports.AnimationsFront = FrontClass(AnimationsActor, {
  initialize: function(client, {animationsActor}) {
    Front.prototype.initialize.call(this, client, {actor: animationsActor});
    this.manage(this);
  },

  destroy: function() {
    Front.prototype.destroy.call(this);
  }
});
