set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
# for <kio/global.h>
include_directories(${KDE4_KIO_INCLUDES})

add_definitions(
    -DTEST_DATA="${CMAKE_SOURCE_DIR}/solid/solid/backends/fakehw/fakecomputer.xml"
)

MACRO(KIO_UNIT_TESTS)
    FOREACH(_testname ${ARGN})
        kde4_add_test(kio-${_testname} ${_testname}.cpp)
        target_link_libraries(kio-${_testname} ${QT_QTTEST_LIBRARY} ${QT_QTXML_LIBRARY} kio)
    ENDFOREACH(_testname)
ENDMACRO(KIO_UNIT_TESTS)

# jobtest seems to trigger a ctest problem; jobtest finishes and ctest waits for ever

KIO_UNIT_TESTS(
    krununittest
    kprotocolinfotest
    jobtest
    jobguitest
    kurlcompletiontest
    kfileitemtest
    kbookmarktest
    kacltest
    kdirmodeltest
    kdirlistertest
    fileundomanagertest
    clipboardupdatertest
    globaltest
    udsentrytest
    kfilemetainfotest
    kfileplacesmodeltest
    kurlnavigatortest
    kdiroperatortest
    knewfilemenutest
)
