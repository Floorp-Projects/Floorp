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
import org.mozilla.fenix.nimbus.MessageSurfaceId
import org.mozilla.fenix.nimbus.Messaging
import org.mozilla.fenix.nimbus.StyleData

/**
 * This ID must match the name given in the `nimbus.fml.yaml` file, which
 * itself generates the classname for [org.mozilla.fenix.nimbus.Messaging].
 *
 * If that ever changes, it should also change here.
 *
 * This constant is the id for the messaging feature (the Nimbus feature). We declare it here
 * so as to afford the best chance of it being changed if a rename operation is needed.
 *
 * It is used in the Studies view, to filter out any experiments which only use a messaging surface.
 */
const val MESSAGING_FEATURE_ID = "messaging"

/**
 * Provides messages from [messagingFeature] and combine with the metadata store on [metadataStorage].
 */
class NimbusMessagingStorage(
    private val context: Context,
    private val metadataStorage: MessageMetadataStorage,
    private val reportMalformedMessage: (String) -> Unit,
    private val gleanPlumb: GleanPlumbInterface,
    private val messagingFeature: FeatureHolder<Messaging>,
    private val attributeProvider: CustomAttributeProvider? = null,
) {
    /**
     * Contains all malformed messages where they key can be the value or a trigger of the message
     * and the value is the message id.
     */
    @VisibleForTesting
    internal val malFormedMap = mutableMapOf<String, String>()
    private val logger = Logger("MessagingStorage")
    private val nimbusFeature = messagingFeature
    private val customAttributes: JSONObject
        get() = attributeProvider?.getCustomAttributes(context) ?: JSONObject()

    /**
     * Returns a list of available messages descending sorted by their priority.
     */
    suspend fun getMessages(): List<Message> {
        val featureValue = messagingFeature.value()
        val nimbusTriggers = featureValue.triggers
        val nimbusStyles = featureValue.styles
        val nimbusActions = featureValue.actions

        val nimbusMessages = featureValue.messages
        val defaultStyle = StyleData()
        val storageMetadata = metadataStorage.getMetadata()

        return nimbusMessages
            .mapNotNull { (key, value) ->
                val action = sanitizeAction(key, value.action, nimbusActions, value.isControl) ?: return@mapNotNull null
                Message(
                    id = key,
                    data = value,
                    action = action,
                    style = nimbusStyles[value.style] ?: defaultStyle,
                    metadata = storageMetadata[key] ?: addMetadata(key),
                    triggers = sanitizeTriggers(key, value.trigger, nimbusTriggers)
                        ?: return@mapNotNull null,
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
    fun getNextMessage(surface: MessageSurfaceId, availableMessages: List<Message>): Message? {
        val jexlCache = HashMap<String, Boolean>()
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        val message = availableMessages.firstOrNull {
            surface == it.surface && isMessageEligible(it, helper, jexlCache)
        } ?: return null

        // Check this isn't an experimental message. If not, we can go ahead and return it.
        if (!isMessageUnderExperiment(message, nimbusFeature.value().messageUnderExperiment)) {
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
     * Returns a pair of uuid and valid action for the provided [action].
     *
     * Uses Nimbus' targeting attributes to do basic string interpolation.
     *
     * e.g.
     * `https://example.com/{locale}/whatsnew.html?version={app_version}`
     *
     * If the string `{uuid}` is detected in the [action] string, then it is
     * replaced with a random UUID. This is returned as the first value of the returned
     * [Pair].
     *
     * The fully resolved (with all substitutions) action is returned as the second value
     * of the [Pair].
     */
    fun generateUuidAndFormatAction(action: String): Pair<String?, String> {
        val helper = gleanPlumb.createMessageHelper(customAttributes)
        val uuid = helper.getUuid(action)

        return Pair(uuid, helper.stringFormat(action, uuid))
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
        nimbusActions: Map<String, String>,
        isControl: Boolean,
    ): String? {
        return when {
            unsafeAction.startsWith("http") -> {
                unsafeAction
            }
            isControl -> "CONTROL_ACTION"
            else -> {
                val safeAction = nimbusActions[unsafeAction]
                if (safeAction.isNullOrBlank() || safeAction.isEmpty()) {
                    if (!malFormedMap.containsKey(unsafeAction)) {
                        reportMalformedMessage(messageId)
                    }
                    malFormedMap[unsafeAction] = messageId
                    return null
                }
                safeAction
            }
        }
    }

    @VisibleForTesting
    internal fun sanitizeTriggers(
        messageId: String,
        unsafeTriggers: List<String>,
        nimbusTriggers: Map<String, String>,
    ): List<String>? {
        return unsafeTriggers.map {
            val safeTrigger = nimbusTriggers[it]
            if (safeTrigger.isNullOrBlank() || safeTrigger.isEmpty()) {
                if (!malFormedMap.containsKey(it)) {
                    reportMalformedMessage(messageId)
                }
                malFormedMap[it] = messageId
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
        jexlCache: MutableMap<String, Boolean> = mutableMapOf(),
    ): Boolean {
        return message.triggers.all { condition ->
            jexlCache[condition]
                ?: try {
                    if (malFormedMap.containsKey(condition)) {
                        return false
                    }
                    helper.evalJexl(condition).also { result ->
                        jexlCache[condition] = result
                    }
                } catch (e: NimbusException.EvaluationException) {
                    reportMalformedMessage(message.id)
                    malFormedMap[condition] = message.id
                    logger.info("Unable to evaluate $condition")
                    false
                }
        }
    }

    @VisibleForTesting
    internal fun getOnControlBehavior(): ControlMessageBehavior = nimbusFeature.value().onControl

    private suspend fun addMetadata(id: String): Message.Metadata {
        return metadataStorage.addMetadata(
            Message.Metadata(
                id = id,
            ),
        )
    }
}
