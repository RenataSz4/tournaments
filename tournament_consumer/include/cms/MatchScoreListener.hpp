//
// Created by developer on 11/10/25.
//

#ifndef LISTENER_MATCHSCORE_LISTENER_HPP
#define LISTENER_MATCHSCORE_LISTENER_HPP

#include <nlohmann/json.hpp>

#include "QueueMessageListener.hpp"
#include "delegate/PlayoffDelegate.hpp"
#include "event/ScoreRegisteredEvent.hpp"

class MatchScoreListener : public QueueMessageListener {
    void processMessage(const std::string& message) override;
    std::shared_ptr<PlayoffDelegate> playoffDelegate;

public:
    MatchScoreListener(
        const std::shared_ptr<ConnectionManager>& connectionManager,
        const std::shared_ptr<PlayoffDelegate>& playoffDelegate
    );
    ~MatchScoreListener() override;
};

inline MatchScoreListener::MatchScoreListener(
    const std::shared_ptr<ConnectionManager>& connectionManager,
    const std::shared_ptr<PlayoffDelegate>& playoffDelegate)
    : QueueMessageListener(connectionManager),
      playoffDelegate(playoffDelegate) {
}

inline MatchScoreListener::~MatchScoreListener() {
    Stop();
}

inline void MatchScoreListener::processMessage(const std::string& message) {
    std::println("[MatchScoreListener] Received message: {}", message);

    try {
        domain::ScoreRegisteredEvent event = nlohmann::json::parse(message);
        playoffDelegate->ProcessScoreRegistered(event);
    } catch (const std::exception& e) {
        std::println("[MatchScoreListener] Error parsing message: {}", e.what());
    }
}

#endif //LISTENER_MATCHSCORE_LISTENER_HPP
