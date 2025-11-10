#ifndef SERVICE_MATCH_DELEGATE_HPP
#define SERVICE_MATCH_DELEGATE_HPP

#include <memory>
#include <expected>
#include <string>
#include <optional>

#include "IMatchDelegate.hpp"
#include "persistence/repository/IRepository.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "cms/QueueMessageProducer.hpp"
#include "domain/Tournament.hpp"
#include "domain/Team.hpp"
#include "domain/Match.hpp"

class MatchDelegate : public IMatchDelegate {
    std::shared_ptr<IRepository<domain::Tournament, std::string>> tournamentRepository;
    std::shared_ptr<IMatchRepository> matchRepository;
    std::shared_ptr<IRepository<domain::Team, std::string_view>> teamRepository;
    std::shared_ptr<QueueMessageProducer> producer;

public:
    MatchDelegate(
        const std::shared_ptr<IRepository<domain::Tournament, std::string>>& tournamentRepository,
        const std::shared_ptr<IMatchRepository>& matchRepository,
        const std::shared_ptr<IRepository<domain::Team, std::string_view>>& teamRepository,
        const std::shared_ptr<QueueMessageProducer>& producer
    );

    std::expected<std::vector<std::shared_ptr<domain::Match>>, std::string>
    GetMatchesByTournament(
        const std::string_view& tournamentId,
        const std::optional<std::string>& statusFilter = std::nullopt
    ) override;

    std::expected<std::shared_ptr<domain::Match>, std::string>
    GetMatchById(
        const std::string_view& tournamentId,
        const std::string_view& matchId
    ) override;

    std::expected<std::string, std::string>
    CreateMatch(
        const std::string_view& tournamentId,
        const domain::Match& match
    ) override;

    std::expected<void, std::string>
    UpdateMatchScore(
        const std::string_view& tournamentId,
        const std::string_view& matchId,
        int homeScore,
        int visitorScore
    ) override;

private:
    // Helper method to check if round is elimination phase
    bool IsEliminationRound(const std::string& round) const;
};

#endif /* SERVICE_MATCH_DELEGATE_HPP */
