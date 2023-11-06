#include <cstdlib>
#include <iostream>
#include <chrono>
#include <signal.h>
#include <thread>

#include "subscriber.hpp"
#include "ActionSyncData.hpp"

using namespace org::eclipse::cyclonedds;

bool stop{false};

void my_handler(sig_atomic_t s)
{
    printf("Caught signal %d\n",s);
    stop = true;
}

void notification_callback(const ActionSyncData::Notification& msg)
{
    std::cout << "=== [Subscriber] Message received:" << std::endl;
    std::cout << "    actor_name : " << msg.actor_name() << std::endl;
    std::cout << "    parameters : " << msg.parameters() << std::endl;
    std::cout << "    waitable   : " << msg.waitable().action_name() << " in " << msg.waitable().graph_name() << std::endl;
}

int main() 
try 
{
    std::cout << "=== [Subscriber] Create reader." << std::endl;

    signal (SIGINT, my_handler);
    temoto::Subscriber<ActionSyncData::Notification> notification_sub("ActionSyncData_Notification", notification_callback);

    while(!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    return EXIT_SUCCESS;
}
catch (const dds::core::Exception& e)
{
    std::cerr << "=== [Subscriber] DDS exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
} 
catch (const std::exception& e)
{
    std::cerr << "=== [Subscriber] C++ exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}