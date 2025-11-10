//
// Created by developer on 11/10/25.
//

#ifndef CONSUMER_PLAYOFFDELEGATE_HPP
#define CONSUMER_PLAYOFFDELEGATE_HPP

#include <memory>
#include <print>
#include <vector>
#include <map>
#include <algorithm>

#include "event/ScoreRegisteredEvent.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "domain/Tournament.hpp"
#include "domain/Match.hpp"
#include "domain/Team.hpp"

class PlayoffDelegate {
    std::shared_ptr<IMatchRepository> matchRepository;
    std::shared_ptr<GroupRepository> groupRepository;
    std::shared_ptr<TournamentRepository> tournamentRepository;

    // Helper methods
    bool IsRegularPhaseComplete(const std::string& tournamentId);
    bool ArePlayoffsGenerated(const std::string& tournamentId);
    void GenerateMundialPlayoffs(const std::string& tournamentId);
    std::vector<domain::Team> GetTopTeamsFromGroups(const std::string& tournamentId, int teamsPerGroup);
    std::vector<domain::Team> CalculateGroupStandings(const domain::Group& group,
                                                      const std::vector<std::shared_ptr<domain::Match>>& groupMatches);
    void CreateEliminationRound(const std::string& tournamentId,
                                const std::vector<domain::Team>& teams,
                                const std::string& roundName);
    bool IsTournamentComplete(const std::string& tournamentId);
    void MarkTournamentComplete(const std::string& tournamentId);

public:
    PlayoffDelegate(
        const std::shared_ptr<IMatchRepository>& matchRepository,
        const std::shared_ptr<GroupRepository>& groupRepository,
        const std::shared_ptr<TournamentRepository>& tournamentRepository
    );

    void ProcessScoreRegistered(const domain::ScoreRegisteredEvent& event);
};

inline PlayoffDelegate::PlayoffDelegate(
    const std::shared_ptr<IMatchRepository>& matchRepository,
    const std::shared_ptr<GroupRepository>& groupRepository,
    const std::shared_ptr<TournamentRepository>& tournamentRepository)
    : matchRepository(matchRepository),
      groupRepository(groupRepository),
      tournamentRepository(tournamentRepository) {}

inline void PlayoffDelegate::ProcessScoreRegistered(const domain::ScoreRegisteredEvent& event) {
    std::println("[PlayoffDelegate] Processing score registered for match: {}", event.matchId);

    try {
        // Get tournament info
        auto tournament = tournamentRepository->ReadById(event.tournamentId);
        if (tournament == nullptr) {
            std::println("[PlayoffDelegate] Tournament {} not found", event.tournamentId);
            return;
        }

        auto tournamentType = tournament->Format().Type();

        // Handle based on tournament type
        switch (tournamentType) {
            case domain::TournamentType::MUNDIAL: {
                // Check if regular phase just completed
                if (event.round == "regular" && IsRegularPhaseComplete(event.tournamentId)) {
                    if (!ArePlayoffsGenerated(event.tournamentId)) {
                        std::println("[PlayoffDelegate] Regular phase complete, generating playoffs");
                        GenerateMundialPlayoffs(event.tournamentId);
                    }
                }
                break;
            }

            case domain::TournamentType::ROUND_ROBIN: {
                // Round-robin tournaments don't have playoffs
                // Check if tournament is complete
                if (IsTournamentComplete(event.tournamentId)) {
                    MarkTournamentComplete(event.tournamentId);
                }
                break;
            }

            case domain::TournamentType::NFL: {
                // NFL format to be implemented
                std::println("[PlayoffDelegate] NFL format not yet implemented");
                break;
            }
        }

        // Check if this was the final match
        if (event.round == "final" && event.winnerId.has_value()) {
            std::println("[PlayoffDelegate] Final match completed, tournament finished!");
            MarkTournamentComplete(event.tournamentId);
        }

    } catch (const std::exception& e) {
        std::println("[PlayoffDelegate] Error processing score: {}", e.what());
    }
}

inline bool PlayoffDelegate::IsRegularPhaseComplete(const std::string& tournamentId) {
    auto matches = matchRepository->FindByTournamentId(tournamentId, std::optional<std::string>("pending"));

    // Check if there are any pending regular matches
    for (const auto& match : matches) {
        if (match->Round() == "regular") {
            return false; // Still have pending regular matches
        }
    }

    // Verify all regular matches are played
    auto allMatches = matchRepository->FindByTournamentId(tournamentId);
    bool hasRegularMatches = false;
    for (const auto& match : allMatches) {
        if (match->Round() == "regular") {
            hasRegularMatches = true;
            if (match->Status() != domain::MatchStatus::PLAYED) {
                return false;
            }
        }
    }

    return hasRegularMatches; // True if we have regular matches and all are played
}

inline bool PlayoffDelegate::ArePlayoffsGenerated(const std::string& tournamentId) {
    auto allMatches = matchRepository->FindByTournamentId(tournamentId);

    for (const auto& match : allMatches) {
        if (match->Round() == "quarterfinals" ||
            match->Round() == "semifinals" ||
            match->Round() == "final") {
            return true;
        }
    }

    return false;
}

