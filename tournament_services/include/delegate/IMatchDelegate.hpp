#ifndef SERVICE_IMATCH_DELEGATE_HPP
#define SERVICE_IMATCH_DELEGATE_HPP

#include <string>
#include <string_view>
#include <vector>
#include <expected>
#include <optional>
#include <memory>

#include "domain/Match.hpp"

class IMatchDelegate {
public:
    virtual ~IMatchDelegate() = default;

    // Get matches by tournament with optional status filter
    virtual std::expected<std::vector<std::shared_ptr<domain::Match>>, std::string>
    GetMatchesByTournament(
        const std::string_view& tournamentId,
        const std::optional<std::string>& statusFilter = std::nullopt
    ) = 0;

    // Get single match by tournament and match ID
    virtual std::expected<std::shared_ptr<domain::Match>, std::string>
    GetMatchById(
        const std::string_view& tournamentId,
        const std::string_view& matchId
    ) = 0;

    // Create a new match
    virtual std::expected<std::string, std::string>
    CreateMatch(
        const std::string_view& tournamentId,
        const domain::Match& match
    ) = 0;

    // Update match score with business logic validation
    virtual std::expected<void, std::string>
    UpdateMatchScore(
        const std::string_view& tournamentId,
        const std::string_view& matchId,
        int homeScore,
        int visitorScore
    ) = 0;
};

#endif /* SERVICE_IMATCH_DELEGATE_HPP */
