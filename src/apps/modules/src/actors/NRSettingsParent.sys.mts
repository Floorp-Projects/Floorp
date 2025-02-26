//TODO: make reject when the name is invalid
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  receiveMessage(message) {
    switch (message.name) {
      case "getBoolPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_BOOL
        ) {
          return null;
        }
        return Services.prefs.getBoolPref(message.data.name);
      }
      case "getIntPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_INT
        ) {
          return null;
        }
        return Services.prefs.getIntPref(message.data.name);
      }
      case "getStringPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_STRING
        ) {
          return null;
        }
        return Services.prefs.getStringPref(message.data.name);
      }
      case "setBoolPref": {
        Services.prefs.setBoolPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
      case "setIntPref": {
        Services.prefs.setIntPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
      case "setStringPref": {
        Services.prefs.setStringPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
    }
  }
}
