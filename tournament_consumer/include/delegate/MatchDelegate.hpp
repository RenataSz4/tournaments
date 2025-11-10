//
// Created by developer on 10/13/25.
//

#ifndef CONSUMER_MATCHDELEGATE_HPP
#define CONSUMER_MATCHDELEGATE_HPP

#include <memory>
#include <print>
#include <vector>
#include <set>

#include "event/TeamAddEvent.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "domain/Tournament.hpp"
#include "domain/Match.hpp"

class MatchDelegate {
    std::shared_ptr<IMatchRepository> matchRepository;
    std::shared_ptr<GroupRepository> groupRepository;
    std::shared_ptr<TournamentRepository> tournamentRepository;

    // Helper methods
    bool AreAllGroupsFilled(const std::string& tournamentId, const domain::Tournament& tournament);
    bool MatchesAlreadyGenerated(const std::string& tournamentId);
    void GenerateRoundRobinMatches(const std::string& tournamentId, const std::vector<std::shared_ptr<domain::Group>>& groups);
    void GenerateMundialMatches(const std::string& tournamentId, const std::vector<std::shared_ptr<domain::Group>>& groups);
    std::vector<domain::Match> CreateGroupMatches(const std::string& tournamentId, const domain::Group& group);

public:
    MatchDelegate(
        const std::shared_ptr<IMatchRepository>& matchRepository,
        const std::shared_ptr<GroupRepository>& groupRepository,
        const std::shared_ptr<TournamentRepository>& tournamentRepository
    );

    void ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent);
};

inline MatchDelegate::MatchDelegate(
    const std::shared_ptr<IMatchRepository> &matchRepository,
    const std::shared_ptr<GroupRepository> &groupRepository,
    const std::shared_ptr<TournamentRepository> &tournamentRepository)
    : matchRepository(matchRepository),
      groupRepository(groupRepository),
      tournamentRepository(tournamentRepository) {}

inline void MatchDelegate::ProcessTeamAddition(const domain::TeamAddEvent& teamAddEvent) {
    std::println("[MatchDelegate] Processing team addition for tournament: {}", teamAddEvent.tournamentId);

    try {
        // Get tournament info
        auto tournament = tournamentRepository->ReadById(teamAddEvent.tournamentId);
        if (tournament == nullptr) {
            std::println("[MatchDelegate] Tournament {} not found", teamAddEvent.tournamentId);
            return;
        }

        // Check if all groups are filled
        if (!AreAllGroupsFilled(teamAddEvent.tournamentId, *tournament)) {
            std::println("[MatchDelegate] Not all groups are filled yet for tournament {}", teamAddEvent.tournamentId);
            return;
        }

        // Check if matches were already generated
        if (MatchesAlreadyGenerated(teamAddEvent.tournamentId)) {
            std::println("[MatchDelegate] Matches already generated for tournament {}", teamAddEvent.tournamentId);
            return;
        }

        // Get all groups
        auto groups = groupRepository->FindByTournamentId(teamAddEvent.tournamentId);

        // Generate matches based on tournament type
        auto tournamentType = tournament->Format().Type();

        std::println("[MatchDelegate] Generating matches for tournament type: {}", static_cast<int>(tournamentType));

        switch (tournamentType) {
            case domain::TournamentType::ROUND_ROBIN:
                GenerateRoundRobinMatches(teamAddEvent.tournamentId, groups);
                break;
            case domain::TournamentType::MUNDIAL:
                GenerateMundialMatches(teamAddEvent.tournamentId, groups);
                break;
            case domain::TournamentType::NFL:
                // NFL format - could be implemented later
                std::println("[MatchDelegate] NFL format not yet implemented");
                break;
            default:
                std::println("[MatchDelegate] Unknown tournament type");
                break;
        }

    } catch (const std::exception& e) {
        std::println("[MatchDelegate] Error processing team addition: {}", e.what());
    }
}

inline bool MatchDelegate::AreAllGroupsFilled(const std::string& tournamentId, const domain::Tournament& tournament) {
    auto groups = groupRepository->FindByTournamentId(tournamentId);

    int expectedGroups = tournament.Format().NumberOfGroups();
    int maxTeamsPerGroup = tournament.Format().MaxTeamsPerGroup();

    if (groups.size() != expectedGroups) {
        std::println("[MatchDelegate] Groups count mismatch: {} vs expected {}", groups.size(), expectedGroups);
        return false;
    }

    for (const auto& group : groups) {
        if (group->Teams().size() != maxTeamsPerGroup) {
            std::println("[MatchDelegate] Group {} has {} teams, expected {}",
                        group->Id(), group->Teams().size(), maxTeamsPerGroup);
            return false;
        }
    }

    return true;
}

inline bool MatchDelegate::MatchesAlreadyGenerated(const std::string& tournamentId) {
    auto matches = matchRepository->FindByTournamentId(tournamentId);
    return !matches.empty();
}

inline void MatchDelegate::GenerateRoundRobinMatches(
    const std::string& tournamentId,
    const std::vector<std::shared_ptr<domain::Group>>& groups) {

    std::println("[MatchDelegate] Generating Round-Robin matches for {} groups", groups.size());

    // In Round-Robin, all teams play against each other within their groups
    for (const auto& group : groups) {
        auto matches = CreateGroupMatches(tournamentId, *group);

        for (const auto& match : matches) {
            try {
                matchRepository->Create(match);
            } catch (const std::exception& e) {
                std::println("[MatchDelegate] Error creating match: {}", e.what());
            }
        }
    }

    std::println("[MatchDelegate] Round-Robin matches generated successfully");
}

inline void MatchDelegate::GenerateMundialMatches(
    const std::string& tournamentId,
    const std::vector<std::shared_ptr<domain::Group>>& groups) {

    std::println("[MatchDelegate] Generating Mundial (World Cup) matches for {} groups", groups.size());

    // Mundial format: Group stage (regular) + Elimination rounds
    // For now, we only generate the group stage matches
    // Elimination rounds would be generated after group stage is completed

    for (const auto& group : groups) {
        auto matches = CreateGroupMatches(tournamentId, *group);

        for (const auto& match : matches) {
            try {
                matchRepository->Create(match);
            } catch (const std::exception& e) {
                std::println("[MatchDelegate] Error creating match: {}", e.what());
            }
        }
    }

    std::println("[MatchDelegate] Mundial group stage matches generated successfully");
}

inline std::vector<domain::Match> MatchDelegate::CreateGroupMatches(
    const std::string& tournamentId,
    const domain::Group& group) {

    std::vector<domain::Match> matches;
    const auto& teams = group.Teams();

    // Generate all possible pairings (Round-Robin within group)
    for (size_t i = 0; i < teams.size(); i++) {
        for (size_t j = i + 1; j < teams.size(); j++) {
            domain::Match match(
                teams[i].Id,     // homeTeamId
                teams[i].Name,   // homeTeamName
                teams[j].Id,     // visitorTeamId
                teams[j].Name,   // visitorTeamName
                "regular",       // round
                domain::MatchStatus::PENDING
            );

            match.SetTournamentId(tournamentId);
            matches.push_back(match);
        }
    }

    std::println("[MatchDelegate] Created {} matches for group {}", matches.size(), group.Id());
    return matches;
}

#endif //CONSUMER_MATCHDELEGATE_HPP