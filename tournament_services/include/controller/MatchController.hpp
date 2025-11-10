#ifndef MATCH_CONTROLLER_HPP
#define MATCH_CONTROLLER_HPP

#include <string>
#include <memory>
#include <crow.h>
#include <nlohmann/json.hpp>

#include "delegate/IMatchDelegate.hpp"

class MatchController {
    std::shared_ptr<IMatchDelegate> matchDelegate;

public:
    explicit MatchController(const std::shared_ptr<IMatchDelegate>& matchDelegate);
    ~MatchController() = default;

    crow::response GetMatches(const crow::request& request, const std::string& tournamentId);
    crow::response GetMatch(const std::string& tournamentId, const std::string& matchId);
    crow::response UpdateMatchScore(const crow::request& request, const std::string& tournamentId, const std::string& matchId);
};

#endif /* MATCH_CONTROLLER_HPP */
