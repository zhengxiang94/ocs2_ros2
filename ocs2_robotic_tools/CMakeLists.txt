cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
project(ocs2_robotic_tools)




find_package(ament_cmake REQUIRED)
find_package(ocs2_oc REQUIRED)
find_package(ocs2_core REQUIRED)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

find_package(Boost REQUIRED COMPONENTS
  system
  filesystem
  log_setup
  log
)

set(dependencies
  ocs2_core
  ocs2_oc
)

###########
## Build ##
###########
include_directories(
  include
  ${EIGEN3_INCLUDE_DIRS}
)

add_library(${PROJECT_NAME}
  src/common/RotationTransforms.cpp
  src/common/LoopshapingRobotInterface.cpp
)
ament_target_dependencies(${PROJECT_NAME}
  ${dependencies}
)
target_compile_options(${PROJECT_NAME} PUBLIC ${OCS2_CXX_FLAGS})


add_executable(${PROJECT_NAME}_lintTarget
  src/lintTarget.cpp
)
ament_target_dependencies(${PROJECT_NAME}_lintTarget
  ${dependencies}
)
target_link_libraries(${PROJECT_NAME}_lintTarget
  ${PROJECT_NAME}
)

#########################
###   CLANG TOOLING   ###
#########################
find_package(cmake_clang_tools QUIET)
if(cmake_clang_tools_FOUND)
  message(STATUS "Run clang tooling for target " ${PROJECT_NAME}_lintTarget)
  add_clang_tooling(
    TARGETS ${PROJECT_NAME}_lintTarget
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include
    CT_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    CF_WERROR
  )
endif(cmake_clang_tools_FOUND)

#############
## Install ##
#############

install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include/${PROJECT_NAME}
)

install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})

#############
## Testing ##
#############
find_package(ament_lint_auto REQUIRED)
ament_lint_auto_find_test_dependencies()
find_package(ament_cmake_gtest REQUIRED)

ament_add_gtest(rotation_transform_tests
  test/common/TestRotationTransforms.cpp
  test/common/TestRotationDerivativesTransforms.cpp
)
target_link_libraries(rotation_transform_tests
  ${PROJECT_NAME}
)
ament_target_dependencies(rotation_transform_tests
  ${dependencies}
)
target_compile_options(rotation_transform_tests PRIVATE ${OCS2_CXX_FLAGS})
  
            
ament_export_dependencies(${dependencies})  
ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

ament_package()