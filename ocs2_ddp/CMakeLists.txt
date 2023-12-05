cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
project(ocs2_ddp)

find_package(ament_cmake REQUIRED) 
find_package(ocs2_core REQUIRED)
find_package(ocs2_mpc REQUIRED)
find_package(ocs2_qp_solver REQUIRED)
find_package(Boost REQUIRED COMPONENTS
  system
  filesystem
  log_setup
  log
)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# Generate compile_commands.json for clang tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

###################################
## ament_cmake specific configuration ##
###################################

set(dependencies
    ocs2_core
    ocs2_oc
    ocs2_mpc
    ocs2_qp_solver
    Boost
)

###########
## Build ##
###########

include_directories(
  include
  test/include
  ${EIGEN3_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

add_library(${PROJECT_NAME}
  src/riccati_equations/ContinuousTimeRiccatiEquations.cpp
  src/riccati_equations/DiscreteTimeRiccatiEquations.cpp
  src/riccati_equations/RiccatiModification.cpp
  src/search_strategy/LevenbergMarquardtStrategy.cpp
  src/search_strategy/LineSearchStrategy.cpp
  src/search_strategy/StrategySettings.cpp
  src/ContinuousTimeLqr.cpp
  src/GaussNewtonDDP.cpp
  src/HessianCorrection.cpp
  src/ILQR.cpp
  src/SLQ.cpp
  src/DDP_Settings.cpp
  src/DDP_HelperFunctions.cpp
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
    TARGETS ${PROJECT_NAME}_lintTarget ${PROJECT_NAME}
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/test
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
install(DIRECTORY test/include/ 
  DESTINATION include/${PROJECT_NAME}
)

#############
## Testing ##
#############
find_package(ament_lint_auto REQUIRED)
ament_lint_auto_find_test_dependencies()
find_package(ament_cmake_gtest REQUIRED)

ament_add_gtest(exp0_ddp_test test/Exp0Test.cpp)
target_link_libraries(exp0_ddp_test ${PROJECT_NAME})
ament_target_dependencies(exp0_ddp_test ${dependencies})

ament_add_gtest(exp1_ddp_test
  test/Exp1Test.cpp
)
target_link_libraries(exp1_ddp_test
  ${PROJECT_NAME}
)
ament_target_dependencies(exp1_ddp_test ${dependencies})

ament_add_gtest(correctness_test
  test/CorrectnessTest.cpp
)
target_link_libraries(correctness_test
  ${PROJECT_NAME}
)
ament_target_dependencies(correctness_test ${dependencies})

ament_add_gtest(riccati_ode_test
  test/RiccatiTest.cpp
)
target_link_libraries(riccati_ode_test
  ${PROJECT_NAME}
)
ament_target_dependencies(riccati_ode_test ${dependencies})

ament_add_gtest(circular_kinematics_ddp_test
  test/CircularKinematicsTest.cpp
)
target_link_libraries(circular_kinematics_ddp_test
  ${PROJECT_NAME}
)
ament_target_dependencies(circular_kinematics_ddp_test ${dependencies})

ament_add_gtest(hybrid_slq_test
  test/HybridSlqTest.cpp
)
target_link_libraries(hybrid_slq_test
  ${PROJECT_NAME}
)
ament_target_dependencies(hybrid_slq_test ${dependencies})

ament_add_gtest(bouncing_mass_test
  test/bouncingmass/BouncingMassTest.cpp
  test/bouncingmass/OverallReference.cpp
  test/bouncingmass/Reference.cpp
)
target_link_libraries(bouncing_mass_test
  ${PROJECT_NAME}
)
ament_target_dependencies(bouncing_mass_test ${dependencies})

ament_add_gtest(testContinuousTimeLqr
  test/testContinuousTimeLqr.cpp
)
target_link_libraries(testContinuousTimeLqr
  ${PROJECT_NAME}
)
ament_target_dependencies(testContinuousTimeLqr ${dependencies})

ament_add_gtest(testDdpHelperFunction
  test/testDdpHelperFunction.cpp
)
target_link_libraries(testDdpHelperFunction
  ${PROJECT_NAME}
)
ament_target_dependencies(testDdpHelperFunction ${dependencies})

ament_add_gtest(testReachingTask
  test/testReachingTask.cpp
)
target_link_libraries(testReachingTask
  ${PROJECT_NAME}
)
ament_target_dependencies(testReachingTask ${dependencies})

ament_export_dependencies(${dependencies})  
ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_package()