/**
 * NOTE: Do not modify this file by hand.
 * Content was generated from source metrics.yaml files.
 */

interface GleanImpl {

  // toolkit/mozapps/extensions/metrics.yaml

  addonsManager: {
    install: GleanEvent;
    update: GleanEvent;
    installStats: GleanEvent;
    manage: GleanEvent;
    report: GleanEvent;
    reportSuspiciousSite: GleanEvent;
  }

  blocklist: {
    lastModifiedRsAddonsMblf: GleanDatetime;
    mlbfSource: GleanString;
    mlbfGenerationTime: GleanDatetime;
    mlbfStashTimeOldest: GleanDatetime;
    mlbfStashTimeNewest: GleanDatetime;
    addonBlockChange: GleanEvent;
  }

  // toolkit/components/extensions/metrics.yaml

  extensions: {
    useRemotePref: GleanBoolean;
    useRemotePolicy: GleanBoolean;
    startupCacheLoadTime: GleanTimespan;
    startupCacheReadErrors: Record<string, GleanCounter>;
    startupCacheWriteBytelength: GleanQuantity;
    processEvent: Record<string, GleanCounter>;
  }

  extensionsApisDnr: {
    startupCacheReadSize: GleanMemoryDistribution;
    startupCacheReadTime: GleanTimingDistribution;
    startupCacheWriteSize: GleanMemoryDistribution;
    startupCacheWriteTime: GleanTimingDistribution;
    startupCacheEntries: Record<string, GleanCounter>;
    validateRulesTime: GleanTimingDistribution;
    evaluateRulesTime: GleanTimingDistribution;
    evaluateRulesCountMax: GleanQuantity;
  }

  extensionsData: {
    migrateResult: GleanEvent;
    storageLocalError: GleanEvent;
  }

  extensionsQuarantinedDomains: {
    listsize: GleanQuantity;
    listhash: GleanString;
    remotehash: GleanString;
  }

  extensionsCounters: {
    browserActionPreloadResult: Record<string, GleanCounter>;
    eventPageIdleResult: Record<string, GleanCounter>;
  }

  extensionsTiming: {
    backgroundPageLoad: GleanTimingDistribution;
    browserActionPopupOpen: GleanTimingDistribution;
    contentScriptInjection: GleanTimingDistribution;
    eventPageRunningTime: GleanCustomDistribution;
    extensionStartup: GleanTimingDistribution;
    pageActionPopupOpen: GleanTimingDistribution;
    storageLocalGetJson: GleanTimingDistribution;
    storageLocalSetJson: GleanTimingDistribution;
    storageLocalGetIdb: GleanTimingDistribution;
    storageLocalSetIdb: GleanTimingDistribution;
  }

}
