rm -rf build
#Создаем директорию build:
mkdir build && cd build
#Выполняем команду cmake для формирования Make-файла
#С поддержкой PostgreSQL:
cmake ../ -DPOSTGRESQL=ON
#Выполняем команду make для сборки модуля:
make -j 5