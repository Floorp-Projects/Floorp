pipeline {
    agent any
    triggers {
        cron(env.BRANCH_NAME == 'main' ? 'H 0 * * *' : '')
    }
    options {
        timestamps()
        timeout(time: 1, unit: 'HOURS')
    }
    stages {
        stage('test') {
        when { branch 'main' }
            steps {
                dir('app/src/androidTest/java/org/mozilla/fenix/syncIntegration') {
                    sh 'pipenv install'
                    sh 'pipenv check'
                    sh 'pipenv run pytest'
                }
            }
        }
    }
    post {
        always {
            script {
                 if (env.BRANCH_NAME == 'main') {
                 publishHTML(target: [
                     allowMissing: false,
                     alwaysLinkToLastBuild: true,
                     keepAll: true,
                     reportDir: 'app/src/androidTest/java/org/mozilla/fenix/syncintegration/results',
                     reportFiles: 'index.html',
                     reportName: 'HTML Report'])
                 }
            }
        }

        failure {
            script {
                if (env.BRANCH_NAME == 'main') {
                    slackSend(
                        color: 'danger',
                        message: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.BUILD_URL}HTML_20Report/)")
                }
            }
        }

        fixed {
            slackSend(
                color: 'good',
                message: "FIXED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.BUILD_URL}HTML_20Report/)")
        }
    }
}
