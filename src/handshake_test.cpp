#include <class_loader/class_loader.hpp>
#include <iostream>
#include "temoto_action_engine/action_synchronizer_plugin_base.h"

int main(int argc, char** argv)
{
    std::string graph_name(argv[1]);
    std::string actor_name(argv[2]);
    size_t timeout{std::stoi(argv[3])};
    std::vector<std::string> other_actors;

    unsigned int count{4};
    while (argv[count] != nullptr)
        other_actors.push_back(argv[count++]);

    std::string plugin_name{"action_sync_cyclone_dds"};
    class_loader::ClassLoader cl("lib" + plugin_name + ".so", false);

    auto p = cl.createSharedInstance<ActionSynchronizerPluginBase>(plugin_name);
    p->setName(actor_name);

    if (p->waitForConsensus(graph_name, other_actors, 5000))
    {
        std::cout << actor_name << ": Successful handshake between participants reached for graph " << graph_name << std::endl;
        return 0;
    }
    else
    {
        std::cout << actor_name << ": Reached the timeout of " << timeout << " ms" << std::endl;
        return 1;
    }
}