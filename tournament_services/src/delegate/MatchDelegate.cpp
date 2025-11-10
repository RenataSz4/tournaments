#include "delegate/MatchDelegate.hpp"
#include <utility>
#include <format>
#include <nlohmann/json.hpp>

MatchDelegate::MatchDelegate(
    const std::shared_ptr<IRepository<domain::Tournament, std::string>>& tournamentRepository,
    const std::shared_ptr<IMatchRepository>& matchRepository,
    const std::shared_ptr<IRepository<domain::Team, std::string_view>>& teamRepository,
    const std::shared_ptr<QueueMessageProducer>& producer)
    : tournamentRepository(tournamentRepository),
      matchRepository(matchRepository),
      teamRepository(teamRepository),
      producer(producer) {}

std::expected<std::vector<std::shared_ptr<domain::Match>>, std::string>
MatchDelegate::GetMatchesByTournament(
    const std::string_view& tournamentId,
    const std::optional<std::string>& statusFilter) {

    // Validate tournament exists
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected("Tournament not found");
    }

    // Validate status filter if provided
    if (statusFilter.has_value()) {
        const auto& status = statusFilter.value();
        if (status != "pending" && status != "played") {
            return std::unexpected("Invalid status filter. Must be 'pending' or 'played'");
        }
    }

    try {
        return matchRepository->FindByTournamentId(tournamentId, statusFilter);
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Error reading matches: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error reading matches");
    }
}

std::expected<std::shared_ptr<domain::Match>, std::string>
MatchDelegate::GetMatchById(
    const std::string_view& tournamentId,
    const std::string_view& matchId) {

    // Validate tournament exists
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected("Tournament not found");
    }

    try {
        auto match = matchRepository->FindByTournamentIdAndMatchId(tournamentId, matchId);
        if (match == nullptr) {
            return std::unexpected("Match not found");
        }
        return match;
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Error reading match: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error reading match");
    }
}

std::expected<std::string, std::string>
MatchDelegate::CreateMatch(
    const std::string_view& tournamentId,
    const domain::Match& match) {

    // Validate tournament exists
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected("Tournament not found");
    }

    // Validate home team exists
    auto homeTeam = teamRepository->ReadById(match.HomeTeam().Id);
    if (homeTeam == nullptr) {
        return std::unexpected(std::format("Home team '{}' not found", match.HomeTeam().Id));
    }

    // Validate visitor team exists
    auto visitorTeam = teamRepository->ReadById(match.VisitorTeam().Id);
    if (visitorTeam == nullptr) {
        return std::unexpected(std::format("Visitor team '{}' not found", match.VisitorTeam().Id));
    }

    // Validate teams are different
    if (match.HomeTeam().Id == match.VisitorTeam().Id) {
        return std::unexpected("Home team and visitor team must be different");
    }

    // Validate round
    const auto& round = match.Round();
    if (round != "regular" && round != "quarterfinals" &&
        round != "semifinals" && round != "final") {
        return std::unexpected("Invalid round. Must be 'regular', 'quarterfinals', 'semifinals', or 'final'");
    }

    try {
        domain::Match m = match;
        m.SetTournamentId(tournament->Id());

        // Ensure match is created in pending status
        m.SetStatus(domain::MatchStatus::PENDING);
        m.ClearScore();
        m.ClearWinnerId();

        auto id = matchRepository->Create(m);
        return id;
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Error creating match: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error creating match");
    }
}

std::expected<void, std::string>
MatchDelegate::UpdateMatchScore(
    const std::string_view& tournamentId,
    const std::string_view& matchId,
    int homeScore,
    int visitorScore) {

    // Validate tournament exists
    auto tournament = tournamentRepository->ReadById(tournamentId.data());
    if (tournament == nullptr) {
        return std::unexpected("Tournament not found");
    }

    // Get existing match
    auto matchResult = matchRepository->FindByTournamentIdAndMatchId(tournamentId, matchId);
    if (matchResult == nullptr) {
        return std::unexpected("Match not found");
    }

    auto match = matchResult;

    // Business rule: Match must be in pending status
    if (match->Status() != domain::MatchStatus::PENDING) {
        return std::unexpected("Match has already been played. Cannot update score.");
    }

    // Validate scores are non-negative
    if (homeScore < 0 || visitorScore < 0) {
        return std::unexpected("Scores must be non-negative");
    }

    // Business rule: No ties in elimination rounds
    if (IsEliminationRound(match->Round()) && homeScore == visitorScore) {
        return std::unexpected(std::format(
            "Ties are not allowed in elimination rounds ({}). A winner must be determined.",
            match->Round()
        ));
    }

    // Calculate winner_id
    std::string winnerId;
    if (homeScore > visitorScore) {
        winnerId = match->HomeTeam().Id;
    } else if (visitorScore > homeScore) {
        winnerId = match->VisitorTeam().Id;
    }
    // If it's a tie in regular round, winnerId remains empty

    try {
        // Update score (this also sets status to PLAYED in the repository)
        matchRepository->UpdateScore(matchId, homeScore, visitorScore);

        // Update winner if there is one
        if (!winnerId.empty()) {
            matchRepository->UpdateWinner(matchId, winnerId);
        }

        // Publish ScoreRegistered event to ActiveMQ
        nlohmann::json eventPayload = {
            {"tournamentId", std::string(tournamentId)},
            {"matchId", std::string(matchId)},
            {"round", match->Round()},
            {"winnerId", winnerId.empty() ? nullptr : nlohmann::json(winnerId)},
            {"homeTeamId", match->HomeTeam().Id},
            {"visitorTeamId", match->VisitorTeam().Id},
            {"homeScore", homeScore},
            {"visitorScore", visitorScore}
        };

        producer->SendMessage(eventPayload.dump(), "match.score.registered");

        return {};
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Error updating match score: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error updating match score");
    }
}

// Private helper methods
bool MatchDelegate::IsEliminationRound(const std::string& round) const {
    return round == "quarterfinals" ||
           round == "semifinals" ||
           round == "final";
}
