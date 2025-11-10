#define JSON_CONTENT_TYPE "application/json"
#define CONTENT_TYPE_HEADER "content-type"

#include "configuration/RouteDefinition.hpp"
#include "controller/MatchController.hpp"
#include "domain/Utilities.hpp"

MatchController::MatchController(const std::shared_ptr<IMatchDelegate>& matchDelegate)
    : matchDelegate(matchDelegate) {}

crow::response MatchController::GetMatches(const crow::request& request, const std::string& tournamentId) {
    // Validate tournament ID format
    if (!std::regex_match(tournamentId, ID_VALUE)) {
        return crow::response{crow::BAD_REQUEST, "Invalid tournament ID format"};
    }

    // Get optional status filter from query parameter
    std::optional<std::string> statusFilter;
    char* showMatches = request.url_params.get("showMatches");
    if (showMatches != nullptr) {
        statusFilter = std::string(showMatches);
    }

    // Call delegate
    auto result = matchDelegate->GetMatchesByTournament(tournamentId, statusFilter);

    if (!result.has_value()) {
        const std::string& error = result.error();

        // Map errors to HTTP status codes
        if (error.find("not found") != std::string::npos) {
            return crow::response{crow::NOT_FOUND, error};
        }
        if (error.find("Invalid status filter") != std::string::npos) {
            return crow::response{422, error};
        }
        return crow::response{crow::INTERNAL_SERVER_ERROR, error};
    }

    // Success: return matches array
    nlohmann::json body = result.value();
    crow::response res{crow::OK, body.dump()};
    res.add_header(CONTENT_TYPE_HEADER, JSON_CONTENT_TYPE);
    return res;
}

crow::response MatchController::GetMatch(const std::string& tournamentId, const std::string& matchId) {
    // Validate IDs format
    if (!std::regex_match(tournamentId, ID_VALUE)) {
        return crow::response{crow::BAD_REQUEST, "Invalid tournament ID format"};
    }
    if (!std::regex_match(matchId, ID_VALUE)) {
        return crow::response{crow::BAD_REQUEST, "Invalid match ID format"};
    }

    // Call delegate
    auto result = matchDelegate->GetMatchById(tournamentId, matchId);

    if (!result.has_value()) {
        const std::string& error = result.error();

        // Map errors to HTTP status codes
        if (error.find("not found") != std::string::npos) {
            return crow::response{crow::NOT_FOUND, error};
        }
        return crow::response{crow::INTERNAL_SERVER_ERROR, error};
    }

    auto match = result.value();
    if (match == nullptr) {
        return crow::response{crow::NOT_FOUND, "Match not found"};
    }

    // Success: return match
    nlohmann::json body = match;
    crow::response res{crow::OK, body.dump()};
    res.add_header(CONTENT_TYPE_HEADER, JSON_CONTENT_TYPE);
    return res;
}

crow::response MatchController::UpdateMatchScore(
    const crow::request& request,
    const std::string& tournamentId,
    const std::string& matchId) {

    // Validate IDs format
    if (!std::regex_match(tournamentId, ID_VALUE)) {
        return crow::response{crow::BAD_REQUEST, "Invalid tournament ID format"};
    }
    if (!std::regex_match(matchId, ID_VALUE)) {
        return crow::response{crow::BAD_REQUEST, "Invalid match ID format"};
    }

    // Validate JSON body
    if (!nlohmann::json::accept(request.body)) {
        return crow::response{crow::BAD_REQUEST, "Invalid JSON"};
    }

    try {
        auto body = nlohmann::json::parse(request.body);

        // Validate body structure
        if (!body.contains("score")) {
            return crow::response{422, "Missing 'score' field in request body"};
        }

        auto scoreObj = body["score"];
        if (!scoreObj.contains("home") || !scoreObj.contains("visitor")) {
            return crow::response{422, "Score must contain 'home' and 'visitor' fields"};
        }

        // Extract scores
        int homeScore = scoreObj["home"].get<int>();
        int visitorScore = scoreObj["visitor"].get<int>();

        // Call delegate
        auto result = matchDelegate->UpdateMatchScore(tournamentId, matchId, homeScore, visitorScore);

        if (!result.has_value()) {
            const std::string& error = result.error();

            // Map errors to HTTP status codes
            if (error.find("not found") != std::string::npos) {
                return crow::response{crow::NOT_FOUND, error};
            }
            if (error.find("already been played") != std::string::npos ||
                error.find("not allowed") != std::string::npos ||
                error.find("non-negative") != std::string::npos) {
                return crow::response{422, error};
            }
            return crow::response{crow::INTERNAL_SERVER_ERROR, error};
        }

        // Success: 204 No Content
        return crow::response{crow::NO_CONTENT};

    } catch (const nlohmann::json::exception& e) {
        return crow::response{422, std::string("JSON parsing error: ") + e.what()};
    } catch (...) {
        return crow::response{422, "Invalid request body format"};
    }
}

// Register routes
REGISTER_ROUTE(MatchController, GetMatches, "/tournaments/<string>/matches", "GET"_method)
REGISTER_ROUTE(MatchController, GetMatch, "/tournaments/<string>/matches/<string>", "GET"_method)
REGISTER_ROUTE(MatchController, UpdateMatchScore, "/tournaments/<string>/matches/<string>", "PATCH"_method)
