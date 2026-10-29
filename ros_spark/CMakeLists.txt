cmake_minimum_required(VERSION 3.5)
project(ros_spark)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(VERSION "2022.1.1")

# Set OS and ARCH 
if(WIN32)
  set(OS "windows")
  set(ARCH "x86-64")
elseif(APPLE)
  set(OS "osx")
  set(ARCH "x86-64")
elseif(CMAKE_LIBRARY_ARCHITECTURE)
  set(OS "linux")
  if (${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "arm-linux-gnueabihf")
      set(ARCH "linuxraspbian")
  elseif (${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "aarch64-linux-gnu")
      set(ARCH "aarch64bionic")
  elseif (${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")
      set(ARCH "x86-64")
  else()
      message(FATAL_ERROR "Unsupported architecture: ${CMAKE_LIBRARY_ARCHITECTURE}")
  endif()
else()
  message(FATAL_ERROR "Unsupported OS")
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(ros_spark_msgs REQUIRED)
if(WIN32)
  find_package(fmt REQUIRED)
endif()

# Macro for downloading REV Libraries
macro(add_extern_library LIB_NAME HEADERS_URL_PREFIX HEADERS_ZIP_FILENAME LIB_URL_PREFIX LIB_ZIP_FILENAME LIB_FILENAME)
  SET(LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}/${VERSION}")
  SET(LIB_DEST "${LIB_DIR}/${OS}/${ARCH}/static/${LIB_FILENAME}")
  file(MAKE_DIRECTORY ${LIB_DIR}/include)

  # download the library
  set(URL "${LIB_URL_PREFIX}${LIB_ZIP_FILENAME}")
  set(LIB_ZIP_DEST "${LIB_DIR}/${LIB_ZIP_FILENAME}")
  if(NOT EXISTS ${LIB_ZIP_DEST})
      message(STATUS "Downloading ${LIB_ZIP_FILENAME} from ${URL}")
      file(DOWNLOAD ${URL} ${LIB_ZIP_DEST})
  endif()

  # download the headers
  set(URL "${HEADERS_URL_PREFIX}${HEADERS_ZIP_FILENAME}")
  set(HEADERS_ZIP_DEST "${LIB_DIR}/${HEADERS_ZIP_FILENAME}")
  if(NOT EXISTS ${HEADERS_ZIP_DEST})
      message(STATUS "Downloading headers for ${HEADERS_ZIP_FILENAME} from ${URL}")
      file(DOWNLOAD ${URL} ${HEADERS_ZIP_DEST})
  endif()
 
  # Create a target for extracing the zip files
  add_custom_target(${LIB_NAME}-unzip DEPENDS ${LIB_DEST})
  add_custom_command(OUTPUT ${LIB_DEST} PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E tar xzf ${LIB_ZIP_DEST}
      COMMAND cd ${LIB_DIR}/include && ${CMAKE_COMMAND} -E tar xzf ${HEADERS_ZIP_DEST}
      WORKING_DIRECTORY ${LIB_DIR}
      COMMENT "Unpacking ${LIB_FILENAME}"
      VERBATIM
  )

  # Add library
  add_library(${LIB_NAME} STATIC IMPORTED)
  add_dependencies(${LIB_NAME} ${LIB_NAME}-unzip)
  set_target_properties(${LIB_NAME} PROPERTIES
      IMPORTED_LOCATION ${LIB_DEST}
      INTERFACE_INCLUDE_DIRECTORIES "${LIB_DIR}/include;$<INSTALL_INTERFACE:include>"
  )
  install(DIRECTORY ${LIB_DIR}/include/ DESTINATION include)
  install(FILES ${LIB_DEST} DESTINATION lib)
endmacro()

macro(add_wpi_library LIB_NAME)
    add_extern_library(
        ${LIB_NAME}
        "https://frcmaven.wpi.edu/artifactory/wpilib-mvn-release-local/edu/wpi/first/${LIB_NAME}/${LIB_NAME}-cpp/${VERSION}/"
        "${LIB_NAME}-cpp-${VERSION}-headers.zip"
        "https://frcmaven.wpi.edu/artifactory/wpilib-mvn-release-local/edu/wpi/first/${LIB_NAME}/${LIB_NAME}-cpp/${VERSION}/"
        "${LIB_NAME}-cpp-${VERSION}-${OS}${ARCH}static.zip"
        "${CMAKE_STATIC_LIBRARY_PREFIX}${LIB_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
endmacro()


add_extern_library(
    hal
    "https://frcmaven.wpi.edu/artifactory/wpilib-mvn-release-local/edu/wpi/first/hal/hal-cpp/${VERSION}/"
    "hal-cpp-${VERSION}-headers.zip"
    "https://frcmaven.wpi.edu/artifactory/wpilib-mvn-release-local/edu/wpi/first/hal/hal-cpp/${VERSION}/"
    "hal-cpp-${VERSION}-${OS}${ARCH}static.zip"
    "${CMAKE_STATIC_LIBRARY_PREFIX}wpiHal${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

add_extern_library(
    REVLib_cpp
    "https://github.com/REVrobotics/REV-Software-Binaries/releases/download/revlib-${VERSION}/"
    "REVLib-cpp-${VERSION}-headers.zip"
    "https://github.com/REVrobotics/REV-Software-Binaries/releases/download/revlib-${VERSION}/"
    "REVLib-cpp-${VERSION}-${OS}${ARCH}static.zip"
    "${CMAKE_STATIC_LIBRARY_PREFIX}REVLib${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

add_extern_library(
    REVLib_driver
    "https://github.com/REVrobotics/REV-Software-Binaries/releases/download/revlib-${VERSION}/"
    "REVLib-driver-${VERSION}-headers.zip"
    "https://github.com/REVrobotics/REV-Software-Binaries/releases/download/revlib-${VERSION}/"
    "REVLib-driver-${VERSION}-${OS}${ARCH}static.zip"
    "${CMAKE_STATIC_LIBRARY_PREFIX}REVLibDriver${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

add_wpi_library(wpiutil)
add_wpi_library(wpimath)
add_wpi_library(wpilibc)

if(OS STREQUAL "linux")
  file(GLOB HAL_FILES src/HAL/*.cpp)
  add_library(hal_replacement STATIC ${HAL_FILES})
  set_target_properties(hal_replacement PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_dependencies(hal_replacement hal)

  get_target_property(HAL_INCLUDE_DIRS hal INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(hal_replacement PUBLIC ${HAL_INCLUDE_DIRS})

  ament_target_dependencies(hal_replacement rclcpp)
  target_link_libraries(hal_replacement wpiutil)
else()
  add_library(hal_replacement ALIAS hal)
endif()

add_library(spark_max SHARED src/spark_max.cpp)
set_target_properties(spark_max PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
ament_target_dependencies(spark_max
  rclcpp
  rclcpp_components
  ros_spark_msgs
)
target_include_directories(spark_max PUBLIC include)
target_link_libraries(spark_max REVLib_cpp REVLib_driver wpilibc hal_replacement wpiutil wpimath)

rclcpp_components_register_nodes(spark_max "ros_spark::SparkMax")

add_executable(spark_container src/spark_container.cpp)
target_include_directories(spark_max PUBLIC include)
target_link_libraries(spark_container spark_max)

# fmt debug dynamic library is required for wpiutil for Windows only
if(WIN32)
  get_target_property(FMTD_LOC fmt::fmt IMPORTED_LOCATION_DEBUG)
endif()
install(
  TARGETS spark_max
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(
  TARGETS spark_container
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
