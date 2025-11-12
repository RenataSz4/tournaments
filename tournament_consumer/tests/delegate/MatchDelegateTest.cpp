#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

#include "domain/Match.hpp"
#include "domain/Team.hpp"
#include "domain/Group.hpp"
#include "domain/Tournament.hpp"
#include "persistence/repository/IMatchRepository.hpp"
#include "persistence/repository/GroupRepository.hpp"
#include "persistence/repository/TournamentRepository.hpp"
#include "delegate/MatchDelegate.hpp"
#include "event/TeamAddEvent.hpp"
#include "event/ScoreUpdateEvent.hpp"

namespace {
    class MatchRepositoryMock : public IMatchRepository {
    public:
        MOCK_METHOD(std::string, Create, (const domain::Match&), (override));
        MOCK_METHOD(std::shared_ptr<domain::Match>, ReadById, (std::string), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, ReadAll, (), (override));
        MOCK_METHOD(std::string, Update, (const domain::Match&), (override));
        MOCK_METHOD(void, Delete, (std::string), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, FindByTournamentId, (const std::string_view&), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, FindPlayedByTournamentId, (const std::string_view&), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, FindPendingByTournamentId, (const std::string_view&), (override));
        MOCK_METHOD(std::shared_ptr<domain::Match>, FindByTournamentIdAndMatchId, (const std::string_view&, const std::string_view&), (override));
        MOCK_METHOD(std::shared_ptr<domain::Match>, FindLastOpenMatch, (const std::string_view&), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Match>>, FindMatchesByTournamentAndRound, (const std::string_view&, domain::Round), (override));
        MOCK_METHOD(bool, TournamentExists, (const std::string_view&), (override));
    };

    class GroupRepositoryMock : public GroupRepository {
    public:
        GroupRepositoryMock() : GroupRepository(nullptr) {}
        MOCK_METHOD(std::shared_ptr<domain::Group>, FindByTournamentIdAndGroupId, (const std::string_view&, const std::string_view&), (override));
        MOCK_METHOD(std::vector<std::shared_ptr<domain::Group>>, FindByTournamentId, (const std::string_view&), (override));
    };

    class TournamentRepositoryMock : public TournamentRepository {
    public:
        TournamentRepositoryMock() : TournamentRepository(nullptr) {}
    };

    class ConsumerMatchDelegateTest : public ::testing::Test {
    protected:
        std::shared_ptr<MatchRepositoryMock> matchRepoMock;
        std::shared_ptr<GroupRepositoryMock> groupRepoMock;
        std::shared_ptr<TournamentRepositoryMock> tournamentRepoMock;
        std::shared_ptr<MatchDelegate> matchDelegate;

        void SetUp() override {
            matchRepoMock = std::make_shared<MatchRepositoryMock>();
            groupRepoMock = std::make_shared<GroupRepositoryMock>();
            tournamentRepoMock = std::make_shared<TournamentRepositoryMock>();
            matchDelegate = std::make_shared<MatchDelegate>(matchRepoMock, groupRepoMock, tournamentRepoMock);
        }

        void TearDown() override {
            testing::Mock::VerifyAndClearExpectations(matchRepoMock.get());
            testing::Mock::VerifyAndClearExpectations(groupRepoMock.get());
        }
    };

    // ProcessTeamAddition tests

