#ifndef DOMAIN_MATCH_HPP
#define DOMAIN_MATCH_HPP

#include <string>
#include "Team.hpp"

namespace domain {
    enum class MatchPhase {
        GROUP_STAGE,           // Fase de grupos
        ROUND_OF_16,          // Octavos de final
        QUARTER_FINALS,       // Cuartos de final
        SEMI_FINALS,          // Semifinales
        THIRD_PLACE,          // Tercer lugar
        FINAL                 // Final
    };

    class Match {
    private:
        std::string id;
        std::string tournamentId;
        std::string groupId;           // Solo para fase de grupos
        Team homeTeam;
        Team awayTeam;
        int homeScore;                 // 0-10
        int awayScore;                 // 0-10
        MatchPhase phase;
        bool isCompleted;

    public:
        Match() : homeScore(-1), awayScore(-1), isCompleted(false), phase(MatchPhase::GROUP_STAGE) {}

        // Getters y setters
        std::string Id() const { return id; }
        std::string& Id() { return id; }

        std::string TournamentId() const { return tournamentId; }
        std::string& TournamentId() { return tournamentId; }

        std::string GroupId() const { return groupId; }
        std::string& GroupId() { return groupId; }

        Team HomeTeam() const { return homeTeam; }
        Team& HomeTeam() { return homeTeam; }

        Team AwayTeam() const { return awayTeam; }
        Team& AwayTeam() { return awayTeam; }

        int HomeScore() const { return homeScore; }
        int& HomeScore() { return homeScore; }

        int AwayScore() const { return awayScore; }
        int& AwayScore() { return awayScore; }

        MatchPhase Phase() const { return phase; }
        MatchPhase& Phase() { return phase; }

        bool IsCompleted() const { return isCompleted; }
        bool& IsCompleted() { return isCompleted; }
    };
}
#endif