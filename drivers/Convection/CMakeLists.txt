project(ConvectionDrivers)

add_executable(ConvectingConeDriver ConvectingConeDriver.cpp)
target_link_libraries(ConvectingConeDriver Camellia)

add_executable(SpaceTimePrototypeConvectingConeDriver SpaceTimePrototypeConvectingConeDriver.cpp)
target_link_libraries(SpaceTimePrototypeConvectingConeDriver Camellia)

