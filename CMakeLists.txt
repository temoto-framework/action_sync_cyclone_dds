project(action_sync_cyclone_dds LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.16)

find_package(class_loader REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(temoto_action_engine REQUIRED)

add_compile_options(-std=c++17)

idlcxx_generate(TARGET helloworlddata FILES src/HelloWorldData.idl WARNINGS no-implicit-extensibility)
idlcxx_generate(TARGET actionsyncdata FILES src/ActionSyncData.idl WARNINGS no-implicit-extensibility)

add_executable(sync_notification_send src/sync_notification_send.cpp)
add_executable(sync_notification_receive src/sync_notification_receive.cpp)

target_link_libraries(sync_notification_send CycloneDDS-CXX::ddscxx actionsyncdata)
target_link_libraries(sync_notification_receive CycloneDDS-CXX::ddscxx actionsyncdata)
