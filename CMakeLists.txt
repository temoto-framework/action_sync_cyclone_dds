project(action_sync_cyclone_dds LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

add_compile_options(-g)

find_package(class_loader REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(temoto_action_engine REQUIRED)

idlcxx_generate(TARGET actionsyncdata FILES src/ActionSyncData.idl WARNINGS no-implicit-extensibility)

include_directories(include)

add_library(action_sync_cyclone_dds SHARED src/action_sync_cyclone_dds.cpp)
target_compile_options(action_sync_cyclone_dds PRIVATE -fPIC)
target_link_libraries(action_sync_cyclone_dds 
  actionsyncdata
  CycloneDDS-CXX::ddscxx
  temoto_action_engine
)
class_loader_hide_library_symbols(action_sync_cyclone_dds)

add_executable(handshake_test src/handshake_test.cpp)
target_link_libraries(handshake_test 
  actionsyncdata  
  CycloneDDS-CXX::ddscxx
  pthread 
  temoto_action_engine)
set_property(TARGET handshake_test PROPERTY CXX_STANDARD 17)

install(TARGETS action_sync_cyclone_dds
  EXPORT action_sync_cyclone_ddsTargets
  ARCHIVE DESTINATION bin
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)