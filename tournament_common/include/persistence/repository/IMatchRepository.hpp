//
// Created by developer on 10/14/25.
//

#ifndef TOURNAMENTS_IMATCHREPOSITORY_HPP
#define TOURNAMENTS_IMATCHREPOSITORY_HPP

#include <string_view>
#include <vector>
#include <memory>
#include <optional>

#include "domain/Match.hpp"

class IMatchRepository : public IRepository<domain::Match, std::string> {
public:
    virtual ~IMatchRepository() = default;

    // Find matches by tournament with optional status filter
    virtual std::vector<std::shared_ptr<domain::Match>> FindByTournamentId(
        const std::string_view& tournamentId,
        const std::optional<std::string>& statusFilter = std::nullopt
    ) = 0;

    // Find match by tournament and match ID
    virtual std::shared_ptr<domain::Match> FindByTournamentIdAndMatchId(
        const std::string_view& tournamentId,
        const std::string_view& matchId
    ) = 0;

    // Update match score
    virtual void UpdateScore(
        const std::string_view& matchId,
        int homeScore,
        int visitorScore
    ) = 0;

    // Update match winner
    virtual void UpdateWinner(
        const std::string_view& matchId,
        const std::string_view& winnerId
    ) = 0;

    // Legacy methods (keep for backwards compatibility)
    virtual std::shared_ptr<domain::Match> FindLastOpenMatch(const std::string_view& tournamentId) = 0;
    virtual std::vector<domain::Match> FindMatchesByTournamentAndRound(const std::string_view& tournamentId) = 0;
};
#endif //TOURNAMENTS_IMATCHREPOSITORY_HPP