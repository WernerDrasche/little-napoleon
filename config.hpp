#include <cstdint>
#include <vector>

#define CONFIG_FILE "config.txt"
#define STATS_FILE "stats.sav"

struct Config {
    bool enable_undo = true;
    unsigned num_cons_undos_allow = 1;
    bool consider_undo_wins = false;
    bool close_is_loss = true;
    bool real_moves = true;

    void parse(const char *filename);
};

class Statistic {
    struct Entry {
        enum class Type { Move, WinLoss };
        Type t;
        union {
            struct {
                bool enable_undo;
                unsigned char num_cons_undos_allow;
                bool consider_undo_wins;
                bool close_is_loss;

                uint32_t wins;
                uint32_t losses;
            };
            struct {
                bool real_moves;

                float moves_avg;
            };
        };

        Entry() = default;

        Entry(Type t, bool real_moves, float moves_avg)
            : t(t)
            , real_moves(real_moves)
            , moves_avg(moves_avg) {}

        Entry(Type t,
                bool enable_undo,
                char num_cons_undos_allow,
                bool consider_undo_wins,
                bool close_is_loss,
                uint32_t wins,
                uint32_t losses)
            : t(t)
            , enable_undo(enable_undo)
            , num_cons_undos_allow(num_cons_undos_allow)
            , consider_undo_wins(consider_undo_wins)
            , close_is_loss(close_is_loss)
            , wins(wins)
            , losses(losses) {}

        constexpr bool matchesMove(const Config &config) {
            return config.real_moves == real_moves;
        }

        constexpr bool matchesWinLoss(const Config &config) {
            return config.enable_undo == enable_undo
                && config.num_cons_undos_allow == num_cons_undos_allow
                && config.consider_undo_wins == consider_undo_wins
                && config.close_is_loss == close_is_loss;
        }
    };

    std::vector<Entry> entries;
    int winloss_idx = -1;
    int moves_idx = -1;

public:
    uint32_t wins = 0;
    uint32_t losses = 0;
    float moves_avg = 0;

    void load(const char *filename, const Config &config);
    void save(const char *filename, const Config &config);
    void recordWin(unsigned moves);
    void recordLoss() { ++losses; }
};
