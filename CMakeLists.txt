project(action_sync_cyclone_dds LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

find_package(class_loader REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(temoto_action_engine REQUIRED)

idlcxx_generate(TARGET helloworlddata FILES src/HelloWorldData.idl WARNINGS no-implicit-extensibility)
idlcxx_generate(TARGET actionsyncdata FILES src/ActionSyncData.idl WARNINGS no-implicit-extensibility)

include_directories(include)

add_executable(sync_notification_send src/sync_notification_send.cpp)
add_executable(sync_notification_receive src/sync_notification_receive.cpp)

target_link_libraries(sync_notification_send CycloneDDS-CXX::ddscxx actionsyncdata)
target_link_libraries(sync_notification_receive CycloneDDS-CXX::ddscxx actionsyncdata)

set_property(TARGET sync_notification_send PROPERTY CXX_STANDARD 17)
set_property(TARGET sync_notification_receive PROPERTY CXX_STANDARD 17)

add_library(action_sync_cyclone_dds SHARED src/action_sync_cyclone_dds.cpp)
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