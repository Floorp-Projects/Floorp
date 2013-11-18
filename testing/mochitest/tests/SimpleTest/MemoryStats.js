var MemoryStats = {};

/**
 * Statistics that we want to retrieve and display after every test is
 * done.  The keys of this table are intended to be identical to the
 * relevant attributes of nsIMemoryReporterManager.  However, since
 * nsIMemoryReporterManager doesn't necessarily support all these
 * statistics in all build configurations, we also use this table to
 * tell us whether statistics are supported or not.
 */
var MEM_STAT_UNKNOWN = 0;
var MEM_STAT_UNSUPPORTED = 1;
var MEM_STAT_SUPPORTED = 2;

MemoryStats._hasMemoryStatistics = {}
MemoryStats._hasMemoryStatistics.vsize = MEM_STAT_UNKNOWN;
MemoryStats._hasMemoryStatistics.vsizeMaxContiguous = MEM_STAT_UNKNOWN;
MemoryStats._hasMemoryStatistics.residentFast = MEM_STAT_UNKNOWN;
MemoryStats._hasMemoryStatistics.heapAllocated = MEM_STAT_UNKNOWN;

MemoryStats._getService = function (className, interfaceName) {
    var service;
    try {
        service = Cc[className].getService(Ci[interfaceName]);
    } catch (e) {
        service = SpecialPowers.Cc[className]
                               .getService(SpecialPowers.Ci[interfaceName]);
    }
    return service;
}

MemoryStats.dump = function (dumpFn) {
    var mrm = MemoryStats._getService("@mozilla.org/memory-reporter-manager;1",
                                      "nsIMemoryReporterManager");
    for (var stat in MemoryStats._hasMemoryStatistics) {
        var supported = MemoryStats._hasMemoryStatistics[stat];
        var firstAccess = false;
        if (supported == MEM_STAT_UNKNOWN) {
            firstAccess = true;
            try {
                var value = mrm[stat];
                supported = MEM_STAT_SUPPORTED;
            } catch (e) {
                supported = MEM_STAT_UNSUPPORTED;
            }
            MemoryStats._hasMemoryStatistics[stat] = supported;
        }
        if (supported == MEM_STAT_SUPPORTED) {
            dumpFn("TEST-INFO | MEMORY STAT " + stat + " after test: " + mrm[stat]);
        } else if (firstAccess) {
            dumpFn("TEST-INFO | MEMORY STAT " + stat + " not supported in this build configuration.");
        }
    }
};
