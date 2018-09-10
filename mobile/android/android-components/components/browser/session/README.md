# [Android Components](../../../README.md) > Browser > Session

This component provides a generic representation of a browser [Session](#session) and a [SessionManager](#sessionmanager) to link browser sessions to underlying [Engine Sessions](../../concept/engine/README.md), as well as a [SessionStorage](#sessionstorage) to persist and restore sessions.

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:session:{latest-version}
```

### Session

`Session` is a value type representing the state of a browser session. Session instances are usually created by `UseCase` types in feature modules e.g. in [AddNewTabUseCase](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/tabs/src/main/java/mozilla/components/feature/tabs/TabsUseCases.kt).

```Kotlin
val session = Session("https://mozilla.org")
val privateSession = Session("https://mozilla.org", private = true)

// Session instances provide access to numerous observable fields
val url = session.url
val title = session.title
val loadingProgress = session.progress
val securityInfo = session.securityInfo
// ...

// Changes to a session can be observed
session.register(object : Session.Observer {
    onUrlChanged(session: Session, url: String) {
    	// The session is pointing to the provided url now
    }
    onTitleChanged(session: Session, title: String) {
    	// The title of the page this session points to has changed
    }
    onProgress(session: Session, progress: Int)
    	// The page loading progress was updated
    }
    onSecurityChanged(session: Session, securityInfo: SecurityInfo) {
       // The security info of the session was updated
    }
    onTrackerBlocked() {
    	// A tracker was blocked for this session
    }
    // ...
})
```

### SessionManager

`SessionManager` contains the core functionality of this component. It provides functionality to hold all active browser sessions and link them to their corresponding engine sessions. A `SessionManager` can be created by providing an [Engine](../../concept/engine/README.md) instance:

```Kotlin
val sessionManager = SessionManager(engine)

// Sessions can be added and removed
val session = Session("https://mozilla.org")
sessionManager.add(session)
sessionManager.remove(session)
sessionManager.removeAll()

// Sessions can be selected (i.e. for switching tabs)
sessionManager.select(session)

// Sessions can be retrieved in various ways
val selectedSession = sessionManager.selectedSession
val sessions = sessionManager.sessions
val engineSession = sessionManager.getEngineSession(session) 
val selectedEngineSession = sessionManager.getEngineSession()

// Engine sessions can automatically be created if none exists for the provided session
val engineSession = sessionManager.getOrCreateEngineSession(session)
val selectedEngineSession = sessionManager.getOrCreateEngineSession()

// Changes to a SessionManager can be observed
sessionManager.register(object : SessionManager.Observer {
    onSessionSelected(session: Session) {
        // The provided session was selected. 
    }
    onSessionAdded(session: Session) {
    	// The provided session was added.
    }
    onSessionRemoved(session: Session) {
    	// The provided session was removed.
    }
    onAllSessionsRemoved() {
    	// The session manager has been cleared (removeAll has been called).
    }
})
```

### SessionStorage

`SessionStorage` is an interface describing an abstract contract for session storage implementations. Other components reference this abstract type directly to allow for custom (application-provided) implementations. A default implementation is provided as part of this module, which persists browser and engine session states to a JSON file using an [AtomicFile](https://developer.android.com/reference/android/util/AtomicFile). A `DefaultStorage` instance can be created by providing the Android application context:

```Kotlin
val sessionStorage = DefaultSessionStorage(applicationContext)
```

The [feature-session](../../feature/session/README.md) component takes care of starting/stopping the session storage automatically. If the feature component is not being used the following API calls are available:

```Kotlin
sessionStorage.start(sessionManager)
sessionStorage.stop()
```

Once started, session can be persisted and restored by providing a reference to the application's `SessionManager`:

```Kotlin
sessionStorage.persist(sessionManager)
sessonStorage.restore(sessionManager)
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
