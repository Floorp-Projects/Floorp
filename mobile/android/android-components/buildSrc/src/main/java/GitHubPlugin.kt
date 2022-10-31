/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import groovy.json.JsonSlurper
import org.gradle.api.Plugin
import org.gradle.api.Project
import java.util.Properties

open class GitHubPlugin : Plugin<Project> {

    private lateinit var client: GitHubClient

    companion object {
        private const val REPO_OWNER = "mozilla-mobile"
        private const val REPO_NAME = "android-components"
        private const val BASE_BRANCH_NAME = "main"
        private const val HEAD = "MickeyMoz"
        private const val TOKEN_FILE_PATH = ".github_token"
    }

    override fun apply(project: Project) {
        project.tasks.register("openPR") {
            doLast {
                init(project)
                val title = project.property("title").toString()
                val body = project.property("body", "")
                val branch = project.property("branch").toString()
                val baseBranch = project.property("baseBranch", BASE_BRANCH_NAME)
                val owner = project.property("owner", REPO_OWNER)
                val repo = project.property("repo", REPO_NAME)
                val user = project.property("botUser", HEAD)

                createPullRequest(title, body, branch, baseBranch, owner, repo, user)
            }
        }

        project.tasks.register("openIssue") {
            doLast {
                init(project)
                val title = project.property("title").toString()
                val body = project.property("body", "")
                val owner = project.property("owner", REPO_OWNER)
                val repo = project.property("repo", REPO_NAME)
                createIssue(title, body, owner, repo)
            }
        }
    }

    @Suppress("TooGenericExceptionThrown", "LongParameterList")
    private fun createPullRequest(
        title: String,
        body: String,
        branchName: String,
        baseBranch: String,
        owner: String,
        repoName: String,
        user: String,
    ) {
        val bodyJson = (
            "{\n" +
                "  \"title\": \" $title\",\n" +
                "  \"body\": \"$body\",\n" +
                "  \"head\": \"$user:$branchName\",\n" +
                "  \"base\": \"$baseBranch\"\n" +
                "}"
            )

        val result = client.createPullRequest(owner, repoName, bodyJson)

        val successFul = result.first
        val responseData = result.second

        val stringToPrint = if (successFul) {
            val pullRequestUrl = getUrlFromJSONString(responseData)
            "Pull Request created take a look here $pullRequestUrl"
        } else {
            throw Exception("Unable to create pull request \n $responseData")
        }
        println(stringToPrint)
    }

    @Suppress("TooGenericExceptionThrown")
    private fun createIssue(title: String, body: String, owner: String, repoName: String) {
        val bodyJson = (
            "{\n" +
                "  \"title\": \"$title\",\n" +
                "  \"body\": \"$body\"" +
                "}"
            )

        val result = client.createIssue(owner, repoName, bodyJson)
        val successFul = result.first
        val responseData = result.second

        val stringToPrint = if (successFul) {
            val issueUrl = getUrlFromJSONString(responseData)
            "Issue Create take a look here $issueUrl"
        } else {
            throw Exception("Unable to create issue \n $responseData")
        }

        println(stringToPrint)
    }

    private fun getUrlFromJSONString(json: String): String {
        val jsonResponse = JsonSlurper().parseText(json)
        return (jsonResponse as Map<*, *>)["html_url"].toString()
    }

    private fun init(project: Project) {
        val properties = Properties()
        val tokenFile = project.property("tokenFile", TOKEN_FILE_PATH)
        properties.load(project.rootProject.file(tokenFile).inputStream())
        val token = properties.getProperty("token")
        client = GitHubClient(token)
    }
}
