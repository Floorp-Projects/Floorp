#include ../../netwerk/base/security-prefs.js
#include init/all.js
#ifdef MOZ_DATA_REPORTING
#include ../../services/datareporting/datareporting-prefs.js
#endif
#ifdef MOZ_SERVICES_HEALTHREPORT
#if MOZ_WIDGET_TOOLKIT == android
#include ../../mobile/android/chrome/content/healthreport-prefs.js
#else
#include ../../services/healthreport/healthreport-prefs.js
#endif
#endif
