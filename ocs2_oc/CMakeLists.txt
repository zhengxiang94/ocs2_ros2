cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
project(ocs2_oc)

## Find ament_cmake macros and libraries
find_package(ament_cmake REQUIRED)
find_package(ocs2_core REQUIRED)
find_package(Boost REQUIRED COMPONENTS
  system
  filesystem
  log_setup
  log
)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)




###################################
## ament_cmake specific configuration ##
###################################

###########
## Build ##
###########

include_directories(
  include
  test/include
  ${EIGEN3_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

set(dependencies
    Boost
    Eigen3
    ocs2_core
)

add_library(${PROJECT_NAME}
  src/approximate_model/ChangeOfInputVariables.cpp
  src/approximate_model/LinearQuadraticApproximator.cpp
  src/multiple_shooting/Helpers.cpp
  src/multiple_shooting/Initialization.cpp
  src/multiple_shooting/LagrangianEvaluation.cpp
  src/multiple_shooting/MetricsComputation.cpp
  src/multiple_shooting/PerformanceIndexComputation.cpp
  src/multiple_shooting/ProjectionMultiplierCoefficients.cpp
  src/multiple_shooting/Transcription.cpp
  src/oc_data/LoopshapingPrimalSolution.cpp
  src/oc_data/PerformanceIndex.cpp
  src/oc_data/TimeDiscretization.cpp
  src/oc_problem/OptimalControlProblem.cpp
  src/oc_problem/LoopshapingOptimalControlProblem.cpp
  src/oc_problem/OptimalControlProblemHelperFunction.cpp
  src/oc_problem/OcpSize.cpp
  src/oc_problem/OcpToKkt.cpp
  src/oc_solver/SolverBase.cpp
  src/precondition/Ruzi.cpp
  src/rollout/PerformanceIndicesRollout.cpp
  src/rollout/RolloutBase.cpp
  src/rollout/RootFinder.cpp
  src/rollout/InitializerRollout.cpp
  src/rollout/StateTriggeredRollout.cpp
  src/rollout/TimeTriggeredRollout.cpp
  src/rollout/RolloutSettings.cpp
  src/synchronized_module/ReferenceManager.cpp
  src/synchronized_module/LoopshapingReferenceManager.cpp
  src/synchronized_module/LoopshapingSynchronizedModule.cpp
  src/synchronized_module/SolverObserver.cpp
  src/search_strategy/FilterLinesearch.cpp
  src/trajectory_adjustment/TrajectorySpreading.cpp
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
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/test/include
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

ament_add_gtest(test_${PROJECT_NAME}_multiple_shooting
  test/multiple_shooting/testProjectionMultiplierCoefficients.cpp
  test/multiple_shooting/testTranscriptionMetrics.cpp
  test/multiple_shooting/testTranscriptionPerformanceIndex.cpp
)
ament_target_dependencies(test_${PROJECT_NAME}_multiple_shooting
  ${dependencies}
)
target_link_libraries(test_${PROJECT_NAME}_multiple_shooting
  ${PROJECT_NAME}
)

ament_add_gtest(test_${PROJECT_NAME}_data
  test/oc_data/testTimeDiscretization.cpp
)
ament_target_dependencies(test_${PROJECT_NAME}_data
  ${dependencies}
)
target_link_libraries(test_${PROJECT_NAME}_data
  ${PROJECT_NAME}
)

ament_add_gtest(test_${PROJECT_NAME}_rollout
   test/rollout/testTimeTriggeredRollout.cpp
   test/rollout/testStateTriggeredRollout.cpp
)
target_link_libraries(test_${PROJECT_NAME}_rollout
  ${PROJECT_NAME} ${ocs2_core_TARGETS}
)
ament_target_dependencies(test_${PROJECT_NAME}_rollout
  ${dependencies}
)

ament_add_gtest(test_change_of_variables
  test/testChangeOfInputVariables.cpp
)
ament_target_dependencies(test_change_of_variables
  ${dependencies}
)
target_link_libraries(test_change_of_variables
  ${PROJECT_NAME}
)

ament_add_gtest(test_trajectory_spreading
  test/trajectory_adjustment/TrajectorySpreadingTest.cpp
)
ament_target_dependencies(test_trajectory_spreading
  ${dependencies}
)
target_link_libraries(test_trajectory_spreading
  ${PROJECT_NAME}  
)

ament_add_gtest(test_ocp_to_kkt
  test/oc_problem/testOcpToKkt.cpp
)
target_link_libraries(test_ocp_to_kkt
  ${PROJECT_NAME}
)
ament_target_dependencies(test_ocp_to_kkt
  ${dependencies}
)

ament_add_gtest(test_precondition
  test/precondition/testPrecondition.cpp
)
target_link_libraries(test_precondition
  ${PROJECT_NAME}
)
ament_target_dependencies(test_precondition
  ${dependencies}
)

ament_export_dependencies(${dependencies})  
ament_export_include_directories("include/${PROJECT_NAME}")
ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)

ament_package()