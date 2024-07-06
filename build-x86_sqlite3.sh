#Создаем директорию build:
mkdir build && cd build
#Выполняем команду cmake для формирования Make-файла
#С поддержкой PostgreSQL:
cmake ../ -DSQLITE3=ON
#Выполняем команду make для сборки модуля:
make -j 5