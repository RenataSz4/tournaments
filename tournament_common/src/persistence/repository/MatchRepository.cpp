//
// Created by root on 11/4/25.
//

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

#include "persistence/repository/MatchRepository.hpp"
#include "domain/Utilities.hpp"
#include "persistence/configuration/PostgresConnection.hpp"

MatchRepository::MatchRepository(const std::shared_ptr<IDbConnectionProvider>& connectionProvider)
    : connectionProvider(connectionProvider) {}

std::shared_ptr<domain::Match> MatchRepository::ReadById(std::string id) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    const pqxx::result result = tx.exec(pqxx::prepped{"select_match_by_id"}, id);
    tx.commit();

    if (result.empty()) {
        return nullptr;
    }

    return MapRowToMatch(result[0]);
}

std::string MatchRepository::Create(const domain::Match& entity) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    nlohmann::json matchDoc = entity;

    pqxx::work tx(*(connection->connection));
    const pqxx::result result = tx.exec(
        pqxx::prepped{"insert_match"},
        pqxx::params{
            entity.TournamentId(),
            entity.HomeTeam().Id,
            entity.VisitorTeam().Id,
            entity.Round(),
            MatchStatusToString(entity.Status()),
            matchDoc.dump()
        }
    );
    tx.commit();

    return result[0]["id"].c_str();
}

std::string MatchRepository::Update(const domain::Match& entity) {
    auto pooled = connectionProvider->Connection();
    auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    nlohmann::json matchDoc = entity;

    pqxx::work tx(*(connection->connection));
    pqxx::result result = tx.exec(
        pqxx::prepped{"update_match"},
        pqxx::params{entity.Id(), matchDoc.dump()}
    );
    tx.commit();

    return result[0]["id"].c_str();
}

void MatchRepository::Delete(std::string id) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    tx.exec(pqxx::prepped{"delete_match"}, id);
    tx.commit();
}

std::vector<std::shared_ptr<domain::Match>> MatchRepository::ReadAll() {
    std::vector<std::shared_ptr<domain::Match>> matches;

    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    const pqxx::result result{tx.exec("SELECT * FROM matches")};
    tx.commit();

    for (auto row : result) {
        matches.push_back(MapRowToMatch(row));
    }

    return matches;
}

std::vector<std::shared_ptr<domain::Match>> MatchRepository::FindByTournamentId(
    const std::string_view& tournamentId,
    const std::optional<std::string>& statusFilter
) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    pqxx::result result;

    if (statusFilter.has_value()) {
        result = tx.exec(
            pqxx::prepped{"select_matches_by_tournament_and_status"},
            pqxx::params{tournamentId.data(), statusFilter.value()}
        );
    } else {
        result = tx.exec(
            pqxx::prepped{"select_matches_by_tournament"},
            pqxx::params{tournamentId.data()}
        );
    }
    tx.commit();

    std::vector<std::shared_ptr<domain::Match>> matches;
    for (auto row : result) {
        matches.push_back(MapRowToMatch(row));
    }
    return matches;
}

std::shared_ptr<domain::Match> MatchRepository::FindByTournamentIdAndMatchId(
    const std::string_view& tournamentId,
    const std::string_view& matchId
) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    const pqxx::result result = tx.exec(
        pqxx::prepped{"select_match_by_tournament_and_match_id"},
        pqxx::params{tournamentId.data(), matchId.data()}
    );
    tx.commit();

    if (result.empty()) {
        return nullptr;
    }

    return MapRowToMatch(result[0]);
}

void MatchRepository::UpdateScore(
    const std::string_view& matchId,
    int homeScore,
    int visitorScore
) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    tx.exec(
        pqxx::prepped{"update_match_score"},
        pqxx::params{matchId.data(), homeScore, visitorScore}
    );
    tx.commit();
}

void MatchRepository::UpdateWinner(
    const std::string_view& matchId,
    const std::string_view& winnerId
) {
    auto pooled = connectionProvider->Connection();
    const auto connection = dynamic_cast<PostgresConnection*>(&*pooled);

    pqxx::work tx(*(connection->connection));
    tx.exec(
        pqxx::prepped{"update_match_winner"},
        pqxx::params{matchId.data(), winnerId.data()}
    );
    tx.commit();
}

std::shared_ptr<domain::Match> MatchRepository::FindLastOpenMatch(const std::string_view& tournamentId) {
    // Legacy method - kept for backwards compatibility
    return nullptr;
}

std::vector<domain::Match> MatchRepository::FindMatchesByTournamentAndRound(const std::string_view& tournamentId) {
    // Legacy method - kept for backwards compatibility
    return std::vector<domain::Match>();
}

// Private helper methods
std::shared_ptr<domain::Match> MatchRepository::MapRowToMatch(const pqxx::row& row) {
    auto match = std::make_shared<domain::Match>();

    // Set basic fields
    match->SetId(row["id"].c_str());
    match->SetTournamentId(row["tournament_id"].c_str());
    match->SetRound(row["round"].c_str());

    // Set home team - try to get name from JOIN first, fallback to document
    domain::Team homeTeam;
    homeTeam.Id = row["home_team_id"].c_str();
    if (!row["home_team_name"].is_null()) {
        homeTeam.Name = row["home_team_name"].c_str();
    } else {
        nlohmann::json docJson = nlohmann::json::parse(row["document"].c_str());
        if (docJson.contains("homeTeam") && docJson["homeTeam"].contains("name")) {
            homeTeam.Name = docJson["homeTeam"]["name"].get<std::string>();
        }
    }
    match->SetHomeTeam(homeTeam);

    // Set visitor team - try to get name from JOIN first, fallback to document
    domain::Team visitorTeam;
    visitorTeam.Id = row["visitor_team_id"].c_str();
    if (!row["visitor_team_name"].is_null()) {
        visitorTeam.Name = row["visitor_team_name"].c_str();
    } else {
        nlohmann::json docJson = nlohmann::json::parse(row["document"].c_str());
        if (docJson.contains("visitorTeam") && docJson["visitorTeam"].contains("name")) {
            visitorTeam.Name = docJson["visitorTeam"]["name"].get<std::string>();
        }
    }
    match->SetVisitorTeam(visitorTeam);

    // Set score if available
    if (!row["home_score"].is_null() && !row["visitor_score"].is_null()) {
        domain::Score score;
        score.home = row["home_score"].as<int>();
        score.visitor = row["visitor_score"].as<int>();
        match->SetScore(score);
    }

    // Set winner if available
    if (!row["winner_team_id"].is_null()) {
        match->SetWinnerId(row["winner_team_id"].c_str());
    }

    // Set status
    std::string statusStr = row["match_status"].c_str();
    match->SetStatus(StringToMatchStatus(statusStr));

    return match;
}

std::string MatchRepository::MatchStatusToString(domain::MatchStatus status) {
    switch (status) {
        case domain::MatchStatus::PENDING: return "pending";
        case domain::MatchStatus::PLAYED: return "played";
        default: return "pending";
    }
}

domain::MatchStatus MatchRepository::StringToMatchStatus(const std::string& status) {
    if (status == "played") {
        return domain::MatchStatus::PLAYED;
    }
    return domain::MatchStatus::PENDING;
}
