---
title: TelemetryHolder.<init> - 
---

[org.mozilla.telemetry](../index.html) / [TelemetryHolder](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`TelemetryHolder()`

Holder of a static reference to the Telemetry instance. This is required for background services that somehow need to get access to the configuration and storage. This is not particular nice. Hopefully we can replace this with something better.

