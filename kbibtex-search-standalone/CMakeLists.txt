cmake_minimum_required(VERSION 3.10)
project(KBibTeXSearch VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)

# Find jsoncpp using pkg-config
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Optional: Find libxml2 or pugixml for better XML parsing
find_package(LibXml2)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${JSONCPP_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/bibsearch.cpp
    src/main.cpp
)

# Create executable
add_executable(kbibtex-search ${SOURCES})

# Link libraries
target_link_libraries(kbibtex-search
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

if(LibXml2_FOUND)
    target_include_directories(kbibtex-search PRIVATE ${LIBXML2_INCLUDE_DIR})
    target_link_libraries(kbibtex-search ${LIBXML2_LIBRARIES})
    target_compile_definitions(kbibtex-search PRIVATE HAS_LIBXML2)
endif()

# Set compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(kbibtex-search PRIVATE -Wall -Wextra -O2)
endif()

# Install rules
install(TARGETS kbibtex-search DESTINATION bin)
install(FILES include/bibsearch.h DESTINATION include)

# Create pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/kbibtex-search.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kbibtex-search.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kbibtex-search.pc"
        DESTINATION lib/pkgconfig)

# Create a static library as well (optional)
add_library(bibsearch STATIC src/bibsearch.cpp)
target_link_libraries(bibsearch
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

install(TARGETS bibsearch DESTINATION lib)

# Build documentation (optional)
find_package(Doxygen)
if(DOXYGEN_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()

# CPack configuration for package generation
set(CPACK_PACKAGE_NAME "kbibtex-search")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Literature search tool for bibliographic databases")
set(CPACK_PACKAGE_VENDOR "KBibTeX")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ;ZIP")

include(CPack)