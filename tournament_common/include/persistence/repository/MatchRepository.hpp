//
// Created by root on 11/4/25.
//

#ifndef TOURNAMENTS_MATCHREPOSITORY_HPP
#define TOURNAMENTS_MATCHREPOSITORY_HPP

#include <pqxx/pqxx>
#include "IMatchRepository.hpp"
#include "persistence/configuration/IDbConnectionProvider.hpp"

class MatchRepository : public IMatchRepository {
    std::shared_ptr<IDbConnectionProvider> connectionProvider;

    // Helper methods
    std::shared_ptr<domain::Match> MapRowToMatch(const pqxx::row& row);
    static std::string MatchStatusToString(domain::MatchStatus status);
    static domain::MatchStatus StringToMatchStatus(const std::string& status);

public:
    explicit MatchRepository(const std::shared_ptr<IDbConnectionProvider>& connectionProvider);

    // IRepository methods
    std::shared_ptr<domain::Match> ReadById(std::string id) override;
    std::string Create(const domain::Match& entity) override;
    std::string Update(const domain::Match& entity) override;
    void Delete(std::string id) override;
    std::vector<std::shared_ptr<domain::Match>> ReadAll() override;

    // IMatchRepository methods
    std::vector<std::shared_ptr<domain::Match>> FindByTournamentId(
        const std::string_view& tournamentId,
        const std::optional<std::string>& statusFilter = std::nullopt
    ) override;

    std::shared_ptr<domain::Match> FindByTournamentIdAndMatchId(
        const std::string_view& tournamentId,
        const std::string_view& matchId
    ) override;

    void UpdateScore(
        const std::string_view& matchId,
        int homeScore,
        int visitorScore
    ) override;

    void UpdateWinner(
        const std::string_view& matchId,
        const std::string_view& winnerId
    ) override;

    // Legacy methods
    std::shared_ptr<domain::Match> FindLastOpenMatch(const std::string_view& tournamentId) override;
    std::vector<domain::Match> FindMatchesByTournamentAndRound(const std::string_view& tournamentId) override;
};

#endif //TOURNAMENTS_MATCHREPOSITORY_HPP