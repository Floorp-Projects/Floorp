package mozilla.components.browser.engine.system

/**
 * Used for when we need to schedule loading a url or data with a mime type, if there isn't
 * a session already created.
 */
data class ScheduledLoad constructor(
    val url: String?,
    val data: String? = null,
    val mimeType: String? = null
) {
    constructor(data: String, mimeType: String) : this(null, data, mimeType)
    constructor() : this(null, null, null)
}
