#include <fstream>
#include <iostream>
#include <string>
#include <charconv>
#include <cstdio>
#include <err.h>
#include <cassert>
#include "config.hpp"

#define ENABLE_UNDO "allow_undo"
#define NUM_CONS_UNDOS_ALLOW "number_of_consecutive_undos_without_counting_as_undo_used"
#define CONSIDER_UNDO_WINS "consider_undo_used_wins_in_statistic"
#define CLOSE_IS_LOSS "closing_running_game_counts_as_loss"
#define REAL_MOVES "count_real_moves"

const char *whitespace = "\t\r\n ";

void trim(std::string_view &s, const char *t = whitespace) {
    size_t from = s.find_first_not_of(t);
    size_t to = s.find_last_not_of(t);
    s = s.substr(from, to + 1);
}

bool parseBool(std::string_view str) {
    if (str == "true") {
        return true;
    }
    if (str == "false") {
        return false;
    }
    throw std::runtime_error("invalid bool");
}

char parseNumber(std::string_view str) {
    int num;
    auto res = std::from_chars(str.begin(), str.end(), num);
    if (res.ec == std::errc::invalid_argument
            || num < 0 || num > 255)
    {
        throw std::runtime_error("invalid number");
    }
    return num;
}

bool onlyWhitespace(std::string_view str, const char *ws=whitespace) {
    return str.find_first_not_of(ws) == (size_t)-1;
}

void Config::parse(const char *filename) {
    std::ifstream file(filename);
    std::string line_str;
    bool create = true;
    while (std::getline(file, line_str)) {
        std::string_view line(line_str);
        if (onlyWhitespace(line)) continue;
        create = false;
        size_t x = line.find_first_of('=');
        if (x == (size_t)-1) {
            throw std::runtime_error("missing '='");
        }
        std::string_view lhs = line.substr(0, x);
        std::string_view rhs = line.substr(x + 1);
        trim(lhs);
        trim(rhs);
        if (lhs == ENABLE_UNDO) {
            enable_undo = parseBool(rhs);
        } else if (lhs == NUM_CONS_UNDOS_ALLOW) {
            num_cons_undos_allow = parseNumber(rhs);
        } else if (lhs == CONSIDER_UNDO_WINS) {
            consider_undo_wins = parseBool(rhs);
        } else if (lhs == CLOSE_IS_LOSS) {
            close_is_loss = parseBool(rhs);
        } else if (lhs == REAL_MOVES) {
            real_moves = parseBool(rhs);
        } else {
            throw std::runtime_error("invalid setting");
        }
    }
    if (create) {
        std::ofstream out(filename);
        out << ENABLE_UNDO " = true\n"
               NUM_CONS_UNDOS_ALLOW " = 1\n"
               CONSIDER_UNDO_WINS " = false\n"
               CLOSE_IS_LOSS " = false\n"
               REAL_MOVES " = true\n";
    }
}

void Statistic::load(const char *filename, const Config &config) {
    FILE *f = std::fopen(filename, "r");
    if (!f) return;
    Entry entry;
    int i = 0;
    while (std::fread(&entry, sizeof(entry), 1, f) == 1) {
        entries.push_back(entry);
        using enum Entry::Type;
        if (entry.t == Move && entry.matchesMove(config)) {
            assert(moves_idx == -1);
            moves_avg = entry.moves_avg;
            moves_idx = i;
        } else if (entry.t == WinLoss && entry.matchesWinLoss(config)) {
            assert(winloss_idx == -1);
            wins = entry.wins;
            losses = entry.losses;
            winloss_idx = i;
        }
        ++i;
    }
    std::fclose(f);
}

void Statistic::save(const char *filename, const Config &config) {
    FILE *f = std::fopen(filename, "w");
    if (!f) {
        err(1, "fopen %s", filename);
    }
    if (moves_idx == -1) {
        entries.emplace_back(
                Entry::Type::Move,
                config.real_moves,
                moves_avg);
    } else {
        entries[moves_idx].moves_avg = moves_avg;
    }
    if (winloss_idx == -1) {
        entries.emplace_back(
                Entry::Type::WinLoss,
                config.enable_undo,
                config.num_cons_undos_allow,
                config.consider_undo_wins,
                config.close_is_loss,
                wins,
                losses);
    } else {
        Entry &entry = entries[winloss_idx];
        entry.wins = wins;
        entry.losses = losses;
    }
    size_t num_entries = entries.size();
    if (fwrite(entries.data(), sizeof(Entry), num_entries, f) != num_entries) {
        err(1, "fwrite %s", filename);
    }
    std::fclose(f);
}

void Statistic::recordWin(unsigned moves) {
        assert(moves > 0);
        moves_avg = ((moves_avg * wins) + moves) / (wins + 1);
        ++wins;
}

void testConfig() {
    Config config;
    try {
        config.parse(CONFIG_FILE);
    } catch (std::exception &e) {
        printf("Error parsing config: %s\n", e.what());
    }
    Statistic stats;
    stats.load(STATS_FILE, config);
    stats.save(STATS_FILE, config);
    printf("%u %u %u %u %u",
            config.enable_undo,
            config.num_cons_undos_allow,
            config.consider_undo_wins,
            config.close_is_loss,
            config.real_moves);
}

/*
int main() {
    testConfig();
    return 0;
}
*/
