# Test executables for clap-saw-demo-ftxui

# Test plugin loader
add_executable(test_plugin test_plugin.cpp)
target_link_libraries(test_plugin clap-core)
if(APPLE)
    target_link_libraries(test_plugin ${CMAKE_DL_LIBS})
endif()

# Test GUI functionality
add_executable(test_gui test_gui.cpp)
target_link_libraries(test_gui clap-core)
if(APPLE)
    target_link_libraries(test_gui ${CMAKE_DL_LIBS})
endif()

# Test parameter functionality
add_executable(test_parameters test_parameters.cpp)
target_link_libraries(test_parameters clap-core)
if(APPLE)
    target_link_libraries(test_parameters ${CMAKE_DL_LIBS})
endif()
