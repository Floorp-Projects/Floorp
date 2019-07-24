package com.leanplum.internal;

import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;

import java.util.HashSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class CountAggregator {
    private Set<String> enabledCounters = new HashSet<>();
    private final Map<String, Integer> counts = new HashMap<>();

    public void setEnabledCounters(Set<String> enabledCounters) {
        this.enabledCounters = enabledCounters;
    }

    public void incrementCount(@NonNull String name) {
        incrementCount(name, 1);
    }

    public void incrementCount(@NonNull String name, int incrementCount) {
        if (enabledCounters.contains(name)) {
            Integer count = 0;
            if (counts.containsKey(name)) {
                count = counts.get(name);
            }
            count = count + incrementCount;
            counts.put(name, count);
        }
    }

    @VisibleForTesting
    public Map<String, Integer> getAndClearCounts() {
        Map<String, Integer> previousCounts = new HashMap<>();
        previousCounts.putAll(counts);
        counts.clear();
        return previousCounts;
    }

    @VisibleForTesting
    public Map<String, Object> makeParams(@NonNull String name, int count) {
        Map<String, Object> params = new HashMap<>();

        params.put(Constants.Params.TYPE, Constants.Values.SDK_COUNT);
        params.put(Constants.Params.NAME, name);
        params.put(Constants.Params.COUNT, count);

        return params;
    }

    public void sendAllCounts() {
        Map<String, Integer> counts = getAndClearCounts();

        for(Map.Entry<String, Integer> entry : counts.entrySet()) {
            String name = entry.getKey();
            Integer count = entry.getValue();
            Map<String, Object> params = makeParams(name, count);
            try {
                RequestOld.post(Constants.Methods.LOG, params).sendEventually();
            } catch (Throwable t) {
                android.util.Log.e("Leanplum", "Unable to send count.", t);
            }
        }
    }

    public Map<String, Integer> getCounts() {
        return counts;
    }
}
