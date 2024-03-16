project(kioslave-curl)

include_directories(
    ${KDE4_KIO_INCLUDES}
    ${CURL_INCLUDE_DIRS}
)

########### next target ###############

add_executable(kio_curl kio_curl.cpp)

target_link_libraries(kio_curl
    ${CURL_LIBRARIES}
    kdecore
    kio
)

install(TARGETS kio_curl DESTINATION ${KDE4_LIBEXEC_INSTALL_DIR})

########### install files ###############

install(
    FILES http.protocol https.protocol ftp.protocol sftp.protocol
    DESTINATION ${KDE4_SERVICES_INSTALL_DIR}
)
