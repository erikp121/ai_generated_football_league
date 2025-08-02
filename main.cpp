// main.cpp
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>

#include <sqlite3.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// --- Data structures ---
struct Team {
    int id;
    std::string name;
};
struct League {
    int id;
    std::string name;
    std::vector<Team> teams;
};

// --- Forward declarations ---
void InitImGui(GLFWwindow* window);
void ToggleFullscreen(GLFWwindow*& window);

bool LoadFileDBToMemory(const char* filename, sqlite3*& memory_db, std::vector<League>& leagues);
void CloseMemoryDB(sqlite3*& memory_db, std::vector<League>& leagues);
bool LoadLeaguesAndTeams(sqlite3* memory_db, std::vector<League>& leagues);
void SimulateSeasonAllLeagues(sqlite3* memory_db, std::vector<League>& leagues);
void RenderLeagueTable(sqlite3* memory_db, const League& lg);
void RenderLeagueTabs(sqlite3* memory_db, const std::vector<League>& leagues, int& current_league_tab);
void RenderSimulateWidget(sqlite3* memory_db, std::vector<League>& leagues);
void RenderSaveWidget(sqlite3* memory_db);
void RenderLoadWidget(sqlite3*& memory_db, std::vector<League>& leagues);
bool SaveMemoryDBToFile(sqlite3* memory_db, const char* filename);

// --- Implementation ---

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Football Sim", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Window creation failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ImGui + OpenGL Init
    InitImGui(window);

    // Database & app state
    sqlite3* memory_db = nullptr;
    std::vector<League> leagues;
    int current_league_tab = 0;

    if (!LoadFileDBToMemory("db.db", memory_db, leagues)) {
        fprintf(stderr, "Failed initial DB load\n");
        CloseMemoryDB(memory_db, leagues);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Key callback for fullscreen toggle ALT+ENTER
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
            // NOTE: We must cast user pointer to GLFWwindow** for toggle since window will be recreated
            GLFWwindow** pwindow = (GLFWwindow**)glfwGetWindowUserPointer(win);
            if (pwindow) {
                ToggleFullscreen(*pwindow);
            }
        }
    });

    // Store window pointer in user pointer for callback access
    glfwSetWindowUserPointer(window, &window);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderSimulateWidget(memory_db, leagues);
        RenderSaveWidget(memory_db);
        RenderLoadWidget(memory_db, leagues);

        ImGui::Begin("Leagues");
        RenderLeagueTabs(memory_db, leagues, current_league_tab);
        ImGui::End();

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    CloseMemoryDB(memory_db, leagues);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// --- Function implementations ---

void InitImGui(GLFWwindow* window) {
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void ToggleFullscreen(GLFWwindow*& window) {
    static bool is_fullscreen = false;
    static int windowed_x = 100, windowed_y = 100;
    static int windowed_width = 1280, windowed_height = 720;

    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

    if (!is_fullscreen) {
        // Save windowed position and size before going fullscreen
        glfwGetWindowPos(window, &windowed_x, &windowed_y);
        glfwGetWindowSize(window, &windowed_width, &windowed_height);
    }

    // Shutdown ImGui and OpenGL before destroying window
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Destroy old window
    glfwDestroyWindow(window);

    if (!is_fullscreen) {
        // Create fullscreen window
        window = glfwCreateWindow(mode->width, mode->height, "Football Sim", primary_monitor, nullptr);
    } else {
        // Create windowed mode window
        window = glfwCreateWindow(windowed_width, windowed_height, "Football Sim", nullptr, nullptr);
        glfwSetWindowPos(window, windowed_x, windowed_y);
    }

    if (!window) {
        fprintf(stderr, "Failed to recreate GLFW window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Re-initialize ImGui and OpenGL context
    InitImGui(window);

    // Reset key callback and user pointer after recreation
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
            GLFWwindow** pwindow = (GLFWwindow**)glfwGetWindowUserPointer(win);
            if (pwindow) {
                ToggleFullscreen(*pwindow);
            }
        }
    });
    glfwSetWindowUserPointer(window, &window);

    is_fullscreen = !is_fullscreen;
}

