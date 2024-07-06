cmake_minimum_required(VERSION 3.5)

SET(COMPANY "lms")
SET(SUFX    "mtp")

# Установка режима сборки по умолчанию
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)." FORCE)
endif()

project(${SUFX}-tiny-database-client LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-O3,--copy-dt-needed-entries")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    set(PROJECT_BINARY_DIR "./bin")
    set(EXECUTABLE_OUTPUT_PATH "./bin")
else()
    set(PROJECT_BINARY_DIR "/opt/${COMPANY}/${SUFX}-tiny-database-client/bin")
    set(EXECUTABLE_OUTPUT_PATH "/opt/${COMPANY}/${SUFX}-tiny-database-client/bin")
endif()

#set(PROJECT_BINARY_DIR "/opt/${COMPANY}/${SUFX}-tiny-database-client/bin")
#set(EXECUTABLE_OUTPUT_PATH "/opt/${COMPANY}/${SUFX}-tiny-database-client/bin")

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )

set(SOCI_PDIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/soci)
set(LOGGER_PDIR ${CMAKE_CURRENT_SOURCE_DIR}/toolkit-cpp/loggerpp)
set(CONFMGR_PDIR ${CMAKE_CURRENT_SOURCE_DIR}/toolkit-cpp/configmgr)
set(WATCHDOG_PDIR ${CMAKE_CURRENT_SOURCE_DIR}/toolkit-cpp/watchdog_worker)

set(MSGTRANSPORT_PDIR ${CMAKE_CURRENT_SOURCE_DIR}/toolkit-cpp/message_transport)

option(POSTGRESQL "POSTGRESQL Client!" OFF)
option(SQLITE3 "SQLITE3 Client!" OFF)

IF (POSTGRESQL)
message( STATUS "ADD DEFINE use_POSTGRESQL")
add_compile_definitions(use_POSTGRESQL)
ELSEIF(SQLITE3)
message( STATUS "ADD DEFINE use_SQLITE3")
add_compile_definitions(use_SQLITE3)
ENDIF()

find_package(PostgreSQL REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories( ${SOCI_PDIR}/include
                     ${SQLite3_INCLUDE_DIRS}
                     ${PostgreSQL_INCLUDE_DIRS}
                     ${CMAKE_CURRENT_SOURCE_DIR}/toolkit-cpp
                     ${MSGTRANSPORT_PDIR}/include
                 )

add_subdirectory(${WATCHDOG_PDIR}/systemd_client)
add_subdirectory(${WATCHDOG_PDIR}/modmon_client)

set(TARGET_SRC
    DBMainManager.cpp
    RequestParser.cpp

    DataProcessingBasic.cpp
    DataProcessingAnalytic.cpp
    DataProcessingEvent.cpp
    DataProcessingVMS.cpp

    EventBase.cpp
    EventsModelCCTV.cpp

    SOCIDBClient.cpp
    PostgreSQLClient.cpp
    SQLite3Client.cpp

    ./toolkit-cpp/Utils/maths.cpp
    ./toolkit-cpp/Utils/files.cpp
    ./toolkit-cpp/Utils/mstrings.cpp
    ./toolkit-cpp/Utils/elapsed_timer.cpp
    ./toolkit-cpp/Utils/FileMonitor.cpp
    ./toolkit-cpp/Utils/uuid.cpp
    ./toolkit-cpp/DirManager/dirman.cpp
    ./toolkit-cpp/DirManager/dirman_posix.cpp

    ${WATCHDOG_PDIR}/WatchDogWorker.cpp

    ${MSGTRANSPORT_PDIR}/src/BaseTransportAny.cpp
    ${MSGTRANSPORT_PDIR}/src/BaseTransportServer.cpp
    )

SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
#SET(CMAKE_INSTALL_RPATH "/opt/lms/mtp-libs/message_transport_upd"
#                        "/opt/lms/mtp-libs/loggerpp_upd"
#                        "/opt/lms/mtp-libs/configmgr_upd"
#                        "/opt/lms/mtp-libs/soci"
#                    )

SET(CMAKE_INSTALL_RPATH "/opt/lms/mtp-libs/message_transport"
                        "/opt/lms/mtp-libs/loggerpp"
                        "/opt/lms/mtp-libs/configmgr"
                        "/opt/lms/mtp-libs/soci"
                    )


add_executable(${SUFX}-tiny-database-client ${TARGET_SRC} main.cpp)

#install(TARGETS ${SUFX}-tiny-database-client
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

target_link_libraries(${SUFX}-tiny-database-client

    PRIVATE
      systemd_client
      modmon_client

    ${SOCI_PDIR}/lib/${ARCHITECTURE}/libsoci_core.so
    ${SOCI_PDIR}/lib/${ARCHITECTURE}/libsoci_postgresql.so
    ${SOCI_PDIR}/lib/${ARCHITECTURE}/libsoci_sqlite3.so

    ${LOGGER_PDIR}/lib/${ARCHITECTURE}/libloggerpp.so
    ${CONFMGR_PDIR}/lib/${ARCHITECTURE}/libconfigmgr.so

    dl rt pthread
)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    message( STATUS "Перенос исполняемого файла в папку установки")
    install(TARGETS ${SUFX}-tiny-database-client DESTINATION "/opt/${COMPANY}/${SUFX}-tiny-database-client/bin")

    message( STATUS "Перенос папок configurations и constraint в папку установки")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/configurations DESTINATION "/opt/${COMPANY}/${SUFX}-tiny-database-client")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/constraints DESTINATION "/opt/${COMPANY}/${SUFX}-tiny-database-client")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/templates DESTINATION "/opt/${COMPANY}/${SUFX}-tiny-database-client")
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/preinstall DESTINATION "/opt/${COMPANY}/${SUFX}-tiny-database-client")
endif()
