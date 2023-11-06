#include <iostream>

#include "publisher.hpp"
#include "ActionSyncData.hpp"

using namespace org::eclipse::cyclonedds;

int main()
try 
{
    temoto::Publisher<ActionSyncData::Notification> notification_pub("ActionSyncData_Notification");
    notification_pub.waitForSubscribers();

    /* Create a message to write. */
    ActionSyncData::Notification msg;
    msg.actor_name("actor_1");
    msg.parameters("json data");
    msg.waitable(ActionSyncData::Waitable("actor_1", "action_X", "graph_X"));

    /* Write the message. */
    std::cout << "=== [Publisher] Write sample." << std::endl;
    notification_pub.publish(msg);

    return EXIT_SUCCESS;
}
catch (const dds::core::Exception& e) 
{
    std::cerr << "=== [Publisher] Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

