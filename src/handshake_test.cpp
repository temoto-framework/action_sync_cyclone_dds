
#include <iostream>
#include <thread>
#include <chrono>
#include "temoto_action_engine/action_synchronizer_plugin_base.h"
#include "temoto_action_engine/action_plugin.hpp"

using namespace std::chrono;

std::string graph_name;
std::string actor_name;
size_t timeout{1000};
std::set<std::string> other_actors;

void notificationCallback(const Notification& n)
{
    std::cout << "[" << duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count() << "] " << "Got notification from " << n.waitable.actor_name << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main(int argc, char** argv)
{
    graph_name = argv[1];
    actor_name = argv[2];
    timeout = std::stoi(argv[3]);

    unsigned int count{4};
    while (argv[count] != nullptr)
        other_actors.insert(argv[count++]);

    ActionPlugin<ActionSynchronizerPluginBase> plugin_("action_sync_cyclone_dds");

    plugin_.load();
    plugin_.get()->setName(actor_name);
    plugin_.get()->setNotificationReceivedCallback(&notificationCallback);

    if (plugin_.get()->bidirHandshake(graph_name, other_actors, 5000))
    {
        std::cout << actor_name << ": Successful handshake between participants reached for graph " << graph_name << std::endl;
    }
    else
    {
        std::cout << actor_name << ": Reached the timeout of " << timeout << " ms" << std::endl;
        return 1;
    }

    Notification notification{
        .parameters = "params_XYZ",
        .result = "on_true",
        .waitable = Waitable{
            .action_name = actor_name + "::action_X",
            .graph_name  = graph_name,
            .actor_name  = actor_name
    }};

    bool success = plugin_.get()->notify(notification, other_actors, 5000);
    std::cout << "[" << duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count() << "] done with notify" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if(success)
    {
        std::cout << actor_name << ": Successfully sent a notification " << std::endl;
    }
    else
    {
        std::cout << actor_name << ": Unsuccessfully sent a notification " << std::endl;
    }

    return success ? 0 : 1;
}
