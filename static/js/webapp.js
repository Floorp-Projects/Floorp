/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

var loop = loop || {};
loop.webapp = (function($, TB) {
  "use strict";

  /**
   * Base Loop server URL.
   *
   * XXX: should be configurable, but how?
   *
   * @type {String}
   */
  var baseApiUrl = "http://localhost:5000";

  /**
   * App router.
   * @type {loop.webapp.Router}
   */
  var router;

  /**
   * Current conversation model instance.
   * @type {loop.shared.models.ConversationModel}
   */
  var conversation;

  /**
   * Homepage view.
   */
  var HomeView = loop.shared.views.BaseView.extend({
    el: "#home"
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var ConversationFormView = loop.shared.views.BaseView.extend({
    el: "#conversation-form",

    events: {
      "submit": "initiate"
    },

    initialize: function() {
      this.listenTo(this.model, "session:error", function(error) {
        // XXX: display a proper error notification to end user, probably
        //      reusing the BB notification system from the Loop desktop client.
        alert(error);
      });
    },

    initiate: function(event) {
      event.preventDefault();
      this.model.initiate(baseApiUrl);
    }
  });

  /**
   * App Router. Allows defining a main active view and ease toggling it when
   * the active route changes.
   * @link http://mikeygee.com/blog/backbone.html
   */
  var Router = Backbone.Router.extend({
    _conversation: undefined,
    activeView: undefined,

    routes: {
      "": "home",
      "call/ongoing": "conversation",
      "call/:token": "initiate"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.conversation) {
        throw new Error("missing required conversation");
      }
      this._conversation = options.conversation;

      this.listenTo(this._conversation, "session:ready", this._onSessionReady);
      this.listenTo(this._conversation, "session:ended", this._onSessionEnded);

      // Load default view
      this.loadView(new HomeView());
    },

    /**
     * Navigates to conversation when the call session is ready.
     */
    _onSessionReady: function() {
      this.navigate("call/ongoing", {trigger: true});
    },

    /**
     * Navigates back to initiate when the call session has ended.
     */
    _onSessionEnded: function() {
      this.navigate("call/" + this._conversation.get("token"), {trigger: true});
    },

    /**
     * Loads and render current active view.
     *
     * @param {loop.shared.BaseView} view View.
     */
    loadView : function(view) {
      if (this.activeView) {
        this.activeView.hide();
      }
      this.activeView = view.render().show();
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.loadView(new HomeView());
    },

    /**
     * Loads conversation launcher view, setting the received conversation token
     * to the current conversation model.
     *
     * @param  {String} loopToken Loop conversation token.
     */
    initiate: function(loopToken) {
      this._conversation.set("loopToken", loopToken);
      this.loadView(new ConversationFormView({model: this._conversation}));
    },

    /**
     * Loads conversation establishment view.
     *
     */
    conversation: function() {
      if (!this._conversation.isSessionReady()) {
        var loopToken = this._conversation.get("loopToken");
        if (loopToken) {
          return this.navigate("call/" + loopToken, {trigger: true});
        } else {
          // XXX: notify user that a call token is missing
          return this.navigate("home", {trigger: true});
        }
      }
      this.loadView(
        new loop.shared.views.ConversationView({model: this._conversation}));
    }
  });

  /**
   * App initialization.
   */
  function init() {
    conversation = new loop.shared.models.ConversationModel();
    router = new Router({conversation: conversation});
    Backbone.history.start();
  }

  return {
    ConversationFormView: ConversationFormView,
    HomeView: HomeView,
    init: init,
    Router: Router
  };
})(jQuery, window.TB);
