#!/bin/bash

OLDPWD=$PWD

NAME_NEW_DB=main_db

echo "Попытка установки postgres-12.." &&
sudo apt install postgresql-12

if [[ -f /etc/postgresql/12/main/pg_hba_bckp.conf && -f /etc/postgresql/12/main/postgresql_bckp.conf ]]; then
    echo "Сброс Postgresql..."
    sudo mv /etc/postgresql/12/main/pg_hba_bckp.conf /etc/postgresql/12/main/pg_hba.conf
    sudo mv /etc/postgresql/12/main/postgresql_bckp.conf /etc/postgresql/12/main/postgresql.conf
    sudo systemctl restart postgresql
    sleep 5
fi

echo "Обновление настроек Postgresql..."
sudo mv /etc/postgresql/12/main/pg_hba.conf /etc/postgresql/12/main/pg_hba_bckp.conf
sudo mv /etc/postgresql/12/main/postgresql.conf /etc/postgresql/12/main/postgresql_bckp.conf

sudo cp pg_hba.conf /etc/postgresql/12/main/pg_hba.conf
sudo chown postgres:postgres /etc/postgresql/12/main/pg_hba.conf
sudo chmod 644 /etc/postgresql/12/main/pg_hba.conf

sudo cp postgresql.conf /etc/postgresql/12/main/postgresql.conf
sudo chown postgres:postgres /etc/postgresql/12/main/postgresql.conf
sudo chmod 644 /etc/postgresql/12/main/postgresql.conf

if [[ ! -f install.don ]]; then
    echo "Создание Пользователя initial_user на Postgresql..."
    sudo -u postgres -H -- psql -c "CREATE ROLE initial_user NOSUPERUSER CREATEDB CREATEROLE NOINHERIT LOGIN PASSWORD 'terlus';";
    
    sudo -u postgres -H -- createuser admin
    sudo -u postgres -H -- psql -c "alter user admin with password 'terlus';";
    sudo -u postgres -H -- psql -c "GRANT admin TO initial_user;";
    sudo -u postgres -H -- psql -c "ALTER USER initial_user CREATEDB;";
    sudo -u postgres -H -- psql -c "ALTER ROLE initial_user with INHERIT;";

    sudo -u postgres -H -- createdb $NAME_NEW_DB -O admin
   	sudo -u postgres -H -- psql -d $NAME_NEW_DB -f db_dump

    sudo -u postgres -H -- psql -d $NAME_NEW_DB -c "GRANT ALL ON ALL TABLES IN SCHEMA public TO admin;"
    sudo -u postgres -H -- psql -d $NAME_NEW_DB -c "GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO admin;"

    sudo systemctl restart postgresql@12-main.service
    sleep 3

    touch install.don
fi

cd $OLDPWD
