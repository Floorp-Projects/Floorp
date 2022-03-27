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
import org.mozilla.fenix.nimbus.ControlMessageBehavior
import org.mozilla.fenix.nimbus.Messaging
import org.mozilla.fenix.nimbus.StyleData

/**
 * Provides messages from [messagingFeature] and combine with the metadata store on [metadataStorage].
 */
class NimbusMessagingStorage(
    private val context: Context,
    private val metadataStorage: MessageMetadataStorage,
    private val reportMalformedMessage: (String) -> Unit,
    private val gleanPlumb: GleanPlumbInterface,
    private val messagingFeature: FeatureHolder<Messaging>,
    private val attributeProvider: CustomAttributeProvider? = null
) {
    private val logger = Logger("MessagingStorage")
    private val nimbusFeature = messagingFeature.value()
    private val customAttributes: JSONObject
        get() = attributeProvider?.getCustomAttributes(context) ?: JSONObject()

    /**
     * Returns a list of available messages descending sorted by their priority.
     */
    suspend fun getMessages(): List<Message> {
        val nimbusTriggers = nimbusFeature.triggers
        val nimbusStyles = nimbusFeature.styles
        val nimbusActions = nimbusFeature.actions

        val nimbusMessages = nimbusFeature.messages
        val defaultStyle = StyleData(context)
        val storageMetadata = metadataStorage.getMetadata()

        return nimbusMessages.mapNotNull { (key, value) ->
            val action = sanitizeAction(key, value.action, nimbusActions) ?: return@mapNotNull null
            Message(
                id = key,
                data = value,
                action = action,
                style = nimbusStyles[value.style] ?: defaultStyle,
                metadata = storageMetadata[key] ?: addMetadata(key),
                triggers = sanitizeTriggers(key, value.trigger, nimbusTriggers)
                    ?: return@mapNotNull null
            )
        }.filter {
            it.maxDisplayCount >= it.metadata.displayCount &&
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
        val jexlCache = HashMap<String, Boolean>()
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        val message = availableMessages.firstOrNull {
            isMessageEligible(it, helper, jexlCache)
        } ?: return null

        // Check this isn't an experimental message. If not, we can go ahead and return it.
        if (!isMessageUnderExperiment(message, nimbusFeature.messageUnderExperiment)) {
            return message
        }
        // If the message is under experiment, then we need to record the exposure
        messagingFeature.recordExposure()

        // If this is an experimental message, but not a placebo, then just return the message.
        return if (!message.data.isControl) {
            message
        } else {
            // This is a control, so we need to either return the next message (there may not be one)
            // or not display anything.
            when (getOnControlBehavior()) {
                ControlMessageBehavior.SHOW_NEXT_MESSAGE -> availableMessages.firstOrNull {
                    // There should only be one control message, and we've just detected it.
                    !it.data.isControl && isMessageEligible(it, helper, jexlCache)
                }
                ControlMessageBehavior.SHOW_NONE -> null
            }
        }
    }

    /**
     * Returns a pair of uuid and valid action for the provided [message].
     */
    fun getMessageAction(message: Message): Pair<String?, String> {
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        val uuid = helper.getUuid(message.action)

        return Pair(uuid, helper.stringFormat(message.action, uuid))
    }

    /**
     * Updated the provided [metadata] in the storage.
     */
    suspend fun updateMetadata(metadata: Message.Metadata) {
        metadataStorage.updateMetadata(metadata)
    }

    @VisibleForTesting
    internal fun sanitizeAction(
        messageId: String,
        unsafeAction: String,
        nimbusActions: Map<String, String>
    ): String? {
        return if (unsafeAction.startsWith("http")) {
            unsafeAction
        } else {
            val safeAction = nimbusActions[unsafeAction]
            if (safeAction.isNullOrBlank() || safeAction.isEmpty()) {
                reportMalformedMessage(messageId)
                return null
            }
            safeAction
        }
    }

    @VisibleForTesting
    internal fun sanitizeTriggers(
        messageId: String,
        unsafeTriggers: List<String>,
        nimbusTriggers: Map<String, String>
    ): List<String>? {
        return unsafeTriggers.map {
            val safeTrigger = nimbusTriggers[it]
            if (safeTrigger.isNullOrBlank() || safeTrigger.isEmpty()) {
                reportMalformedMessage(messageId)
                return null
            }
            safeTrigger
        }
    }

    @VisibleForTesting
    internal fun isMessageUnderExperiment(message: Message, expression: String?): Boolean {
        return message.data.isControl || when {
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
        helper: GleanPlumbMessageHelper,
        jexlCache: MutableMap<String, Boolean> = mutableMapOf()
    ): Boolean {
        return message.triggers.all { condition ->
            jexlCache[condition]
                ?: try {
                    helper.evalJexl(condition).also { result ->
                        jexlCache[condition] = result
                    }
                } catch (e: NimbusException.EvaluationException) {
                    reportMalformedMessage(message.id)
                    logger.info("Unable to evaluate $condition")
                    false
                }
        }
    }

    @VisibleForTesting
    internal fun getOnControlBehavior(): ControlMessageBehavior = nimbusFeature.onControl

    private suspend fun addMetadata(id: String): Message.Metadata {
        return metadataStorage.addMetadata(
            Message.Metadata(
                id = id,
            )
        )
    }
}
