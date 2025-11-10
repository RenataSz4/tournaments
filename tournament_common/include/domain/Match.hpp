#ifndef DOMAIN_MATCH_HPP
#define DOMAIN_MATCH_HPP

#include <string>
#include <optional>
#include "domain/Team.hpp"

namespace domain {
    enum class MatchStatus {
        PENDING,
        PLAYED
    };

    enum class Winner {
        HOME,
        VISITOR
    };

    struct Score {
        int home;
        int visitor;

        [[nodiscard]] Winner GetWinner() const {
            if (visitor < home) {
                return Winner::HOME;
            }
            return Winner::VISITOR;
        }
    };

    class Match {
        std::string id;
        std::string tournamentId;
        Team homeTeam;
        Team visitorTeam;
        std::string round;
        std::optional<Score> score;
        std::optional<std::string> winnerId;
        MatchStatus status;

    public:
        explicit Match(
            const std::string& homeTeamId = "",
            const std::string& homeTeamName = "",
            const std::string& visitorTeamId = "",
            const std::string& visitorTeamName = "",
            const std::string& round = "regular",
            MatchStatus status = MatchStatus::PENDING
        ) : homeTeam{homeTeamId, homeTeamName},
            visitorTeam{visitorTeamId, visitorTeamName},
            round(round),
            status(status) {}

        // Const getters
        [[nodiscard]] const std::string& Id() const { return id; }
        [[nodiscard]] const std::string& TournamentId() const { return tournamentId; }
        [[nodiscard]] const Team& HomeTeam() const { return homeTeam; }
        [[nodiscard]] const Team& VisitorTeam() const { return visitorTeam; }
        [[nodiscard]] const std::string& Round() const { return round; }
        [[nodiscard]] const std::optional<Score>& MatchScore() const { return score; }
        [[nodiscard]] const std::optional<std::string>& WinnerId() const { return winnerId; }
        [[nodiscard]] MatchStatus Status() const { return status; }

        // Mutable getters
        std::string& MutableId() { return id; }
        std::string& MutableTournamentId() { return tournamentId; }
        Team& MutableHomeTeam() { return homeTeam; }
        Team& MutableVisitorTeam() { return visitorTeam; }
        std::string& MutableRound() { return round; }
        std::optional<Score>& MutableScore() { return score; }
        std::optional<std::string>& MutableWinnerId() { return winnerId; }
        MatchStatus& MutableStatus() { return status; }

        // Setters
        void SetId(const std::string& newId) { id = newId; }
        void SetTournamentId(const std::string& tid) { tournamentId = tid; }
        void SetHomeTeam(const Team& team) { homeTeam = team; }
        void SetVisitorTeam(const Team& team) { visitorTeam = team; }
        void SetRound(const std::string& newRound) { round = newRound; }
        void SetScore(const Score& newScore) { score = newScore; }
        void SetWinnerId(const std::string& wid) { winnerId = wid; }
        void SetStatus(MatchStatus newStatus) { status = newStatus; }

        // Utility methods
        void ClearScore() { score.reset(); }
        void ClearWinnerId() { winnerId.reset(); }

        [[nodiscard]] bool HasScore() const { return score.has_value(); }
        [[nodiscard]] bool HasWinner() const { return winnerId.has_value(); }
    };

}
#endif