-- 1. Create tables
CREATE TABLE IF NOT EXISTS Teams (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Leagues (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS LeagueTable (
    id INTEGER PRIMARY KEY,
    league_id INTEGER NOT NULL,
    team_id INTEGER NOT NULL,
    points INTEGER NOT NULL DEFAULT 0,
    matches_played INTEGER NOT NULL DEFAULT 0,
    goals_scored INTEGER NOT NULL DEFAULT 0,
    goals_conceded INTEGER NOT NULL DEFAULT 0,
    goal_diff INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (league_id) REFERENCES Leagues(id),
    FOREIGN KEY (team_id) REFERENCES Teams(id),
    UNIQUE(league_id, team_id)
);

-- 2. Begin transaction to speed up bulk inserts
BEGIN TRANSACTION;

-- 3. Insert 256 leagues: League1 to League256
WITH RECURSIVE league_gen(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM league_gen WHERE id < 256
)
INSERT OR IGNORE INTO Leagues (id, name)
SELECT id, 'League' || id FROM league_gen;

-- 4. Insert 4096 teams: Team1 to Team4096
WITH RECURSIVE team_gen(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM team_gen WHERE id < 4096
)
INSERT OR IGNORE INTO Teams (id, name)
SELECT id, 'Team ' || id FROM team_gen;

-- 5. Insert into LeagueTable: 16 teams per league
WITH RECURSIVE league_team_pairs(league_id, team_id) AS (
    SELECT 1 AS league_id, 1 AS team_id
    UNION ALL
    SELECT
        CASE WHEN (team_id % 16) = 0 THEN league_id + 1 ELSE league_id END,
        team_id + 1
    FROM league_team_pairs
    WHERE team_id < 4096
)
INSERT OR IGNORE INTO LeagueTable (
    league_id, team_id, points, matches_played, goals_scored, goals_conceded, goal_diff
)
SELECT
    league_id, team_id, 0, 0, 0, 0, 0
FROM league_team_pairs;

-- 6. Commit transaction
COMMIT;

-- 7. Create performance-enhancing indexes

-- Speed up finding all teams in a given league
CREATE INDEX IF NOT EXISTS idx_league_table_league_id ON LeagueTable(league_id);

-- Speed up finding which league a team belongs to
CREATE INDEX IF NOT EXISTS idx_league_table_team_id ON LeagueTable(team_id);

-- Speed up joins involving foreign key lookups (optional but useful for JOIN-heavy queries)
CREATE INDEX IF NOT EXISTS idx_teams_id ON Teams(id);
CREATE INDEX IF NOT EXISTS idx_leagues_id ON Leagues(id);
