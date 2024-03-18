
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Script for use when compiling a changelog. It will find all GH mentions in commit
 * messages newer than the HEAD of the passed git tag. If no tag is passed, it will
 * default to the highest versioned tag, as defined by gits `--sort=version:refname`.
 *
 * @param (optional) git tag of the earliest commit through which to search. If null,
 * this will default to the tag on origin with the highest version name.
 *
 * To run this script:
 * - Update local main
 * - From project root, run `kotlinc -script automation/releasetools/PrintMentionedIssuesAndPrs.kts`
 *
 * TODO
 * - Use origin/main, instead of local main
 * - Interface with the GitHub API to filter out references to PRs
 * - Pull down issue names for each, to make constructing the changelog easier
 */

val origin = "https://github.com/mozilla-mobile/fenix.git"
val DEBUG = false

println("Starting PrintMentionedIssuesAndPrs.kts")

val tag = try { args[0] } catch (e: IndexOutOfBoundsException) { getHighestVersionedTag() }
debug { "Last tag: $tag" }

val commonCommit = getMostRecentCommonAncestorWithMaster(tag)
debug { "common commit: $commonCommit" }

val log = gitLogSince(commonCommit)
debug { "Log: $log" }

val numbers = getNumbersFromLog(log)
debug { "Numbers: ${numbers.joinToString()}" }

println(numbers)

fun getHighestVersionedTag(): String {
    val originTags = runCommand("git ls-remote --tags --sort=version:refname $origin")

    // Tags are sorted in ascending order, so this returns the name of the newest tag
    return originTags.substringAfterLast("refs/tags/")
        // Trim the trailing line break
        .trim()
}

fun getMostRecentCommonAncestorWithMaster(tag: String): String {
    runCommand("git fetch $origin --tags")
    // TODO use origin main
    return runCommand("git merge-base main $tag").trim()
}

fun gitLogSince(sha: String): String {
    val returnAsString = "--no-pager"
    val maxCount = "-500"
    // There is no plumbing version of 'git log', but pretty formatting provides a
    // mostly consistent return value
    // See: https://stackoverflow.com/a/53584289/9307461
    val formatSubjectOnly = "--pretty=format:%s"

    return runCommand("git $returnAsString log $sha..HEAD $maxCount $formatSubjectOnly")
}

fun getNumbersFromLog(log: String): List<String> {
    val pattern = "#\\d+".toRegex()
    return pattern.findAll(log)
        .map { it.value }
        .distinct()
        .toList()
}

// Function adapted from:
// https://stackoverflow.com/questions/35421699/how-to-invoke-external-command-from-within-kotlin-code
fun runCommand(cmd: String, workingDir: File = File(".")): String {
    try {
        val parts = cmd.split(" ".toRegex())
        debug { "Parts: $parts" }
        val proc = ProcessBuilder(*parts.toTypedArray())
            .directory(workingDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start()
        proc.waitFor(10, TimeUnit.SECONDS)

        debug { "Err: ${proc.errorStream.bufferedReader().use { it.readText() }}" }

        return proc.inputStream.bufferedReader().use { it.readText() }
    } catch (e: IOException) {
        e.printStackTrace()
        throw(e)
    }
}

fun debug(block: () -> String) {
    if (DEBUG) println(block())
}
