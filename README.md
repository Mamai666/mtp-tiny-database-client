# mtp-tiny-database-client

## Необходимые библиотеки для сборки SOCI и клиента БД
Для сборки SOCI из исходников: 
1. перейти в toolkit-cpp/SOCI
2. Установить зависимости:
* sudo apt install libsqlite3-dev
* sudo apt install sqlite3
* sudo apt install libpq-dev postgresql-server-dev-all
3. mkdir build
4. cd build
5. cmake -G "Unix Makefiles" -DWITH_BOOST=OFF -DSOCI_TESTS=OFF -DWITH_MYSQL=OFF -DWITH_POSTGRESQL=ON -DWITH_SQLITE3=ON -DSOCI_STATIC=OFF ../
6. make -j 4
7. build/lib
8. найти нужные so:
* libsoci_*.so


