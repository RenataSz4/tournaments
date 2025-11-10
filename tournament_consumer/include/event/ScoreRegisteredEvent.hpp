//
// Created by developer on 11/10/25.
//

#ifndef TOURNAMENTS_SCOREREGISTEREDEVENT_HPP
#define TOURNAMENTS_SCOREREGISTEREDEVENT_HPP

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace domain {
    struct ScoreRegisteredEvent {
        std::string tournamentId;
        std::string matchId;
        std::string round;
        std::optional<std::string> winnerId;
        std::string homeTeamId;
        std::string visitorTeamId;
        int homeScore;
        int visitorScore;
    };

    inline void from_json(const nlohmann::json &json, ScoreRegisteredEvent &event) {
        json.at("tournamentId").get_to(event.tournamentId);
        json.at("matchId").get_to(event.matchId);
        json.at("round").get_to(event.round);

        if (json.contains("winnerId") && !json["winnerId"].is_null()) {
            event.winnerId = json["winnerId"].get<std::string>();
        }

        json.at("homeTeamId").get_to(event.homeTeamId);
        json.at("visitorTeamId").get_to(event.visitorTeamId);
        json.at("homeScore").get_to(event.homeScore);
        json.at("visitorScore").get_to(event.visitorScore);
    }
}

#endif //TOURNAMENTS_SCOREREGISTEREDEVENT_HPP
