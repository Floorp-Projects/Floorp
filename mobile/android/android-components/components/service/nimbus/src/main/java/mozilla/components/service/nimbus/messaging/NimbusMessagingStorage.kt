/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import kotlinx.coroutines.runBlocking
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject
import org.mozilla.experiments.nimbus.GleanPlumbInterface
import org.mozilla.experiments.nimbus.NimbusMessagingHelperInterface
import org.mozilla.experiments.nimbus.internal.FeatureHolder
import org.mozilla.experiments.nimbus.internal.NimbusException
import mozilla.components.service.nimbus.GleanMetrics.Messaging as GleanMessaging

/**
 * This ID must match the name given in the `messaging.fml.yaml` file, which
 * itself generates the classname for [mozilla.components.service.nimbus.messaging.FxNimbusMessaging].
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
    private val onMalformedMessage: (String) -> Unit = {
        GleanMessaging.malformed.record(GleanMessaging.MalformedExtra(it))
    },
    private val gleanPlumb: GleanPlumbInterface,
    private val messagingFeature: FeatureHolder<Messaging>,
    private val attributeProvider: JexlAttributeProvider? = null,
) {
    /**
     * Contains all malformed messages where they key can be the value or a trigger of the message
     * and the value is the message id.
     */
    @VisibleForTesting
    val malFormedMap = mutableMapOf<String, String>()
    private val logger = Logger("MessagingStorage")
    private val customAttributes: JSONObject
        get() = attributeProvider?.getCustomAttributes(context) ?: JSONObject()

    /**
     * Returns a Nimbus message helper, for evaluating JEXL.
     *
     * The JEXL context is time-sensitive, so this should be created new for each set of evaluations.
     *
     * Since it has a native peer, it should be [destroy]ed after finishing the set of evaluations.
     */
    fun createMessagingHelper(): NimbusMessagingHelperInterface =
        gleanPlumb.createMessageHelper(customAttributes)

    /**
     * Returns the [Message] for the given [key] or returns null if none found.
     */
    suspend fun getMessage(key: String): Message? =
        createMessage(messagingFeature.value(), key)

    @Suppress("ReturnCount")
    private suspend fun createMessage(featureValue: Messaging, key: String): Message? {
        val message = featureValue.messages[key] ?: return null
        if (message.text.isBlank()) {
            reportMalformedMessage(key)
            return null
        }

        val trigger = sanitizeTriggers(key, message.trigger, featureValue.triggers) ?: return null
        val action = sanitizeAction(key, message.action, featureValue.actions, message.isControl) ?: return null
        val defaultStyle = StyleData()
        val storageMetadata = metadataStorage.getMetadata()

        return Message(
            id = key,
            data = message,
            action = action,
            style = featureValue.styles[message.style] ?: defaultStyle,
            metadata = storageMetadata[key] ?: addMetadata(key),
            triggers = trigger,
        )
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun reportMalformedMessage(key: String) {
        messagingFeature.recordMalformedConfiguration(key)
        onMalformedMessage(key)
    }

    /**
     * Returns a list of currently available messages descending sorted by their priority.
     * This list of messages will not include any expired, pressed or dismissed messages.
     */
    suspend fun getMessages(): List<Message> {
        val featureValue = messagingFeature.value()
        val nimbusMessages = featureValue.messages
        return nimbusMessages.keys
            .mapNotNull { key ->
                createMessage(featureValue, key)
            }.filter {
                !it.isExpired &&
                    !it.metadata.dismissed &&
                    !it.metadata.pressed
            }.sortedByDescending {
                it.style.priority
            }
    }

    /**
     * Returns the next higher priority message which all their triggers are true.
     */
    fun getNextMessage(surface: MessageSurfaceId, availableMessages: List<Message>): Message? =
        createMessagingHelper().use {
            getNextMessage(
                surface,
                availableMessages,
                setOf(),
                it,
            )
        }

    @Suppress("ReturnCount")
    private fun getNextMessage(
        surface: MessageSurfaceId,
        availableMessages: List<Message>,
        excluded: Set<String>,
        helper: NimbusMessagingHelperInterface,
    ): Message? {
        val message = availableMessages
            .filter { surface == it.surface }
            .filter { !excluded.contains(it.id) }
            .firstOrNull { isMessageEligible(it, helper) } ?: return null

        val slug = message.data.experiment
        if (slug != null) {
            // We know that it's experimental, and we know which experiment it came from.
            messagingFeature.recordExperimentExposure(slug)
        } else if (message.data.isControl) {
            // It's not experimental, but it is a control. This is obviously malformed.
            reportMalformedMessage(message.id)
        }

        // If this is an experimental message, but not a placebo, then just return the message.
        if (!message.data.isControl) {
            return message
        }

        // If a message is a control then it's considered as displayed
        val updatedMetadata = message.metadata.copy(
            displayCount = message.metadata.displayCount + 1,
            lastTimeShown = System.currentTimeMillis(),
        )

        runBlocking {
            updateMetadata(updatedMetadata)
        }

        // This is a control, so we need to either return the next message (there may not be one)
        // or not display anything.
        return when (getOnControlBehavior()) {
            ControlMessageBehavior.SHOW_NEXT_MESSAGE ->
                getNextMessage(
                    surface,
                    availableMessages,
                    excluded + message.id,
                    helper,
                )

            ControlMessageBehavior.SHOW_NONE -> null
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
    fun generateUuidAndFormatAction(action: String): Pair<String?, String> =
        createMessagingHelper().use { helper ->
            val uuid = helper.getUuid(action)
            val url = helper.stringFormat(action, uuid)
            Pair(uuid, url)
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

    /**
     * Return true if the message passed as a parameter is eligible
     *
     * Aimed to be used from tests only, but currently public because some tests inside Fenix need
     * it. This should be set as internal when this bug is fixed:
     * https://bugzilla.mozilla.org/show_bug.cgi?id=1823472
     */
    @VisibleForTesting
    fun isMessageEligible(
        message: Message,
        helper: NimbusMessagingHelperInterface,
    ): Boolean {
        return message.triggers.all { condition ->
            try {
                if (malFormedMap.containsKey(condition)) {
                    return false
                }
                helper.evalJexl(condition)
            } catch (e: NimbusException.EvaluationException) {
                reportMalformedMessage(message.id)
                malFormedMap[condition] = message.id
                logger.info("Unable to evaluate $condition")
                false
            }
        }
    }

    @VisibleForTesting
    internal fun getOnControlBehavior(): ControlMessageBehavior = messagingFeature.value().onControl

    private suspend fun addMetadata(id: String): Message.Metadata {
        return metadataStorage.addMetadata(
            Message.Metadata(
                id = id,
            ),
        )
    }
}

/**
 * A helper method to safely destroy the message helper after use.
 */
fun <R> NimbusMessagingHelperInterface.use(block: (NimbusMessagingHelperInterface) -> R) =
    block(this).also {
        this.destroy()
    }
