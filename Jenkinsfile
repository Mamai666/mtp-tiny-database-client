pipeline {
    agent any

    environment {
        REPO_URL = 'http://192.168.1.120:3000/mamai/mtp-tiny-database-client.git'
    }

    stages {
        stage('Clean Workspace') {
            steps {
                script {
                    deleteDir()
                }
            }
        }

        stage('Clone Repositories') {
            steps {
                withCredentials([usernamePassword(credentialsId: 'gitea_home', passwordVariable: 'GITEA_PASSWORD', usernameVariable: 'GITEA_USERNAME')]) {
                    script {
                        sh """
                        echo "Клонирование репозитория с исходным кодом..."
                        git clone http://$GITEA_USERNAME:$GITEA_PASSWORD@$REPO_URL
                        cd mtp-tiny-database-client
                        git submodule init
                        git submodule update
                        """
                    }
                }
            }
        }

        stage('Build Docker Images') {
            parallel {
                stage('Build Docker Image x86') {
                    steps {
                        dir('mtp-tiny-database-client') {
                            script {
                                sh 'echo "Building Docker image for x86"'
                                sh 'docker build -t tiny-bd-client-x86 -f Dockerfile.x86 .'
                            }
                        }
                    }
                }
                stage('Build Docker Image aarch64') {
                    steps {
                        dir('mtp-tiny-database-client') {
                            script {
                                sh 'echo "Building Docker image for aarch64"'
                                sh 'docker build -t tiny-bd-client-aarch64 -f Dockerfile.aarch64 .'
                            }
                        }
                    }
                }
            }
        }

        stage('Run Docker Container for Build PostgreSQL x86') {
            steps {
                script {
                    sh '''
                    echo "Запуск Docker контейнера для сборки проекта с поддержкой PostgreSQL x86"
                    docker run --rm -v /mnt/docker/jenkins/jenkins_container/jenkins/workspace/_mtp-tiny-database-client_master/mtp-tiny-database-client/:/workspace tiny-bd-client-x86 bash -c "
                    echo 'Запуск сборки модуля с поддержкой PostgreSQL x86'
                    ./build-x86_psql.sh
                    echo 'Запуск формирования рабочей директории модуля с поддержкой PostgreSQL x86'
                    ./docker-export_x86_psql.sh
                    "
                    '''
                }
            }
        }

        stage('Run Docker Container for Build SQLite3 x86') {
            steps {
                script {
                    sh '''
                    echo "Запуск Docker контейнера для сборки проекта с поддержкой SQLite3 x86"
                    docker run --rm -v /mnt/docker/jenkins/jenkins_container/jenkins/workspace/_mtp-tiny-database-client_master/mtp-tiny-database-client/:/workspace tiny-bd-client-x86 bash -c "
                    echo 'Запуск сборки модуля с поддержкой SQLite3 x86'
                    ./build-x86_sqlite3.sh
                    echo 'Запуск формирования рабочей директории модуля с поддержкой SQLite3 x86'
                    ./docker-export_x86_sqlite3.sh
                    "
                    '''
                }
            }
        }

        stage('Run Docker Container for Build PostgreSQL aarch64') {
            steps {
                script {
                    sh '''
                    echo "Запуск Docker контейнера для сборки проекта с поддержкой PostgreSQL aarch64"
                    docker run --rm -v /mnt/docker/jenkins/jenkins_container/jenkins/workspace/_mtp-tiny-database-client_master/mtp-tiny-database-client/:/workspace tiny-bd-client-aarch64 bash -c "
                    echo 'Запуск сборки модуля с поддержкой PostgreSQL aarch64'
                    ./build-aarch64_psql.sh
                    echo 'Запуск формирования рабочей директории модуля с поддержкой PostgreSQL aarch64'
                    ./docker-export_aarch64_psql.sh
                    "
                    '''
                }
            }
        }

        stage('Run Docker Container for Build SQLite3 aarch64') {
            steps {
                script {
                    sh '''
                    echo "Запуск Docker контейнера для сборки проекта с поддержкой SQLite3 aarch64" 
                    docker run --rm -v /mnt/docker/jenkins/jenkins_container/jenkins/workspace/_mtp-tiny-database-client_master/mtp-tiny-database-client/:/workspace tiny-bd-client-aarch64 bash -c "
                    echo 'Запуск сборки модуля с поддержкой SQLite3 aarch64'
                    ./build-aarch64_sqlite3.sh
                    echo 'Запуск формирования рабочей директории модуля с поддержкой SQLite3 aarch64'
                    ./docker-export_aarch64_sqlite3.sh
                    "
                    '''
                }
            }
        }
    }


    post {
        always {
            archiveArtifacts artifacts: 'mtp-tiny-database-client/build/**', allowEmptyArchive: true
        }
        success {
            echo 'Build completed successfully!'
        }
        failure {
            echo 'Build failed!'
        }
    }
}