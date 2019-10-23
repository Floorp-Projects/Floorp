pipeline {
    agent any
    triggers {
        cron(env.BRANCH_NAME == 'master' ? 'H 0 * * *' : '')
    }
    options {
        timestamps()
        timeout(time: 1, unit: 'HOURS')
    }
    stages {
        stage('test') {
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
                 publishHTML(target: [
                     allowMissing: false,
                     alwaysLinkToLastBuild: true,
                     keepAll: true,
                     reportDir: '/Users/synctesting/.jenkins/workspace/fenix/app/src/androidTest/java/org/mozilla/fenix/syncintegration/results',
                     reportFiles: 'index.html',
                     reportName: 'HTML Report'])
            }
        }

        failure {
            slackSend(
                color: 'danger',
                message: "FAILED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.BUILD_URL})")
        }

        fixed {
            slackSend(
                color: 'good',
                message: "FIXED: Job '${env.JOB_NAME} [${env.BUILD_NUMBER}]' (${env.BUILD_URL})")
        }
    }
}
