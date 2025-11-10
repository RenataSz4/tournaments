//
// Created by tomas on 9/6/25.
//
#include <activemq/library/ActiveMQCPP.h>
#include <thread>

#include "configuration/ContainerSetup.hpp"
#include "cms/GroupAddTeamListener.hpp"
#include "cms/MatchScoreListener.hpp"

int main() {
    activemq::library::ActiveMQCPP::initializeLibrary();
    {
        std::println("before container");
        const auto container = config::containerSetup();
        std::println("after container");

        // Thread for team addition events
        std::thread teamAddThread([&] {
            auto listener = container->resolve<GroupAddTeamListener>();
            listener->Start("team.added.to.group");
        });

        // Thread for score registered events
        std::thread scoreRegisteredThread([&] {
            auto listener = container->resolve<MatchScoreListener>();
            listener->Start("match.score.registered");
        });

        teamAddThread.join();
        scoreRegisteredThread.join();
    }
    activemq::library::ActiveMQCPP::shutdownLibrary();
    return 0;
}