bool LoadFileDBToMemory(const char* filename, sqlite3*& memory_db, std::vector<League>& leagues) {
    if (memory_db) CloseMemoryDB(memory_db, leagues);

    if (sqlite3_open(":memory:", &memory_db) != SQLITE_OK) {
        fprintf(stderr, "Error opening in-memory DB: %s\n", sqlite3_errmsg(memory_db));
        return false;
    }

    sqlite3* file_db = nullptr;
    if (sqlite3_open(filename, &file_db) != SQLITE_OK) {
        fprintf(stderr, "Error opening file DB %s: %s\n", filename, sqlite3_errmsg(file_db));
        sqlite3_close(memory_db);
        memory_db = nullptr;
        return false;
    }

    sqlite3_backup* b = sqlite3_backup_init(memory_db, "main", file_db, "main");
    if (!b) {
        fprintf(stderr, "Backup init failed: %s\n", sqlite3_errmsg(memory_db));
        sqlite3_close(file_db);
        sqlite3_close(memory_db);
        memory_db = nullptr;
        return false;
    }

    if (sqlite3_backup_step(b, -1) != SQLITE_DONE) {
        fprintf(stderr, "Backup failed: %s\n", sqlite3_errmsg(memory_db));
        sqlite3_backup_finish(b);
        sqlite3_close(file_db);
        sqlite3_close(memory_db);
        memory_db = nullptr;
        return false;
    }

    sqlite3_backup_finish(b);
    sqlite3_close(file_db);

    return LoadLeaguesAndTeams(memory_db, leagues);
}

void CloseMemoryDB(sqlite3*& memory_db, std::vector<League>& leagues) {
    if (memory_db) sqlite3_close(memory_db);
    memory_db = nullptr;
    leagues.clear();
}

bool LoadLeaguesAndTeams(sqlite3* memory_db, std::vector<League>& leagues) {
    leagues.clear();

    const char* sql_leagues = "SELECT id, name FROM Leagues ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(memory_db, sql_leagues, -1, &stmt, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare Leagues select: %s\n", sqlite3_errmsg(memory_db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        League lg;
        lg.id = sqlite3_column_int(stmt, 0);
        const unsigned char* s = sqlite3_column_text(stmt, 1);
        lg.name = s ? (const char*)s : "";
        leagues.push_back(lg);
    }
    sqlite3_finalize(stmt);

    // Prepare once to reuse
    const char* sql_teams = "SELECT t.id, t.name FROM Teams t "
                           "JOIN LeagueTable lt ON t.id = lt.team_id "
                           "WHERE lt.league_id = ? ORDER BY t.id;";
    sqlite3_stmt* ts = nullptr;
    if (sqlite3_prepare_v2(memory_db, sql_teams, -1, &ts, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare Team load statement: %s\n", sqlite3_errmsg(memory_db));
        return false;
    }

    for (auto& lg : leagues) {
        lg.teams.clear();

        sqlite3_reset(ts);
        sqlite3_clear_bindings(ts);

        if (sqlite3_bind_int(ts, 1, lg.id) != SQLITE_OK) {
            fprintf(stderr, "Failed to bind league_id for league %s: %s\n", lg.name.c_str(), sqlite3_errmsg(memory_db));
            continue;
        }

        while (sqlite3_step(ts) == SQLITE_ROW) {
            Team t;
            t.id = sqlite3_column_int(ts, 0);
            const unsigned char* tn = sqlite3_column_text(ts, 1);
            t.name = tn ? (const char*)tn : "";
            lg.teams.push_back(t);
        }
    }
    sqlite3_finalize(ts);

    return true;
}

void SimulateSeasonAllLeagues(sqlite3* memory_db, std::vector<League>& leagues) {
    srand((unsigned)time(nullptr));

    char buf[512];

    sqlite3_exec(memory_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (auto& lg : leagues) {
        // Reset stats for league
        snprintf(buf, sizeof(buf),
                 "UPDATE LeagueTable SET points=0, matches_played=0, goals_scored=0, goals_conceded=0, goal_diff=0 WHERE league_id = %d;",
                 lg.id);
        sqlite3_exec(memory_db, buf, nullptr, nullptr, nullptr);

        for (size_t i = 0; i < lg.teams.size(); i++) {
            for (size_t j = 0; j < lg.teams.size(); j++) {
                if (i == j) continue;

                int home = lg.teams[i].id;
                int away = lg.teams[j].id;
                int hg = rand() % 4;
                int ag = rand() % 4;
                int hp = (hg > ag ? 3 : (hg < ag ? 0 : 1));
                int ap = (hg < ag ? 3 : (hg > ag ? 0 : 1));

                snprintf(buf, sizeof(buf),
                         "UPDATE LeagueTable SET points=points+%d, matches_played=matches_played+1, goals_scored=goals_scored+%d, goals_conceded=goals_conceded+%d WHERE team_id=%d AND league_id=%d;",
                         hp, hg, ag, home, lg.id);
                sqlite3_exec(memory_db, buf, nullptr, nullptr, nullptr);

                snprintf(buf, sizeof(buf),
                         "UPDATE LeagueTable SET points=points+%d, matches_played=matches_played+1, goals_scored=goals_scored+%d, goals_conceded=goals_conceded+%d WHERE team_id=%d AND league_id=%d;",
                         ap, ag, hg, away, lg.id);
                sqlite3_exec(memory_db, buf, nullptr, nullptr, nullptr);

                snprintf(buf, sizeof(buf),
                         "UPDATE LeagueTable SET goal_diff = goals_scored - goals_conceded WHERE team_id IN(%d,%d) AND league_id=%d;",
                         home, away, lg.id);
                sqlite3_exec(memory_db, buf, nullptr, nullptr, nullptr);
            }
        }
    }

    sqlite3_exec(memory_db, "COMMIT;", nullptr, nullptr, nullptr);

    LoadLeaguesAndTeams(memory_db, leagues);
}

void RenderLeagueTable(sqlite3* memory_db, const League& lg) {
    if (ImGui::BeginTable(lg.name.c_str(), 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Team");
        ImGui::TableSetupColumn("Pts");
        ImGui::TableSetupColumn("MP");
        ImGui::TableSetupColumn("GS");
        ImGui::TableSetupColumn("GC");
        ImGui::TableSetupColumn("GD");
        ImGui::TableHeadersRow();

        char buf[256];
        snprintf(buf, sizeof(buf),
                 "SELECT t.name, lt.points, lt.matches_played, lt.goals_scored, lt.goals_conceded, lt.goal_diff "
                 "FROM Teams t JOIN LeagueTable lt ON t.id = lt.team_id "
                 "WHERE lt.league_id=%d;",
                 lg.id);

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(memory_db, buf, -1, &stmt, nullptr) != SQLITE_OK) {
            ImGui::Text("Failed to prepare league table SQL");
            ImGui::EndTable();
            return;
        }

        struct Row {
            std::string name;
            int p, mp, gs, gc, gd;
        };

        std::vector<Row> rows;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Row r;
            r.name = (const char*)sqlite3_column_text(stmt, 0);
            r.p = sqlite3_column_int(stmt, 1);
            r.mp = sqlite3_column_int(stmt, 2);
            r.gs = sqlite3_column_int(stmt, 3);
            r.gc = sqlite3_column_int(stmt, 4);
            r.gd = sqlite3_column_int(stmt, 5);
            rows.push_back(r);
        }

        sqlite3_finalize(stmt);

        std::sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.p > b.p; });

        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(r.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", r.p);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", r.mp);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", r.gs);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", r.gc);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", r.gd);
        }

        ImGui::EndTable();
    }
}

