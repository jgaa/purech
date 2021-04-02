cmake_minimum_required(VERSION 3.0)
project (purech VERSION 0.0.1 LANGUAGES CXX)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.65 REQUIRED COMPONENTS
    system
    program_options
    date_time
    context
    coroutine
    chrono
    log
    )

include(cmake/external-projects.cmake)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    set(RESTC_CPP_LIB restc-cppD)
else()
    set(RESTC_CPP_LIB restc-cpp)
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    pulsar.cpp
    pulsar.h
    pulsar_api.h
    )
add_dependencies(${PROJECT_NAME} externalRestcCpp externalLogfault)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_link_libraries(${PROJECT_NAME}
    ${RESTC_CPP_LIB}
    ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    stdc++fs
    )

add_definitions(
    -DBOOST_COROUTINE_NO_DEPRECATION_WARNING=1
    -DBOOST_ALL_DYN_LINK=1
    )
