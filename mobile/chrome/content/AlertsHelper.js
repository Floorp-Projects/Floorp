var AlertsHelper = {
  _timeoutID: -1,
  _listener: null,
  _cookie: "",
  _clickable: false,
  get container() {
    delete this.container;
    let container = document.getElementById("alerts-container");

    // Move the popup on the other side if we are in RTL
    let [leftSidebar, rightSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    if (leftSidebar.left > rightSidebar.left) {
      container.removeAttribute("right");
      container.setAttribute("left", "0");
    }

    let self = this;
    container.addEventListener("transitionend", function() {
      self.alertTransitionOver();
    }, true);

    return this.container = container;
  },

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    this._clickable = aTextClickable || false;
    this._listener = aListener || null;
    this._cookie = aCookie || "";

    document.getElementById("alerts-image").setAttribute("src", aImageURL);
    document.getElementById("alerts-title").value = aTitle;
    document.getElementById("alerts-text").textContent = aText;

    let container = this.container;
    container.hidden = false;
    container.height = container.getBoundingClientRect().height;
    container.classList.add("showing");

    let timeout = Services.prefs.getIntPref("alerts.totalOpenTime");
    let self = this;
    if (this._timeoutID)
      clearTimeout(this._timeoutID);
    this._timeoutID = setTimeout(function() { self._timeoutAlert(); }, timeout);
  },

  _timeoutAlert: function ah__timeoutAlert() {
    this._timeoutID = -1;

    this.container.classList.remove("showing");
    if (this._listener)
      this._listener.observe(null, "alertfinished", this._cookie);
  },

  alertTransitionOver: function ah_alertTransitionOver() {
    let container = this.container;
    if (!container.classList.contains("showing")) {
      container.height = 0;
      container.hidden = true;
    }
  },

  click: function ah_click(aEvent) {
    if (this._clickable && this._listener)
      this._listener.observe(null, "alertclickcallback", this._cookie);

    if (this._timeoutID != -1) {
      clearTimeout(this._timeoutID);
      this._timeoutAlert();
    }
  }
};
