cmake_minimum_required(VERSION 3.5)
project(contour_finder_cpp)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(dynamixel_sdk_custom_interfaces REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
add_executable(contour_finder src/contour_finder.cpp)

ament_target_dependencies(contour_finder rclcpp sensor_msgs std_msgs OpenCV cv_bridge dynamixel_sdk_custom_interfaces)

install(TARGETS
  contour_finder
  DESTINATION lib/${PROJECT_NAME})
ament_package()
