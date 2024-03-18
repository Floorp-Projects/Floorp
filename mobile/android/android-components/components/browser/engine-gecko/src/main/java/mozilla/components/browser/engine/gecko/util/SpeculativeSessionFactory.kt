/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.util

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.Settings
import org.mozilla.geckoview.GeckoRuntime

/**
 * Helper factory for creating and maintaining a speculative [EngineSession].
 */
internal class SpeculativeSessionFactory {
    @VisibleForTesting
    internal var speculativeEngineSession: SpeculativeEngineSession? = null

    /**
     * Creates a speculative [EngineSession] using the provided [contextId] and [defaultSettings].
     * Creates a private session if [private] is set to true.
     *
     * The speculative [EngineSession] is kept internally until explicitly needed and access via [get].
     */
    @Synchronized
    fun create(
        runtime: GeckoRuntime,
        private: Boolean,
        contextId: String?,
        defaultSettings: Settings?,
    ) {
        if (speculativeEngineSession?.matches(private, contextId) == true) {
            // We already have a speculative engine session for this configuration. Nothing to do here.
            return
        }

        // Clear any potentially non-matching engine session
        clear()

        speculativeEngineSession = SpeculativeEngineSession.create(
            this,
            runtime,
            private,
            contextId,
            defaultSettings,
        )
    }

    /**
     * Clears the internal speculative [EngineSession].
     */
    @Synchronized
    fun clear() {
        speculativeEngineSession?.cleanUp()
        speculativeEngineSession = null
    }

    /**
     * Returns and consumes a previously created [private] speculative [EngineSession] if it uses
     * the same [contextId]. Returns `null` if no speculative [EngineSession] for that
     * configuration is available.
     */
    @Synchronized
    fun get(
        private: Boolean,
        contextId: String?,
    ): GeckoEngineSession? {
        val speculativeEngineSession = speculativeEngineSession ?: return null

        return if (speculativeEngineSession.matches(private, contextId)) {
            this.speculativeEngineSession = null
            speculativeEngineSession.unwrap()
        } else {
            clear()
            null
        }
    }

    @VisibleForTesting
    internal fun hasSpeculativeSession(): Boolean {
        return speculativeEngineSession != null
    }
}

/**
 * Internal wrapper for [GeckoEngineSession] that takes care of registering and unregistering an
 * observer for handling content process crashes/kills.
 */
internal class SpeculativeEngineSession constructor(
    @get:VisibleForTesting internal val engineSession: GeckoEngineSession,
    @get:VisibleForTesting internal val observer: SpeculativeSessionObserver,
) {
    /**
     * Checks whether the [SpeculativeEngineSession] matches the given configuration.
     */
    fun matches(private: Boolean, contextId: String?): Boolean {
        return engineSession.geckoSession.settings.usePrivateMode == private &&
            engineSession.geckoSession.settings.contextId == contextId
    }

    /**
     * Unwraps the internal [GeckoEngineSession].
     *
     * After calling [unwrap] the wrapper will no longer observe the [GeckoEngineSession] and further
     * crash handling is left to the application.
     */
    fun unwrap(): GeckoEngineSession {
        engineSession.unregister(observer)
        return engineSession
    }

    /**
     * Cleans up the internal state of this [SpeculativeEngineSession]. After calling this method
     * his [SpeculativeEngineSession] cannot be used anymore.
     */
    fun cleanUp() {
        engineSession.unregister(observer)
        engineSession.close()
    }

    companion object {
        fun create(
            factory: SpeculativeSessionFactory,
            runtime: GeckoRuntime,
            private: Boolean,
            contextId: String?,
            defaultSettings: Settings?,
        ): SpeculativeEngineSession {
            val engineSession = GeckoEngineSession(runtime, private, defaultSettings, contextId)
            val observer = SpeculativeSessionObserver(factory)
            engineSession.register(observer)

            return SpeculativeEngineSession(engineSession, observer)
        }
    }
}

/**
 * [EngineSession.Observer] implementation that will notify the [SpeculativeSessionFactory] if an
 * [GeckoEngineSession] can no longer be used after a crash.
 */
internal class SpeculativeSessionObserver(
    private val factory: SpeculativeSessionFactory,

) : EngineSession.Observer {
    override fun onCrash() {
        factory.clear()
    }

    override fun onProcessKilled() {
        factory.clear()
    }
}
