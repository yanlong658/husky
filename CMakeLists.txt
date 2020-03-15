cmake_minimum_required(VERSION 2.8.3)
project(ncrl_ugv_ctrl)

set(CMAKE_BUILD_TYPE "Release")

set(CMAKE_CXX_FLAGS "-std=c++11")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -g")

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  roscpp
  geometry_msgs
  std_msgs
  sensor_msgs
  nav_msgs
  tf
  visualization_msgs
)
find_package(PythonLibs)
find_package(Eigen3 REQUIRED)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)


catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES ncrl_ugv_ctrl
#  CATKIN_DEPENDS other_catkin_pkg
#  DEPENDS system_lib
DEPENDS EIGEN3
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
 include
 ${catkin_INCLUDE_DIRS}
 ${EIGEN3_INCLUDE_DIRS}
 ${DJIOSDK_INCLUDE_DIRS}
 ${PYTHON_INCLUDE_DIR}
)

add_library(qp include/ncrl_ugv_ctrl/qptrajectory.cpp include/ncrl_ugv_ctrl/qptrajectory.h)

target_link_libraries(
    qp  ${PYTHON_LIBRARIES}
)


add_executable(husky_path src/husky_path.cpp)
target_link_libraries(husky_path ${catkin_LIBRARIES} qp)