void RenderLeagueTabs(sqlite3* memory_db, const std::vector<League>& leagues, int& current_league_tab) {
    if (ImGui::BeginTabBar("LeaguesTabBar")) {
        for (size_t i = 0; i < leagues.size(); i++) {
            const League& lg = leagues[i];
            if (ImGui::BeginTabItem(lg.name.c_str())) {
                current_league_tab = (int)i;
                RenderLeagueTable(memory_db, lg);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void RenderSimulateWidget(sqlite3* memory_db, std::vector<League>& leagues) {
    ImGui::Begin("Simulation");
    if (ImGui::Button("Simulate Season for All Leagues")) {
        SimulateSeasonAllLeagues(memory_db, leagues);
    }
    ImGui::End();
}

void RenderSaveWidget(sqlite3* memory_db) {
    ImGui::Begin("Save Database");
    static char save_filename[256] = "db_saved.db";
    ImGui::InputText("Filename", save_filename, sizeof(save_filename));
    if (ImGui::Button("Save")) {
        if (SaveMemoryDBToFile(memory_db, save_filename)) {
            ImGui::Text("Saved successfully!");
        } else {
            ImGui::Text("Save failed!");
        }
    }
    ImGui::End();
}

void RenderLoadWidget(sqlite3*& memory_db, std::vector<League>& leagues) {
    ImGui::Begin("Load Database");
    static char load_filename[256] = "db.db";
    ImGui::InputText("Filename", load_filename, sizeof(load_filename));
    if (ImGui::Button("Load")) {
        if (LoadFileDBToMemory(load_filename, memory_db, leagues)) {
            ImGui::Text("Loaded successfully!");
        } else {
            ImGui::Text("Load failed!");
        }
    }
    ImGui::End();
}

bool SaveMemoryDBToFile(sqlite3* memory_db, const char* filename) {
    sqlite3* file_db = nullptr;
    if (sqlite3_open(filename, &file_db) != SQLITE_OK) {
        fprintf(stderr, "Failed to open file DB for saving: %s\n", sqlite3_errmsg(file_db));
        if (file_db) sqlite3_close(file_db);
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(file_db, "main", memory_db, "main");
    if (!backup) {
        fprintf(stderr, "Backup init failed: %s\n", sqlite3_errmsg(file_db));
        sqlite3_close(file_db);
        return false;
    }

    if (sqlite3_backup_step(backup, -1) != SQLITE_DONE) {
        fprintf(stderr, "Backup step failed: %s\n", sqlite3_errmsg(file_db));
        sqlite3_backup_finish(backup);
        sqlite3_close(file_db);
        return false;
    }

    sqlite3_backup_finish(backup);
    sqlite3_close(file_db);

    return true;
}
