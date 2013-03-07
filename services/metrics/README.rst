============================
Metrics Collection Framework
============================

This directory contains generic code for collecting and persisting
metrics data for Gecko applications.

Overview
========

Metrics by itself doesn't do much. It simply provides a framework for
collecting data. It is up to users of this framework to hook up the
individual components into a system that makes sense for their purposes.
An example consumer of Metrics is Firefox Health Report (see
/services/healthreport).

Relationship to Telemetry
-------------------------

Telemetry provides similar features to code in this directory. The two
may be unified in the future.

Type Overview
=============

This directory defines a number of JavaScript *types*/*classes*:

Metrics.Provider
  An entity that collects and manages data. A provider is typically
  domain-specific. e.g. AddonsProvider, SearchesProvider.

Metrics.Measurement
  Represents a collection of related pieces/fields of data. Instances of
  these are essentially data structure descriptors.

Metrics.Storage
  Persistent SQLite-backed store for collected metrics data and state.

Metrics.ProviderManager
  High-level entity coordinating activity among several Metrics.Provider
  instances.

SQLite Storage
==============

*Metrics.Storage* provides an interface for persisting metrics data to a
SQLite database.

The storage API organizes values by fields. A field is a named member of
a Measurement that has specific type and retention characteristics. Some
example field types include:

* Last text value
* Last numeric value for a given day
* Discrete text values for a given day

See storage.jsm for more.

While SQLite is used under the hood, this implementation detail is
hidden from the consumer.

Providers and Measurements
==========================

The most important types in this framework are *Metrics.Provider* and
*Metrics.Measurement*, henceforth known as *Provider* and *Measurement*,
respectively. As you will see, these two types go hand in hand.

A Provider is an entity that *provides* data about a specific subsystem
or feature. They do this by recording data to specific Measurement
types. Both Provider and Measurement are abstract base types.

A Measurement implementation defines a name and version. More
importantly, it also defines its storage requirements and how
previously-stored values are serialized.

Storage allocation is performed by communicating with the SQLite
backend. There is a startup function that tells SQLite what fields the
measurement is recording. The storage backend then registers these in
the database. Internally, this is creating a new primary key for
individual fields so later storage operations can directly reference
these primary keys in order to retrieve data without having to perform
complicated joins.

A Provider can be thought of as a collection of Measurement
implementations. e.g. an Addons provider may consist of a measurement
for all *current* add-ons as well as a separate measurement for
historical counts of add-ons. A provider's primary role is to take
metrics data and write it to various measurements. This effectively
persists the data to SQLite.

Data is emitted from providers in either a push or pull based mechanism.
In push-based scenarios, the provider likely subscribes to external
events (e.g. observer notifications). An event of interest can occur at
any time. When it does, the provider immediately writes the event of
interest to storage or buffers it for eventual writing. In pull-based
scenarios, the provider is periodically queried and asked to populate
data.

Usage
=====

To use the code in this directory, import Metrics.jsm. e.g.

    Components.utils.import("resource://gre/modules/Metrics.jsm");

This exports a *Metrics* object which holds references to the main JS
types and functions provided by this feature.