inline void PlayoffDelegate::GenerateMundialPlayoffs(const std::string& tournamentId) {
    std::println("[PlayoffDelegate] Generating Mundial playoff structure");

    // Get top 2 teams from each group
    auto topTeams = GetTopTeamsFromGroups(tournamentId, 2);

    if (topTeams.size() != 16) {
        std::println("[PlayoffDelegate] Expected 16 teams for playoffs, got {}", topTeams.size());
        return;
    }

    // Generate quarterfinals (top 16 → 8 teams)
    // In a real Mundial, there would be Round of 16 first, but we'll simplify to quarterfinals
    std::println("[PlayoffDelegate] Generating quarterfinals with {} teams", topTeams.size());

    // Create 8 matches for quarterfinals (Round of 16)
    std::vector<domain::Team> quarterfinalists;
    for (size_t i = 0; i < topTeams.size(); i += 2) {
        domain::Match match(
            topTeams[i].Id,
            topTeams[i].Name,
            topTeams[i + 1].Id,
            topTeams[i + 1].Name,
            "quarterfinals",
            domain::MatchStatus::PENDING
        );
        match.SetTournamentId(tournamentId);

        try {
            matchRepository->Create(match);
            std::println("[PlayoffDelegate] Created quarterfinal: {} vs {}",
                        topTeams[i].Name, topTeams[i + 1].Name);
        } catch (const std::exception& e) {
            std::println("[PlayoffDelegate] Error creating quarterfinal match: {}", e.what());
        }
    }

    std::println("[PlayoffDelegate] Mundial playoffs structure created");
}

inline std::vector<domain::Team> PlayoffDelegate::GetTopTeamsFromGroups(
    const std::string& tournamentId,
    int teamsPerGroup) {

    std::vector<domain::Team> topTeams;
    auto groups = groupRepository->FindByTournamentId(tournamentId);
    auto allMatches = matchRepository->FindByTournamentId(tournamentId);

    for (const auto& group : groups) {
        // Filter matches for this group
        std::vector<std::shared_ptr<domain::Match>> groupMatches;
        for (const auto& match : allMatches) {
            if (match->Round() == "regular") {
                // Check if both teams are in this group
                bool homeInGroup = false;
                bool visitorInGroup = false;

                for (const auto& team : group->Teams()) {
                    if (team.Id == match->HomeTeam().Id) homeInGroup = true;
                    if (team.Id == match->VisitorTeam().Id) visitorInGroup = true;
                }

                if (homeInGroup && visitorInGroup) {
                    groupMatches.push_back(match);
                }
            }
        }

        // Calculate standings
        auto standings = CalculateGroupStandings(*group, groupMatches);

        // Add top N teams
        for (int i = 0; i < std::min(teamsPerGroup, static_cast<int>(standings.size())); i++) {
            topTeams.push_back(standings[i]);
        }
    }

    return topTeams;
}

inline std::vector<domain::Team> PlayoffDelegate::CalculateGroupStandings(
    const domain::Group& group,
    const std::vector<std::shared_ptr<domain::Match>>& groupMatches) {

    // Calculate points for each team
    std::map<std::string, int> points;
    std::map<std::string, int> goalsFor;
    std::map<std::string, int> goalsAgainst;

    // Initialize
    for (const auto& team : group.Teams()) {
        points[team.Id] = 0;
        goalsFor[team.Id] = 0;
        goalsAgainst[team.Id] = 0;
    }

    // Calculate points from matches
    for (const auto& match : groupMatches) {
        if (match->Status() == domain::MatchStatus::PLAYED && match->HasScore()) {
            auto score = match->MatchScore().value();

            goalsFor[match->HomeTeam().Id] += score.home;
            goalsAgainst[match->HomeTeam().Id] += score.visitor;
            goalsFor[match->VisitorTeam().Id] += score.visitor;
            goalsAgainst[match->VisitorTeam().Id] += score.home;

            if (score.home > score.visitor) {
                points[match->HomeTeam().Id] += 3; // Win
            } else if (score.visitor > score.home) {
                points[match->VisitorTeam().Id] += 3; // Win
            } else {
                points[match->HomeTeam().Id] += 1; // Draw
                points[match->VisitorTeam().Id] += 1; // Draw
            }
        }
    }

    // Sort teams by points, then goal difference
    std::vector<domain::Team> standings = group.Teams();
    std::sort(standings.begin(), standings.end(),
        [&points, &goalsFor, &goalsAgainst](const domain::Team& a, const domain::Team& b) {
            int pointsA = points[a.Id];
            int pointsB = points[b.Id];

            if (pointsA != pointsB) {
                return pointsA > pointsB;
            }

            // Same points, check goal difference
            int gdA = goalsFor[a.Id] - goalsAgainst[a.Id];
            int gdB = goalsFor[b.Id] - goalsAgainst[b.Id];

            if (gdA != gdB) {
                return gdA > gdB;
            }

            // Same GD, check goals scored
            return goalsFor[a.Id] > goalsFor[b.Id];
        }
    );

    return standings;
}

inline bool PlayoffDelegate::IsTournamentComplete(const std::string& tournamentId) {
    auto matches = matchRepository->FindByTournamentId(tournamentId, std::optional<std::string>("pending"));
    return matches.empty(); // No pending matches = tournament complete
}

inline void PlayoffDelegate::MarkTournamentComplete(const std::string& tournamentId) {
    std::println("[PlayoffDelegate] Tournament {} is now COMPLETE!", tournamentId);
    // Here you could update tournament status in database
    // For now, just log it
}

#endif //CONSUMER_PLAYOFFDELEGATE_HPP
