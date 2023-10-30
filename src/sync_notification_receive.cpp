#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>

#include "dds/dds.hpp"
#include "ActionSyncData.hpp"

using namespace org::eclipse::cyclonedds;

int main() {
    try {
        std::cout << "=== [Subscriber] Create reader." << std::endl;

        /* First, a domain participant is needed.
         * Create one on the default domain. */
        dds::domain::DomainParticipant participant(domain::default_id());

        /* To subscribe to something, a topic is needed. */
        dds::topic::Topic<ActionSyncData::Notification> topic(participant, "ActionSyncData_Notification");

        /* A reader also needs a subscriber. */
        dds::sub::Subscriber subscriber(participant);

        /* Now, the reader can be created to subscribe to a HelloWorld message. */
        dds::sub::DataReader<ActionSyncData::Notification> reader(subscriber, topic);

        /* Poll until a message has been read.
         * It isn't really recommended to do this kind wait in a polling loop.
         * It's done here just to illustrate the easiest way to get data.
         * Please take a look at Listeners and WaitSets for much better
         * solutions, albeit somewhat more elaborate ones. */
        std::cout << "=== [Subscriber] Wait for message." << std::endl;
        bool poll = true;
        while (poll) {
            /* For this example, the reader will return a set of messages (aka
             * Samples). There are other ways of getting samples from reader.
             * See the various read() and take() functions that are present. */
            dds::sub::LoanedSamples<ActionSyncData::Notification> samples;

            /* Try taking samples from the reader. */
            samples = reader.take();

            /* Are samples read? */
            if (samples.length() > 0) {
                /* Use an iterator to run over the set of samples. */
                dds::sub::LoanedSamples<ActionSyncData::Notification>::const_iterator sample_iter;
                for (sample_iter = samples.begin();
                     sample_iter < samples.end();
                     ++sample_iter) {
                    /* Get the message and sample information. */
                    const ActionSyncData::Notification& msg = sample_iter->data();
                    const dds::sub::SampleInfo& info = sample_iter->info();

                    /* Sometimes a sample is read, only to indicate a data
                     * state change (which can be found in the info). If
                     * that's the case, only the key value of the sample
                     * is set. The other data parts are not.
                     * Check if this sample has valid data. */
                    if (info.valid()) {
                        std::cout << "=== [Subscriber] Message received:" << std::endl;
                        std::cout << "    actor_name : " << msg.actor_name() << std::endl;
                        std::cout << "    parameters : " << msg.parameters() << std::endl;
                        std::cout << "    waitable   : " << msg.waitable().action_name() << " in " << msg.waitable().graph_name() << std::endl;

                        /* Only 1 message is expected in this example. */
                        poll = false;
                    }
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
    } catch (const dds::core::Exception& e) {
        std::cerr << "=== [Subscriber] DDS exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "=== [Subscriber] C++ exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "=== [Subscriber] Done." << std::endl;

    return EXIT_SUCCESS;
}
