/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject
import org.mozilla.experiments.nimbus.GleanPlumbInterface
import org.mozilla.experiments.nimbus.GleanPlumbMessageHelper
import org.mozilla.experiments.nimbus.internal.FeatureHolder
import org.mozilla.experiments.nimbus.internal.NimbusException

import org.mozilla.fenix.nimbus.Messaging
import org.mozilla.fenix.nimbus.StyleData

/**
 * Provides messages from [messagingFeature] and combine with the metadata store on [metadataStorage].
 */
class NimbusMessagingStorage(
    private val context: Context,
    private val metadataStorage: MessageMetadataStorage,
    private val gleanPlumb: GleanPlumbInterface,
    private val messagingFeature: FeatureHolder<Messaging>
) {
    private val logger = Logger("MessagingStorage")
    private val nimbusFeature = messagingFeature.value()
    private val customAttributes: JSONObject
        get() = JSONObject()

    /**
     * Returns a list of available messages descending sorted by their priority.
     */
    fun getMessages(): List<Message> {
        val nimbusTriggers = nimbusFeature.triggers
        val nimbusStyles = nimbusFeature.styles
        val nimbusActions = nimbusFeature.actions

        val nimbusMessages = nimbusFeature.messages
        val defaultStyle = StyleData(context)
        val storageMetadata = metadataStorage.getMetadata().associateBy {
            it.id
        }

        return nimbusMessages.mapNotNull { (key, value) ->
            val action = sanitizeAction(value.action, nimbusActions) ?: return@mapNotNull null
            Message(
                id = key,
                data = value,
                action = action,
                style = nimbusStyles[value.style] ?: defaultStyle,
                metadata = storageMetadata[key] ?: addMetadata(key),
                triggers = sanitizeTriggers(value.trigger, nimbusTriggers) ?: return@mapNotNull null
            )
        }.filter {
            it.data.maxDisplayCount >= it.metadata.displayCount &&
                !it.metadata.dismissed &&
                !it.metadata.pressed
        }.sortedByDescending {
            it.style.priority
        }
    }

    /**
     * Returns the next higher priority message which all their triggers are true.
     */
    fun getNextMessage(availableMessages: List<Message>): Message? {
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        var message = availableMessages.firstOrNull {
            isMessageEligible(it, helper)
        } ?: return null

        if (isMessageUnderExperiment(message, nimbusFeature.messageUnderExperiment)) {
            messagingFeature.recordExposure()

            if (message.data.isControl) {
                message = availableMessages.firstOrNull {
                    !it.data.isControl && isMessageEligible(it, helper)
                } ?: return null
            }
        }
        return message
    }

    /**
     * Returns a valid action for the provided [message].
     */
    fun getMessageAction(message: Message): String {
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        val uuid = helper.getUuid(message.action)

        return helper.stringFormat(message.action, uuid)
    }

    /**
     * Updated the provided [metadata] in the storage.
     */
    fun updateMetadata(metadata: Message.Metadata) {
        metadataStorage.updateMetadata(metadata)
    }

    @VisibleForTesting
    internal fun sanitizeAction(
        unsafeAction: String,
        nimbusActions: Map<String, String>
    ): String? {
        return if (unsafeAction.startsWith("http")) {
            unsafeAction
        } else {
            val safeAction = nimbusActions[unsafeAction]
            if (safeAction.isNullOrBlank() || safeAction.isEmpty()) {
                return null
            }
            safeAction
        }
    }

    @VisibleForTesting
    internal fun sanitizeTriggers(
        unsafeTriggers: List<String>,
        nimbusTriggers: Map<String, String>
    ): List<String>? {
        return unsafeTriggers.map {
            val safeTrigger = nimbusTriggers[it]
            if (safeTrigger.isNullOrBlank() || safeTrigger.isEmpty()) {
                return null
            }
            safeTrigger
        }
    }

    @VisibleForTesting
    internal fun isMessageUnderExperiment(message: Message, expression: String?): Boolean {
        return when {
            expression.isNullOrBlank() -> {
                false
            }
            expression.endsWith("-") -> {
                message.id.startsWith(expression)
            }
            else -> {
                message.id == expression
            }
        }
    }

    @VisibleForTesting
    internal fun isMessageEligible(
        message: Message,
        helper: GleanPlumbMessageHelper
    ): Boolean {
        return message.triggers.all { condition ->
            try {
                helper.evalJexl(condition)
            } catch (e: NimbusException.EvaluationException) {
                // Report to glean as malformed message
                // Will be addressed on https://github.com/mozilla-mobile/fenix/issues/24224
                logger.info("Unable to evaluate $condition")
                false
            }
        }
    }

    private fun addMetadata(id: String): Message.Metadata {
        // This will be improve on https://github.com/mozilla-mobile/fenix/issues/24222
        return metadataStorage.addMetadata(
            Message.Metadata(
                id = id,
            )
        )
    }
}
