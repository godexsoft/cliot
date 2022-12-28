set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost 1.75 COMPONENTS thread system REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(cliot PUBLIC ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
target_include_directories(cliot PUBLIC ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
