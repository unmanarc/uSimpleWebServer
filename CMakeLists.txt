cmake_minimum_required(VERSION 3.12)
include(GNUInstallDirs)

set(APP_NAME "uSimpleWebServer")
set(SVERSION "1.0.0")
project(${APP_NAME})
project(${PROJECT_NAME} VERSION ${SVERSION} DESCRIPTION "Unmanarcs Simple Web Server")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

file(GLOB_RECURSE EDV_INCLUDE_FILES "src/*.h*")
file(GLOB_RECURSE EDV_SOURCE_FILES "src/*.c*")

add_executable( ${APP_NAME} ${EDV_INCLUDE_FILES} ${EDV_SOURCE_FILES} ${EDV_INCLUDE_FILES2} ${EDV_SOURCE_FILES2} )

install( TARGETS ${APP_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

if (EXTRAPREFIX)
    target_include_directories(${APP_NAME} PUBLIC ${EXTRAPREFIX}/include)
    target_link_libraries(${APP_NAME} "-L${EXTRAPREFIX}/lib")
    target_link_libraries(${APP_NAME} "-L${EXTRAPREFIX}/lib64")
endif()

if (PORTABLE)

    target_link_libraries(${APP_NAME} "cx2_prg_service.a")
    target_link_libraries(${APP_NAME} "cx2_net_sockets.a")
    target_link_libraries(${APP_NAME} "cx2_netp_http.a")
    target_link_libraries(${APP_NAME} "cx2_netp_mime.a")
    target_link_libraries(${APP_NAME} "cx2_mem_vars.a")
    target_link_libraries(${APP_NAME} "cx2_prg_logs.a")
    target_link_libraries(${APP_NAME} "cx2_thr_mutex.a")
    target_link_libraries(${APP_NAME} "cx2_thr_threads.a")
    target_link_libraries(${APP_NAME} "cx2_hlp_functions.a")
    target_link_libraries(${APP_NAME} "cx2_thr_safecontainers.a")
    if (CENTOS7SUPPORT)
        target_link_libraries(${APP_NAME} "boost_system.a")
        target_link_libraries(${APP_NAME} "boost_filesystem.a")
        target_link_libraries(${APP_NAME} "boost_regex.a")
    else()
        target_link_libraries(${APP_NAME} "boost_filesystem.a")
        target_link_libraries(${APP_NAME} "boost_regex.a")
    endif()

    if (WITH_SSL_SUPPORT)
        add_definitions(-DWITH_SSL_SUPPORT)
        target_link_libraries(${APP_NAME} "-lssl")
        target_link_libraries(${APP_NAME} "-lcrypto")
        target_link_libraries(${APP_NAME} "-lz")
    endif()
else()
    target_link_libraries(${APP_NAME} "-lcx2_netp_http")
    target_link_libraries(${APP_NAME} "-lcx2_prg_service")
    target_link_libraries(${APP_NAME} "-lcx2_net_sockets")
    target_link_libraries(${APP_NAME} "-lcx2_netp_mime")
    target_link_libraries(${APP_NAME} "-lcx2_mem_vars")
    target_link_libraries(${APP_NAME} "-lcx2_prg_logs")
    target_link_libraries(${APP_NAME} "-lcx2_thr_mutex")
    target_link_libraries(${APP_NAME} "-lcx2_thr_threads")
    target_link_libraries(${APP_NAME} "-lcx2_hlp_functions")
    target_link_libraries(${APP_NAME} "-lcx2_thr_safecontainers")
    target_link_libraries(${APP_NAME} "-lssl")
    target_link_libraries(${APP_NAME} "-lcrypto")
    target_link_libraries(${APP_NAME} "-lz")

    if (CENTOS7SUPPORT)
        target_link_libraries(${APP_NAME} "-lboost_system")
        target_link_libraries(${APP_NAME} "-lboost_filesystem")
        target_link_libraries(${APP_NAME} "-lboost_regex")
    else()
        target_link_libraries(${APP_NAME} "-lboost_filesystem")
        target_link_libraries(${APP_NAME} "-lboost_regex")
    endif()
endif()



target_link_libraries(${APP_NAME} "-lpthread")
target_link_libraries(${APP_NAME} "-ldl")



if (WIN32)
    target_link_libraries(${APP_NAME} "-lws2_32")
endif()

######################################