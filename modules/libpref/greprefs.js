#include ../../netwerk/base/security-prefs.js
#include init/all.js
#ifdef MOZ_DATA_REPORTING
#include ../../toolkit/components/telemetry/datareporting-prefs.js
#endif
#ifdef MOZ_SERVICES_HEALTHREPORT
#if MOZ_WIDGET_TOOLKIT == android
#include ../../mobile/android/chrome/content/healthreport-prefs.js
#else
#include ../../toolkit/components/telemetry/healthreport-prefs.js
#endif
#endif
