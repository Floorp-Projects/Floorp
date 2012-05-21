/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


document.addEventListener('load', function() {
  window.setInterval(qaNotifications.updateNotificationStore(), 3600000);
}, false)

var qaNotifications = {
  storageService : Components.classes["@mozilla.org/storage/service;1"]
                        .getService(Ci.mozIStorageService),
  dbHandle : null,

  openDatabase : function() {
    var file = Cc["@mozilla.org/file/directory_service;1"]
                     .getService(Ci.nsIProperties)
                     .get("ProfD", Ci.nsIFile);
    file.append("mozqa.sqlite");
      this.dbHandle = this.storageService.openDatabase(file);
      this.createNotificationTable();
    },

    closeDatabase : function() {
      // there's no way to actually close a database, but setting
      // the db reference to null will cause the db handle to become
      // elegible for GC which is about as good as it gets
      dbHandle = null;
    },

    createNotificationTable : function() {
      this.db.executeSimpleSQL("CREATE TABLE if not exists notifications ( \
        id text, \
        datetime integer, \
        firstNotification integer, \
        secondNotification integer, \
        serializedJSData string)");
    },

    updateNotificationStore : function() {
      var req = new XMLHttpRequest();
      var url = qaPref.getPref('qa.extension.hermes.url', 'char');
      req.open('GET', url, true);
      req.onreadystatechange = function(evt) {
        if (req.readyState == 4 && req.status == 200) {
          if (req.responseXML.getElementsByTagName('notifications') == null)
          return;
        var notifs = req.responseXML.getElementsByTagName('notification');
        for (var i=0; i<notifs.length; i++) {
          var notif = notifs[i];
          notif = new Notification('xml', notif);
          notif.serializeToDb();
        }
        }
      };
      req.send(null);
    },

    checkNotificationStatus : function() {
      // see if we are elegible for notification:
      var time = qaPref.getPref(qaPref.prefBase+'.lastNotificationCheckTime', 'int');
      var interval = qaPref.getPref(qaPref.prefBase+'.minNotificationCheckInterval', 'int');
      if (time + interval > new Date().valueOf())
        return; // nothing to see here, try again later

      var sth = this.db.createStatement("SELECT * FROM notifications");
      var notifications = [];
      try {
      while (sth.executeStep()) {
        notifications.push(new Notification('json', MochiKit.Base.evalJSON(
          sth.getString(4))));
      }
    } finally {
        sth.reset();
      }

      if (notifications[0] && notifications[0].okToShow() == true) {
        notifications[0].displayToBox();
        $('qa-notify').removeAttribute('hidden');
      }
      // reset the interval timer
      qaPref.setPref(qaPref.prefBase+'.lastNotificationCheckTime', new Date().valueOf(), 'int');
    },

    showHideNotify: function(bool) {
    var notify = $('qa-notify');
    if (bool == true) {
      notify.removeAttribute("hidden");
    } else {
      notify.setAttribute("hidden", "true");
    }
  },

  getNotificationSettings : function(bool) {
    var prefs = qaPref.getPref(qaPref.prefBase+'.notificationSettings', 'char');
    return prefs.split(",");
  },
};
qaNotifications.__defineGetter__('db', function() {
  if (this.dbHandle != null)
    return this.dbHandle;
  else {
    this.openDatabase();
    return this.dbHandle;
  }
});

function Notification() {

}

