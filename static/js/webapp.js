/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop:true*/

var loop = loop || {};
loop.webapp = (function() {
  "use strict";

  var router;

  /**
   * Base Backbone view.
   */
  var BaseView = Backbone.View.extend({
    hide: function() {
      this.$el.hide();
      return this;
    },

    show: function() {
      this.$el.show();
      return this;
    }
  });

  /**
   * Homepage view.
   */
  var HomeView = BaseView.extend({
    el: "#home"
  });

  /**
   * Call launcher view.
   */
  var CallLauncherView = BaseView.extend({
    el: "#call-launcher",

    events: {
      "click button": "launchCall"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.token) {
        throw new Error("missing required token");
      }
      this.token = options.token;
    },

    launchCall: function(event) {
      event.preventDefault();
      // XXX: request the loop server for call information using this.token
    }
  });

  /**
   * Call view.
   */
  var CallView = BaseView.extend({
    el: "#call"
  });

  /**
   * App Router. Allows defining a main active view and ease toggling it when
   * the active route changes.
   * @link http://mikeygee.com/blog/backbone.html
   */
  var Router = Backbone.Router.extend({
    view: undefined,

    routes: {
        "": "home",
        "call/:token": "call"
    },

    initialize: function() {
      this.loadView(new HomeView());
    },

    /**
     * Loads and render current active view.
     *
     * @param {BaseView} view View.
     */
    loadView : function(view) {
      this.view && this.view.hide();
      this.view = view.render().show();
    },

    /**
     * Main entry point.
     */
    home: function() {
      this.loadView(new HomeView());
    },

    /**
     * Call setup view.
     *
     * @param  {String} token Call token.
     */
    call: function(token) {
      this.loadView(new CallLauncherView({token: token}));
    }
  });

  /**
   * App initialization.
   */
  function init() {
    router = new Router();
    Backbone.history.start();
  }

  return {
    init: init,
    BaseView: BaseView,
    HomeView: HomeView,
    Router: Router,
    CallLauncherView: CallLauncherView
  };
})();
