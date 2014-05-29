/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

// XXX This file needs unit tests.

var loop = loop || {};
loop.shared = loop.shared || {};
loop.shared.views = (function(TB) {
  "use strict";

  /**
   * Base Backbone view.
   */
  var BaseView = Backbone.View.extend({
    /**
     * Hides view element.
     *
     * @return {BaseView}
     */
    hide: function() {
      this.$el.hide();
      return this;
    },

    /**
     * Shows view element.
     *
     * @return {BaseView}
     */
    show: function() {
      this.$el.show();
      return this;
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = BaseView.extend({
    el: "#conversation",

    initialize: function() {
      this.videoStyles = { width: "100%", height: "100%" };

      // XXX this feels like to be moved to the ConversationModel, but as it's
      // tighly coupled with the DOM (element ids to receive streams), we'd need
      // an abstraction we probably don't want yet.
      this.session   = TB.initSession(this.model.get("sessionId"));
      this.publisher = TB.initPublisher(this.model.get("apiKey"), "outgoing",
                                        this.videoStyles);

      this.session.connect(this.model.get("apiKey"),
                           this.model.get("sessionToken"));

      this.listenTo(this.session, "sessionConnected", this._sessionConnected);
      this.listenTo(this.session, "streamCreated", this._streamCreated);
      this.listenTo(this.session, "connectionDestroyed", this._sessionEnded);
    },

    _sessionConnected: function(event) {
      this.session.publish(this.publisher);
      this._subscribeToStreams(event.streams);
    },

    _streamCreated: function(event) {
      this._subscribeToStreams(event.streams);
    },

    _sessionEnded: function(event) {
      // XXX: better end user notification
      alert("Your session has ended. Reason: " + event.reason);
      this.model.trigger("session:ended");
    },

    _subscribeToStreams: function(streams) {
      streams.forEach(function(stream) {
        if (stream.connection.connectionId !==
            this.session.connection.connectionId) {
          this.session.subscribe(stream, "incoming", this.videoStyles);
        }
      }.bind(this));
    }
  });

  return {
    BaseView: BaseView,
    ConversationView: ConversationView
  };
})(window.TB);
