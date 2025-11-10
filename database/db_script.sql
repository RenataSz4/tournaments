-- podman run -d --replace --name=tournament_db --network development -e POSTGRES_PASSWORD=password -p 5432:5432 postgres:17.6-alpine3.22
-- podman exec -i tournament_db psql -U postgres -d postgres < db_script.sql

CREATE USER tournament_svc WITH PASSWORD 'password';
CREATE USER tournament_admin WITH PASSWORD 'password';

CREATE DATABASE tournament_db;

\connect tournament_db

grant all privileges on database tournament_db to tournament_admin;
grant all privileges on database tournament_db to tournament_svc;
grant usage on schema public to tournament_admin;
grant usage on schema public to tournament_svc;

GRANT SELECT ON ALL TABLES IN SCHEMA public TO tournament_admin;
GRANT DELETE ON ALL TABLES IN SCHEMA public TO tournament_admin;
GRANT UPDATE ON ALL TABLES IN SCHEMA public TO tournament_admin;
GRANT INSERT ON ALL TABLES IN SCHEMA public TO tournament_admin;
GRANT CREATE ON SCHEMA public TO tournament_admin;

\connect tournament_db tournament_admin

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE TABLE TEAMS (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    document JSONB NOT NULL,
    last_update_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX team_unique_name_idx ON teams ((document->>'name'));

CREATE TABLE TOURNAMENTS (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    document JSONB NOT NULL,
    last_update_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX tournament_unique_name_idx ON TOURNAMENTS ((document->>'name'));

CREATE TABLE GROUPS (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    TOURNAMENT_ID UUID not null references TOURNAMENTS(ID),
    document JSONB NOT NULL,
    last_update_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE UNIQUE INDEX tournament_group_unique_name_idx ON GROUPS (tournament_id,(document->>'name'));

CREATE TABLE MATCHES (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    document JSONB NOT NULL,
    last_update_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

GRANT SELECT ON ALL TABLES IN SCHEMA public TO tournament_svc;
GRANT DELETE ON ALL TABLES IN SCHEMA public TO tournament_svc;
GRANT UPDATE ON ALL TABLES IN SCHEMA public TO tournament_svc;
GRANT INSERT ON ALL TABLES IN SCHEMA public TO tournament_svc;

-- Enhancements to MATCHES table
-- Create ENUM types for match rounds and status
CREATE TYPE match_round AS ENUM ('regular', 'quarterfinals', 'semifinals', 'final');
CREATE TYPE match_status AS ENUM ('pending', 'played');

-- Add columns to existing MATCHES table
ALTER TABLE MATCHES ADD COLUMN tournament_id UUID NOT NULL REFERENCES TOURNAMENTS(id);
ALTER TABLE MATCHES ADD COLUMN home_team_id UUID NOT NULL REFERENCES TEAMS(id);
ALTER TABLE MATCHES ADD COLUMN visitor_team_id UUID NOT NULL REFERENCES TEAMS(id);
ALTER TABLE MATCHES ADD COLUMN home_score INTEGER;
ALTER TABLE MATCHES ADD COLUMN visitor_score INTEGER;
ALTER TABLE MATCHES ADD COLUMN round match_round NOT NULL DEFAULT 'regular';
ALTER TABLE MATCHES ADD COLUMN winner_team_id UUID REFERENCES TEAMS(id);
ALTER TABLE MATCHES ADD COLUMN match_status match_status NOT NULL DEFAULT 'pending';

-- Add constraints to ensure data integrity
ALTER TABLE MATCHES ADD CONSTRAINT matches_different_teams_check
    CHECK (home_team_id <> visitor_team_id);

ALTER TABLE MATCHES ADD CONSTRAINT matches_winner_is_participant_check
    CHECK (winner_team_id IS NULL OR winner_team_id IN (home_team_id, visitor_team_id));

ALTER TABLE MATCHES ADD CONSTRAINT matches_scores_consistency_check
    CHECK (
        (home_score IS NULL AND visitor_score IS NULL) OR
        (home_score IS NOT NULL AND visitor_score IS NOT NULL)
    );

-- Add indexes for common queries
CREATE INDEX matches_tournament_id_idx ON MATCHES(tournament_id);
CREATE INDEX matches_home_team_id_idx ON MATCHES(home_team_id);
CREATE INDEX matches_visitor_team_id_idx ON MATCHES(visitor_team_id);
CREATE INDEX matches_status_idx ON MATCHES(match_status);
CREATE INDEX matches_round_idx ON MATCHES(round);