    TEST_F(ConsumerMatchDelegateTest, ProcessTeamAddition_GroupWith4Teams_CreatesMatches) {
        domain::TeamAddEvent event;
        event.tournamentId = "tournament-1";
        event.groupId = "group-A";
        event.teamId = "team-4";

        auto group = std::make_shared<domain::Group>();
        group->SetId("group-A");
        std::vector<domain::Team> teams = {
            {"team-1", "Team A"},
            {"team-2", "Team B"},
            {"team-3", "Team C"},
            {"team-4", "Team D"}
        };
        group->SetTeams(teams);

        EXPECT_CALL(*groupRepoMock, FindByTournamentIdAndGroupId(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(std::string_view("group-A"))))
                .WillOnce(testing::Return(group));

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::REGULAR)))
                .WillOnce(testing::Return(std::vector<std::shared_ptr<domain::Match>>{}));

        // Expect 6 matches to be created (4 teams round-robin: C(4,2) = 6)
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(6)
                .WillRepeatedly(testing::Return("match-id"));

        matchDelegate->ProcessTeamAddition(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessTeamAddition_GroupWith3Teams_DoesNotCreateMatches) {
        domain::TeamAddEvent event;
        event.tournamentId = "tournament-1";
        event.groupId = "group-A";
        event.teamId = "team-3";

        auto group = std::make_shared<domain::Group>();
        group->SetId("group-A");
        std::vector<domain::Team> teams = {
            {"team-1", "Team A"},
            {"team-2", "Team B"},
            {"team-3", "Team C"}
        };
        group->SetTeams(teams);

        EXPECT_CALL(*groupRepoMock, FindByTournamentIdAndGroupId(testing::_, testing::_))
                .WillOnce(testing::Return(group));

        // Should not try to create matches
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(0);

        matchDelegate->ProcessTeamAddition(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessTeamAddition_GroupNotFound_DoesNothing) {
        domain::TeamAddEvent event;
        event.tournamentId = "tournament-1";
        event.groupId = "non-existent";
        event.teamId = "team-1";

        EXPECT_CALL(*groupRepoMock, FindByTournamentIdAndGroupId(testing::_, testing::_))
                .WillOnce(testing::Return(nullptr));

        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(0);

        matchDelegate->ProcessTeamAddition(event);
    }

    // ProcessScoreUpdate tests - Regular matches

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_RegularMatch_ChecksIfAllComplete) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "match-1";
        event.score = {2, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("match-1");
        match->SetRound(domain::Round::REGULAR);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(std::string_view("match-1"))))
                .WillOnce(testing::Return(match));

        // Mock regular matches - only 1 complete (not all 48)
        std::vector<std::shared_ptr<domain::Match>> regularMatches;
        for (int i = 0; i < 10; ++i) {
            auto m = std::make_shared<domain::Match>();
            m->SetRound(domain::Round::REGULAR);
            regularMatches.push_back(m);
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::REGULAR)))
                .WillOnce(testing::Return(regularMatches));

        matchDelegate->ProcessScoreUpdate(event);
    }

    // ProcessScoreUpdate tests - Playoff matches

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_EighthsMatch_WaitsForAllBeforeCreatingQuarters) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "match-1";
        event.score = {2, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("match-1");
        match->SetHomeTeamId("team-1");
        match->SetHomeTeamName("Team A");
        match->SetVisitorTeamId("team-2");
        match->SetVisitorTeamName("Team B");
        match->SetRound(domain::Round::EIGHTHS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        // Return 8 matches but only 7 have scores
        std::vector<std::shared_ptr<domain::Match>> eighthsMatches;
        for (int i = 0; i < 8; ++i) {
            auto m = std::make_shared<domain::Match>();
            m->SetRound(domain::Round::EIGHTHS);
            if (i < 7) {
                m->SetScore({i, i + 1});
            }
            eighthsMatches.push_back(m);
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::EIGHTHS)))
                .WillOnce(testing::Return(eighthsMatches));

        // Should not create quarters yet
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(0);

        matchDelegate->ProcessScoreUpdate(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastEighthsMatch_CreatesQuarters) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "match-8";
        event.score = {2, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("match-8");
        match->SetHomeTeamId("team-15");
        match->SetHomeTeamName("Team O");
        match->SetVisitorTeamId("team-16");
        match->SetVisitorTeamName("Team P");
        match->SetRound(domain::Round::EIGHTHS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        // Return 8 eighths matches, all with scores
        std::vector<std::shared_ptr<domain::Match>> eighthsMatches;
        for (int i = 0; i < 8; ++i) {
            auto m = std::make_shared<domain::Match>();
            m->SetId("match-" + std::to_string(i + 1));
            m->SetHomeTeamId("team-h-" + std::to_string(i));
            m->SetHomeTeamName("Home Team " + std::to_string(i));
            m->SetVisitorTeamId("team-v-" + std::to_string(i));
            m->SetVisitorTeamName("Visitor Team " + std::to_string(i));
            m->SetRound(domain::Round::EIGHTHS);
            m->SetScore({i % 2 == 0 ? 2 : 1, i % 2 == 0 ? 1 : 2});
            eighthsMatches.push_back(m);
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::EIGHTHS)))
                .Times(2)  // Called twice: once to check, once to create quarters
                .WillRepeatedly(testing::Return(eighthsMatches));

        // Should create 4 quarters matches
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(4)
                .WillRepeatedly(testing::Return("quarter-match-id"));

        matchDelegate->ProcessScoreUpdate(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastQuarterMatch_CreatesSemis) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "match-4";
        event.score = {3, 2};

        auto match = std::make_shared<domain::Match>();
        match->SetId("match-4");
        match->SetHomeTeamId("team-7");
        match->SetHomeTeamName("Team G");
        match->SetVisitorTeamId("team-8");
        match->SetVisitorTeamName("Team H");
        match->SetRound(domain::Round::QUARTERS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        // Return 4 quarters matches, all with scores
        std::vector<std::shared_ptr<domain::Match>> quartersMatches;
        for (int i = 0; i < 4; ++i) {
            auto m = std::make_shared<domain::Match>();
            m->SetId("quarter-" + std::to_string(i + 1));
            m->SetHomeTeamId("team-h-" + std::to_string(i));
            m->SetHomeTeamName("QHome " + std::to_string(i));
            m->SetVisitorTeamId("team-v-" + std::to_string(i));
            m->SetVisitorTeamName("QVisitor " + std::to_string(i));
            m->SetRound(domain::Round::QUARTERS);
            m->SetScore({i % 2 == 0 ? 2 : 1, i % 2 == 0 ? 1 : 2});
            quartersMatches.push_back(m);
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::QUARTERS)))
                .Times(2)
                .WillRepeatedly(testing::Return(quartersMatches));

        // Should create 2 semis matches
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(2)
                .WillRepeatedly(testing::Return("semi-match-id"));

        matchDelegate->ProcessScoreUpdate(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastSemiMatch_CreatesFinal) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "match-2";
        event.score = {1, 0};

        auto match = std::make_shared<domain::Match>();
        match->SetId("match-2");
        match->SetHomeTeamId("team-3");
        match->SetHomeTeamName("Team C");
        match->SetVisitorTeamId("team-4");
        match->SetVisitorTeamName("Team D");
        match->SetRound(domain::Round::SEMIS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        // Return 2 semis matches, both with scores
        std::vector<std::shared_ptr<domain::Match>> semisMatches;
        for (int i = 0; i < 2; ++i) {
            auto m = std::make_shared<domain::Match>();
            m->SetId("semi-" + std::to_string(i + 1));
            m->SetHomeTeamId("team-h-" + std::to_string(i));
            m->SetHomeTeamName("SHome " + std::to_string(i));
            m->SetVisitorTeamId("team-v-" + std::to_string(i));
            m->SetVisitorTeamName("SVisitor " + std::to_string(i));
            m->SetRound(domain::Round::SEMIS);
            m->SetScore({i == 0 ? 2 : 1, i == 0 ? 1 : 2});
            semisMatches.push_back(m);
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::SEMIS)))
                .Times(2)
                .WillRepeatedly(testing::Return(semisMatches));

        // Should create 1 final match
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(1)
                .WillOnce(testing::Return("final-match-id"));

        matchDelegate->ProcessScoreUpdate(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_FinalMatch_DeclaresChampion) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "final-match";
        event.score = {3, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("final-match");
        match->SetHomeTeamId("team-1");
        match->SetHomeTeamName("Champion Team");
        match->SetVisitorTeamId("team-2");
        match->SetVisitorTeamName("Runner-up Team");
        match->SetRound(domain::Round::FINAL);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        // Should not create any more matches
        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(0);

        matchDelegate->ProcessScoreUpdate(event);
    }

    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_MatchNotFound_DoesNothing) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "non-existent";
        event.score = {1, 0};

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(nullptr));

        matchDelegate->ProcessScoreUpdate(event);
    }

    // Test que valida la creación de octavos con emparejamientos correctos según patrón de Mundial
    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_AllRegularMatchesComplete_CreatesEighthsWithCorrectWorldCupPairings) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "regular-48";
        event.score = {2, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("regular-48");
        match->SetRound(domain::Round::REGULAR);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(std::string_view("regular-48"))))
                .WillOnce(testing::Return(match));

        std::vector<std::shared_ptr<domain::Group>> groups;

        // Grupo A
        auto groupA = std::make_shared<domain::Group>();
        groupA->SetId("A");
        groupA->SetName("Group A");
        std::vector<domain::Team> teamsA = {
                {"team-a1", "Chile"},      // 1° lugar
                {"team-a2", "Argentina"},  // 2° lugar
                {"team-a3", "Team A3"},
                {"team-a4", "Team A4"}
        };
        groupA->SetTeams(teamsA);
        groups.push_back(groupA);

        // Grupo B
        auto groupB = std::make_shared<domain::Group>();
        groupB->SetId("B");
        groupB->SetName("Group B");
        std::vector<domain::Team> teamsB = {
                {"team-b1", "Mexico"},    // 1° lugar
                {"team-b2", "Brazil"},    // 2° lugar
                {"team-b3", "Team B3"},
                {"team-b4", "Team B4"}
        };
        groupB->SetTeams(teamsB);
        groups.push_back(groupB);

        // Grupo C
        auto groupC = std::make_shared<domain::Group>();
        groupC->SetId("C");
        groupC->SetName("Group C");
        std::vector<domain::Team> teamsC = {
                {"team-c1", "Spain"},
                {"team-c2", "Portugal"},
                {"team-c3", "Team C3"},
                {"team-c4", "Team C4"}
        };
        groupC->SetTeams(teamsC);
        groups.push_back(groupC);

        // Grupo D
        auto groupD = std::make_shared<domain::Group>();
        groupD->SetId("D");
        groupD->SetName("Group D");
        std::vector<domain::Team> teamsD = {
                {"team-d1", "Germany"},
                {"team-d2", "France"},
                {"team-d3", "Team D3"},
                {"team-d4", "Team D4"}
        };
        groupD->SetTeams(teamsD);
        groups.push_back(groupD);

        // Grupo E
        auto groupE = std::make_shared<domain::Group>();
        groupE->SetId("E");
        groupE->SetName("Group E");
        std::vector<domain::Team> teamsE = {
                {"team-e1", "England"},
                {"team-e2", "Italy"},
                {"team-e3", "Team E3"},
                {"team-e4", "Team E4"}
        };
        groupE->SetTeams(teamsE);
        groups.push_back(groupE);

        // Grupo F
        auto groupF = std::make_shared<domain::Group>();
        groupF->SetId("F");
        groupF->SetName("Group F");
        std::vector<domain::Team> teamsF = {
                {"team-f1", "Uruguay"},
                {"team-f2", "Colombia"},
                {"team-f3", "Team F3"},
                {"team-f4", "Team F4"}
        };
        groupF->SetTeams(teamsF);
        groups.push_back(groupF);

        // Grupo G
        auto groupG = std::make_shared<domain::Group>();
        groupG->SetId("G");
        groupG->SetName("Group G");
        std::vector<domain::Team> teamsG = {
                {"team-g1", "Netherlands"},
                {"team-g2", "Belgium"},
                {"team-g3", "Team G3"},
                {"team-g4", "Team G4"}
        };
        groupG->SetTeams(teamsG);
        groups.push_back(groupG);

        // Grupo H
        auto groupH = std::make_shared<domain::Group>();
        groupH->SetId("H");
        groupH->SetName("Group H");
        std::vector<domain::Team> teamsH = {
                {"team-h1", "Croatia"},
                {"team-h2", "Serbia"},
                {"team-h3", "Team H3"},
                {"team-h4", "Team H4"}
        };
        groupH->SetTeams(teamsH);
        groups.push_back(groupH);

        EXPECT_CALL(*groupRepoMock, FindByTournamentIdAndGroupId(
                testing::Eq(std::string_view("tournament-1")),
                testing::_))
                .WillRepeatedly(testing::Invoke([&](std::string_view tournamentId, std::string_view groupId)
                                                        -> std::shared_ptr<domain::Group> {
                    for (const auto& group : groups) {
                        if (group->Id() == groupId) {
                            return group;
                        }
                    }
                    return nullptr;
                }));

        EXPECT_CALL(*groupRepoMock, FindByTournamentId(
                testing::Eq(std::string_view("tournament-1"))))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(testing::Return(groups));

        std::vector<std::shared_ptr<domain::Match>> groupMatches;

        for (const auto& group : groups) {
            auto& teams = group->Teams();
            for (size_t i = 0; i < teams.size(); ++i) {
                for (size_t j = i + 1; j < teams.size(); ++j) {
                    auto m = std::make_shared<domain::Match>();
                    m->SetHomeTeamId(teams[i].Id);
                    m->SetVisitorTeamId(teams[j].Id);
                    m->SetRound(domain::Round::REGULAR);
                    if (i == 0) {
                        m->SetScore({3, 0});
                    } else if (i == 1 && j > 1) {
                        m->SetScore({2, 0});
                    } else {
                        m->SetScore({1, 1});
                    }
                    groupMatches.push_back(m);
                }
            }
        }

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::REGULAR)))
                .WillRepeatedly(testing::Return(groupMatches));

        std::vector<domain::Match> createdEighths;

        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(8)
                .WillRepeatedly(testing::Invoke([&createdEighths](const domain::Match& match) {
                    createdEighths.push_back(match);
                    return "eighth-" + std::to_string(createdEighths.size());
                }));

        matchDelegate->ProcessScoreUpdate(event);

        ASSERT_EQ(8, createdEighths.size()) << "Debe crear exactamente 8 matches de octavos";

        // Match 1: A1 (Chile) vs B2 (Brazil)
        EXPECT_EQ("team-a1", createdEighths[0].HomeTeamId()) << "Match 1: Home debe ser A1";
        EXPECT_EQ("team-b2", createdEighths[0].VisitorTeamId()) << "Match 1: Visitor debe ser B2";
        EXPECT_EQ("Chile", createdEighths[0].HomeTeamName());
        EXPECT_EQ("Brazil", createdEighths[0].VisitorTeamName());

        // Match 2: A2 (Argentina) vs B1 (Mexico)
        EXPECT_EQ("team-a2", createdEighths[1].HomeTeamId()) << "Match 2: Home debe ser A2";
        EXPECT_EQ("team-b1", createdEighths[1].VisitorTeamId()) << "Match 2: Visitor debe ser B1";
        EXPECT_EQ("Argentina", createdEighths[1].HomeTeamName());
        EXPECT_EQ("Mexico", createdEighths[1].VisitorTeamName());

        // Match 3: C1 (Spain) vs D2 (France)
        EXPECT_EQ("team-c1", createdEighths[2].HomeTeamId()) << "Match 3: Home debe ser C1";
        EXPECT_EQ("team-d2", createdEighths[2].VisitorTeamId()) << "Match 3: Visitor debe ser D2";
        EXPECT_EQ("Spain", createdEighths[2].HomeTeamName());
        EXPECT_EQ("France", createdEighths[2].VisitorTeamName());

        // Match 4: C2 (Portugal) vs D1 (Germany)
        EXPECT_EQ("team-c2", createdEighths[3].HomeTeamId()) << "Match 4: Home debe ser C2";
        EXPECT_EQ("team-d1", createdEighths[3].VisitorTeamId()) << "Match 4: Visitor debe ser D1";
        EXPECT_EQ("Portugal", createdEighths[3].HomeTeamName());
        EXPECT_EQ("Germany", createdEighths[3].VisitorTeamName());

        // Match 5: E1 (England) vs F2 (Colombia)
        EXPECT_EQ("team-e1", createdEighths[4].HomeTeamId()) << "Match 5: Home debe ser E1";
        EXPECT_EQ("team-f2", createdEighths[4].VisitorTeamId()) << "Match 5: Visitor debe ser F2";
        EXPECT_EQ("England", createdEighths[4].HomeTeamName());
        EXPECT_EQ("Colombia", createdEighths[4].VisitorTeamName());

        // Match 6: E2 (Italy) vs F1 (Uruguay)
        EXPECT_EQ("team-e2", createdEighths[5].HomeTeamId()) << "Match 6: Home debe ser E2";
        EXPECT_EQ("team-f1", createdEighths[5].VisitorTeamId()) << "Match 6: Visitor debe ser F1";
        EXPECT_EQ("Italy", createdEighths[5].HomeTeamName());
        EXPECT_EQ("Uruguay", createdEighths[5].VisitorTeamName());

        // Match 7: G1 (Netherlands) vs H2 (Serbia)
        EXPECT_EQ("team-g1", createdEighths[6].HomeTeamId()) << "Match 7: Home debe ser G1";
        EXPECT_EQ("team-h2", createdEighths[6].VisitorTeamId()) << "Match 7: Visitor debe ser H2";
        EXPECT_EQ("Netherlands", createdEighths[6].HomeTeamName());
        EXPECT_EQ("Serbia", createdEighths[6].VisitorTeamName());

        // Match 8: G2 (Belgium) vs H1 (Croatia)
        EXPECT_EQ("team-g2", createdEighths[7].HomeTeamId()) << "Match 8: Home debe ser G2";
        EXPECT_EQ("team-h1", createdEighths[7].VisitorTeamId()) << "Match 8: Visitor debe ser H1";
        EXPECT_EQ("Belgium", createdEighths[7].HomeTeamName());
        EXPECT_EQ("Croatia", createdEighths[7].VisitorTeamName());
    }

    // Test que valida emparejamientos correctos en cuartos de final
    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastEighthsMatch_CreatesQuartersWithCorrectPairings) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "eighth-8";
        event.score = {2, 1};

        auto match = std::make_shared<domain::Match>();
        match->SetId("eighth-8");
        match->SetHomeTeamId("team-g2");
        match->SetHomeTeamName("Belgium");
        match->SetVisitorTeamId("team-h1");
        match->SetVisitorTeamName("Croatia");
        match->SetRound(domain::Round::EIGHTHS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        std::vector<std::shared_ptr<domain::Match>> eighthsMatches;

        // Match 1: Chile(2) vs Brazil(1) -> Gana Chile
        auto e1 = std::make_shared<domain::Match>();
        e1->SetId("eighth-1");
        e1->SetHomeTeamId("team-a1");
        e1->SetHomeTeamName("Chile");
        e1->SetVisitorTeamId("team-b2");
        e1->SetVisitorTeamName("Brazil");
        e1->SetRound(domain::Round::EIGHTHS);
        e1->SetScore({2, 1});
        eighthsMatches.push_back(e1);

        // Match 2: Argentina(3) vs Mexico(1) -> Gana Argentina
        auto e2 = std::make_shared<domain::Match>();
        e2->SetId("eighth-2");
        e2->SetHomeTeamId("team-a2");
        e2->SetHomeTeamName("Argentina");
        e2->SetVisitorTeamId("team-b1");
        e2->SetVisitorTeamName("Mexico");
        e2->SetRound(domain::Round::EIGHTHS);
        e2->SetScore({3, 1});
        eighthsMatches.push_back(e2);

        // Match 3: Spain(1) vs France(2) -> Gana France
        auto e3 = std::make_shared<domain::Match>();
        e3->SetId("eighth-3");
        e3->SetHomeTeamId("team-c1");
        e3->SetHomeTeamName("Spain");
        e3->SetVisitorTeamId("team-d2");
        e3->SetVisitorTeamName("France");
        e3->SetRound(domain::Round::EIGHTHS);
        e3->SetScore({1, 2});
        eighthsMatches.push_back(e3);

        // Match 4: Portugal(2) vs Germany(0) -> Gana Portugal
        auto e4 = std::make_shared<domain::Match>();
        e4->SetId("eighth-4");
        e4->SetHomeTeamId("team-c2");
        e4->SetHomeTeamName("Portugal");
        e4->SetVisitorTeamId("team-d1");
        e4->SetVisitorTeamName("Germany");
        e4->SetRound(domain::Round::EIGHTHS);
        e4->SetScore({2, 0});
        eighthsMatches.push_back(e4);

        // Match 5: England(3) vs Colombia(2) -> Gana England
        auto e5 = std::make_shared<domain::Match>();
        e5->SetId("eighth-5");
        e5->SetHomeTeamId("team-e1");
        e5->SetHomeTeamName("England");
        e5->SetVisitorTeamId("team-f2");
        e5->SetVisitorTeamName("Colombia");
        e5->SetRound(domain::Round::EIGHTHS);
        e5->SetScore({3, 2});
        eighthsMatches.push_back(e5);

        // Match 6: Italy(1) vs Uruguay(0) -> Gana Italy
        auto e6 = std::make_shared<domain::Match>();
        e6->SetId("eighth-6");
        e6->SetHomeTeamId("team-e2");
        e6->SetHomeTeamName("Italy");
        e6->SetVisitorTeamId("team-f1");
        e6->SetVisitorTeamName("Uruguay");
        e6->SetRound(domain::Round::EIGHTHS);
        e6->SetScore({1, 0});
        eighthsMatches.push_back(e6);

        // Match 7: Netherlands(2) vs Serbia(1) -> Gana Netherlands
        auto e7 = std::make_shared<domain::Match>();
        e7->SetId("eighth-7");
        e7->SetHomeTeamId("team-g1");
        e7->SetHomeTeamName("Netherlands");
        e7->SetVisitorTeamId("team-h2");
        e7->SetVisitorTeamName("Serbia");
        e7->SetRound(domain::Round::EIGHTHS);
        e7->SetScore({2, 1});
        eighthsMatches.push_back(e7);

        // Match 8: Belgium(2) vs Croatia(1) -> Gana Belgium (match actual)
        eighthsMatches.push_back(match);

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::EIGHTHS)))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(testing::Return(eighthsMatches));

        std::vector<domain::Match> createdQuarters;

        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(4)
                .WillRepeatedly(testing::Invoke([&createdQuarters](const domain::Match& match) {
                    createdQuarters.push_back(match);
                    return "quarter-" + std::to_string(createdQuarters.size());
                }));

        matchDelegate->ProcessScoreUpdate(event);

        ASSERT_EQ(4, createdQuarters.size()) << "Debe crear exactamente 4 matches de cuartos";

        // Quarter 1: Ganador Eighth 1 (Chile) vs Ganador Eighth 2 (Argentina)
        EXPECT_EQ("team-a1", createdQuarters[0].HomeTeamId()) << "Quarter 1: Home debe ser ganador de Eighth 1";
        EXPECT_EQ("team-a2", createdQuarters[0].VisitorTeamId()) << "Quarter 1: Visitor debe ser ganador de Eighth 2";
        EXPECT_EQ("Chile", createdQuarters[0].HomeTeamName());
        EXPECT_EQ("Argentina", createdQuarters[0].VisitorTeamName());

        // Quarter 2: Ganador Eighth 3 (France) vs Ganador Eighth 4 (Portugal)
        EXPECT_EQ("team-d2", createdQuarters[1].HomeTeamId()) << "Quarter 2: Home debe ser ganador de Eighth 3";
        EXPECT_EQ("team-c2", createdQuarters[1].VisitorTeamId()) << "Quarter 2: Visitor debe ser ganador de Eighth 4";
        EXPECT_EQ("France", createdQuarters[1].HomeTeamName());
        EXPECT_EQ("Portugal", createdQuarters[1].VisitorTeamName());

        // Quarter 3: Ganador Eighth 5 (England) vs Ganador Eighth 6 (Italy)
        EXPECT_EQ("team-e1", createdQuarters[2].HomeTeamId()) << "Quarter 3: Home debe ser ganador de Eighth 5";
        EXPECT_EQ("team-e2", createdQuarters[2].VisitorTeamId()) << "Quarter 3: Visitor debe ser ganador de Eighth 6";
        EXPECT_EQ("England", createdQuarters[2].HomeTeamName());
        EXPECT_EQ("Italy", createdQuarters[2].VisitorTeamName());

        // Quarter 4: Ganador Eighth 7 (Netherlands) vs Ganador Eighth 8 (Belgium)
        EXPECT_EQ("team-g1", createdQuarters[3].HomeTeamId()) << "Quarter 4: Home debe ser ganador de Eighth 7";
        EXPECT_EQ("team-g2", createdQuarters[3].VisitorTeamId()) << "Quarter 4: Visitor debe ser ganador de Eighth 8";
        EXPECT_EQ("Netherlands", createdQuarters[3].HomeTeamName());
        EXPECT_EQ("Belgium", createdQuarters[3].VisitorTeamName());
    }

    // Test que valida emparejamientos correctos en semifinales
    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastQuarterMatch_CreatesSemisWithCorrectPairings) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "quarter-4";
        event.score = {3, 2};

        auto match = std::make_shared<domain::Match>();
        match->SetId("quarter-4");
        match->SetHomeTeamId("team-g1");
        match->SetHomeTeamName("Netherlands");
        match->SetVisitorTeamId("team-g2");
        match->SetVisitorTeamName("Belgium");
        match->SetRound(domain::Round::QUARTERS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        std::vector<std::shared_ptr<domain::Match>> quartersMatches;

        // Quarter 1: Chile(2) vs Argentina(1) -> Gana Chile
        auto q1 = std::make_shared<domain::Match>();
        q1->SetId("quarter-1");
        q1->SetHomeTeamId("team-a1");
        q1->SetHomeTeamName("Chile");
        q1->SetVisitorTeamId("team-a2");
        q1->SetVisitorTeamName("Argentina");
        q1->SetRound(domain::Round::QUARTERS);
        q1->SetScore({2, 1});
        quartersMatches.push_back(q1);

        // Quarter 2: France(1) vs Portugal(0) -> Gana France
        auto q2 = std::make_shared<domain::Match>();
        q2->SetId("quarter-2");
        q2->SetHomeTeamId("team-d2");
        q2->SetHomeTeamName("France");
        q2->SetVisitorTeamId("team-c2");
        q2->SetVisitorTeamName("Portugal");
        q2->SetRound(domain::Round::QUARTERS);
        q2->SetScore({1, 0});
        quartersMatches.push_back(q2);

        // Quarter 3: England(2) vs Italy(1) -> Gana England
        auto q3 = std::make_shared<domain::Match>();
        q3->SetId("quarter-3");
        q3->SetHomeTeamId("team-e1");
        q3->SetHomeTeamName("England");
        q3->SetVisitorTeamId("team-e2");
        q3->SetVisitorTeamName("Italy");
        q3->SetRound(domain::Round::QUARTERS);
        q3->SetScore({2, 1});
        quartersMatches.push_back(q3);

        // Quarter 4: Netherlands(3) vs Belgium(2) -> Gana Netherlands (match actual)
        quartersMatches.push_back(match);

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::QUARTERS)))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(testing::Return(quartersMatches));

        std::vector<domain::Match> createdSemis;

        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(2)
                .WillRepeatedly(testing::Invoke([&createdSemis](const domain::Match& match) {
                    createdSemis.push_back(match);
                    return "semi-" + std::to_string(createdSemis.size());
                }));

        matchDelegate->ProcessScoreUpdate(event);

        ASSERT_EQ(2, createdSemis.size()) << "Debe crear exactamente 2 matches de semis";

        // Semi 1: Ganador Quarter 1 (Chile) vs Ganador Quarter 2 (France)
        EXPECT_EQ("team-a1", createdSemis[0].HomeTeamId()) << "Semi 1: Home debe ser ganador de Quarter 1";
        EXPECT_EQ("team-d2", createdSemis[0].VisitorTeamId()) << "Semi 1: Visitor debe ser ganador de Quarter 2";
        EXPECT_EQ("Chile", createdSemis[0].HomeTeamName());
        EXPECT_EQ("France", createdSemis[0].VisitorTeamName());

        // Semi 2: Ganador Quarter 3 (England) vs Ganador Quarter 4 (Netherlands)
        EXPECT_EQ("team-e1", createdSemis[1].HomeTeamId()) << "Semi 2: Home debe ser ganador de Quarter 3";
        EXPECT_EQ("team-g1", createdSemis[1].VisitorTeamId()) << "Semi 2: Visitor debe ser ganador de Quarter 4";
        EXPECT_EQ("England", createdSemis[1].HomeTeamName());
        EXPECT_EQ("Netherlands", createdSemis[1].VisitorTeamName());
    }

    // Test que valida emparejamiento correcto en la final
    TEST_F(ConsumerMatchDelegateTest, ProcessScoreUpdate_LastSemiMatch_CreatesFinalWithCorrectPairing) {
        domain::ScoreUpdateEvent event;
        event.tournamentId = "tournament-1";
        event.matchId = "semi-2";
        event.score = {1, 0};

        auto match = std::make_shared<domain::Match>();
        match->SetId("semi-2");
        match->SetHomeTeamId("team-e1");
        match->SetHomeTeamName("England");
        match->SetVisitorTeamId("team-g1");
        match->SetVisitorTeamName("Netherlands");
        match->SetRound(domain::Round::SEMIS);
        match->SetScore(event.score);

        EXPECT_CALL(*matchRepoMock, FindByTournamentIdAndMatchId(testing::_, testing::_))
                .WillOnce(testing::Return(match));

        std::vector<std::shared_ptr<domain::Match>> semisMatches;

        // Semi 1: Chile(2) vs France(1) -> Gana Chile
        auto s1 = std::make_shared<domain::Match>();
        s1->SetId("semi-1");
        s1->SetHomeTeamId("team-a1");
        s1->SetHomeTeamName("Chile");
        s1->SetVisitorTeamId("team-d2");
        s1->SetVisitorTeamName("France");
        s1->SetRound(domain::Round::SEMIS);
        s1->SetScore({2, 1});
        semisMatches.push_back(s1);

        // Semi 2: England(1) vs Netherlands(0) -> Gana England (match actual)
        semisMatches.push_back(match);

        EXPECT_CALL(*matchRepoMock, FindMatchesByTournamentAndRound(
                testing::Eq(std::string_view("tournament-1")),
                testing::Eq(domain::Round::SEMIS)))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(testing::Return(semisMatches));

        std::vector<domain::Match> createdFinal;

        EXPECT_CALL(*matchRepoMock, Create(testing::_))
                .Times(1)
                .WillOnce(testing::Invoke([&createdFinal](const domain::Match& match) {
                    createdFinal.push_back(match);
                    return "final-match";
                }));

        matchDelegate->ProcessScoreUpdate(event);

        ASSERT_EQ(1, createdFinal.size()) << "Debe crear exactamente 1 match de final";

        // Final: Ganador Semi 1 (Chile) vs Ganador Semi 2 (England)
        EXPECT_EQ("team-a1", createdFinal[0].HomeTeamId()) << "Final: Home debe ser ganador de Semi 1";
        EXPECT_EQ("team-e1", createdFinal[0].VisitorTeamId()) << "Final: Visitor debe ser ganador de Semi 2";
        EXPECT_EQ("Chile", createdFinal[0].HomeTeamName());
        EXPECT_EQ("England", createdFinal[0].VisitorTeamName());
    }
}