function Notification(type, data) {
  if (type == 'json') {
    this.id = data.id;
    this.notificationClass = data.notificationClass;
    this.type = data.type;
    this.headline = data.headline;
    this.datetime = MochiKit.DateTime.isoTimestamp(data.datetime);
    this.place = data.place;
    this.infotext = data.infotext;
    this.infolinktext = data.infolinktext;
    this.infolinkhref = data.infolinkhref;
    this.golinktext = data.golinktext;
    this.golinkhref = data.golinkhref;
  } else if (type == 'xml') {
    var notif = data;
    this.id = notif.getAttribute('id');
    this.notificationClass = notif.getAttribute('class');
    this.type = notif.getAttribute('type');
    if (notif.getElementsByTagName('headline')[0] != null)
      this.headline = notif.getElementsByTagName('headline')[0].textContent;

    // eventinfo
    if (this.notificationClass == 'event') {
      var eventInfo = notif.getElementsByTagName('eventinfo')[0];
      this.datetime = MochiKit.DateTime.isoTimestamp(
        eventInfo.getElementsByTagName('datetime')[0].textContent);
      this.place = eventInfo.getElementsByTagName('place')[0].textContent;
    }
    this.infotext = notif.getElementsByTagName('infotext')[0].textContent;
    this.infolinktext = notif.getElementsByTagName('infolink')[0].textContent;
    this.infolinkhref = notif.getElementsByTagName('infolink')[0]
      .getAttribute('href');
    this.golinktext = notif.getElementsByTagName('golink')[0].textContent;
    this.golinkhref = notif.getElementsByTagName('golink')[0]
      .getAttribute('href');
  }
}

Notification.prototype = {
  id : null,
  notificationClass : null,
  type : null,
  headline : null,
  datetime : null,
  place : null,
  infotext : null,
  infolinktext : null,
  infolinkhref : null,
  golinktext : null,
  golinkhref : null,

  hasHadFirstNotification : false,
  hasHadSecondNotification : false,
  serializedJSData : null,

  serializeToDb: function() {
    var query = qaNotifications.db.createStatement("SELECT id FROM notifications \
      WHERE id = ?1");
    query.bindStringParameter(0, this.id);
    var foundRow;
    while (query.executeStep()) {
      foundRow = 1;
    }
    query.reset();
    if (foundRow == 1) { // it's already been inserted, so update
      return;
    }

    var sth = qaNotifications.db.createStatement("INSERT into notifications \
      values (?1, ?2, ?3, ?4, ?5)");
    sth.bindStringParameter(0, this.id);
    sth.bindStringParameter(1, this.datetime);
    sth.bindStringParameter(2, this.hasHadFirstNotification);
    sth.bindStringParameter(3, this.hasHadSecondNotification);

    // avoid having serialized data inside serialized data:
    this.serializedJSData = null;
    var jsData = MochiKit.Base.serializeJSON(this);
    sth.bindStringParameter(4, jsData);

    sth.execute();
  },
  displayToBox : function() {
    $('qa-notify-header').value=this.headline;

    if ($('qa-notify-text').firstChild)
      $('qa-notify-text').removeChild($('qa-notify-text').firstChild);
    $('qa-notify-text').appendChild(document.createTextNode(this.infotext));

    $('qa-notify-infolink').value=this.infolinktext;
    $('qa-notify-infolink').href=this.infolinkhref;
    $('qa-notify-infolink').style.display = '';

    if (this.notificationClass == 'event') {
      var inprogress = 0;
      $('qa-notify-event-datetime').value=this.datetime;
      $('qa-notify-event-place').value=this.place;
      $('qa-notify-event').style.display = '';
      $('qa-notify-inprogress').style.display = 'none';

      $('qa-notify-getstartedlink').value=this.golinktext;
      $('qa-notify-getstartedlink').href=this.golinkhref;

      if (inprogress == 1) {
        $('qa-notify-inprogress').style.display = '';
        $('qa-notify-getstartedlink').style.display = '';
        $('qa-notify-infolink').style.display = 'none';
      }
    }
  },
  okToShow : function() {
    var prefs = qaNotifications.getNotificationSettings();
    if (prefs[0] == '1') // all notifications disabled
      return false;
    if (this.type == 'testday' && ! prefs[1] == '0')
      return true;
    if (this.type == 'bugday' && ! prefs[2] == '0')
      return true;
    if (this.type == 'prerelease' && ! prefs[3] == '0')
      return true;
    if (this.type == 'news' && ! prefs[4] == '0')
      return true;
    if (this.type == 'newbuilds' && ! prefs[5] == '0')
      return true;
    if (this.type == 'special' && ! prefs[6] == '0')
      return true;

    // catch all:
    return false;
  }
};
