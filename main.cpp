#include <SFML/Graphics.hpp>
#include <algorithm>
#include <random>
#include <chrono>
#include <list>
#include "config.hpp"

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 800
#define FPS           60
#define OUTLINE_WIDTH 2
#define CARD_SCALE    0.4
#define CARD_MARGIN   10
#define ROW_MARGIN    20
#define FONT_SIZE     25

#define CARD_WIDTH     222
#define CARD_HEIGHT    323
#define CARDS_PER_SUIT 13
#define NUM_VACANT 11
#define NSEC_PER_SEC 1000000000

using iterator = std::vector<char>::iterator;
using chrono_clock = std::chrono::steady_clock;
using chrono_nsec = std::chrono::nanoseconds;
using chrono_time_point = std::chrono::time_point<chrono_clock, chrono_nsec>;

const char NUM_CARDS = 4 * CARDS_PER_SUIT;
const float CARD_WS = CARD_WIDTH * CARD_SCALE + 2 * OUTLINE_WIDTH;
const float CARD_HS = CARD_HEIGHT * CARD_SCALE + 2 * OUTLINE_WIDTH;
const float CARD_WR = CARD_WS * 0.25;
const float START_Y = (WINDOW_HEIGHT - CARD_HS - 4 * (CARD_HS + ROW_MARGIN)) / 2;

const sf::Color COLOR_BG = {40, 150, 80};
const sf::Color COLOR_CARD = sf::Color::White;
const sf::Color COLOR_OUTLINE = sf::Color::Black;
const sf::Color COLOR_SELECT = sf::Color::Blue;

char strbuf[1024];

enum Suit {
    Clubs,
    Hearts,
    Spades,
    Diamonds,
};

enum FaceCard {
    King,
    Ace,
    Jack = 11,
    Queen,
};

struct Card : public sf::Drawable {
    sf::Sprite sprite;
    char id;
    bool selected;
    bool hovered;

    Card(const sf::Texture &texture, char id) 
        : sprite(texture)
        , id(id)
        , selected(false)
        , hovered(false)
    {}

    constexpr bool isVacant() const {
        return id >= NUM_CARDS;
    }

    constexpr bool fits(const Card &other) {
        return id / CARDS_PER_SUIT == other.id / CARDS_PER_SUIT
            && ((id + 1) % CARDS_PER_SUIT == other.id % CARDS_PER_SUIT
                    || (other.id + 1) % CARDS_PER_SUIT == id % CARDS_PER_SUIT);

    }

    sf::Vector2f update(sf::Vector2i delta) {
        sf::Vector2f pos = sprite.getPosition();
        pos.x += delta.x;
        pos.y += delta.y;
        sprite.setPosition(pos);
        return pos;
    }

    virtual void draw(sf::RenderTarget &target, sf::RenderStates) const override {
        if (selected || hovered) {
            sf::FloatRect bounds = sprite.getGlobalBounds();
            sf::RectangleShape rect(bounds.size);
            rect.setFillColor(sf::Color::Transparent);
            rect.setPosition(bounds.position);
            rect.setOutlineColor(COLOR_SELECT);
            rect.setOutlineThickness(selected ? OUTLINE_WIDTH : OUTLINE_WIDTH / 2);
            target.draw(rect);
        }
        if (!isVacant()) {
            target.draw(sprite);
        }
    }
};

struct Row {
    char idx;
};

struct Extra {
    char idx;
};

struct Cellar {};

struct Pile {
    char idx;
};

using Place = std::variant<Row, Extra, Cellar, Pile>;

bool same(Place a, Place b) {
    size_t i = a.index();
    if (b.index() != i) {
        return false;
    }
    switch (i) {
    case 0:
        return std::get<Row>(a).idx == std::get<Row>(b).idx;
        break;
    case 1:
        return std::get<Extra>(a).idx == std::get<Extra>(b).idx;
        break;
    case 2:
        return true;
        break;
    case 3:
        return std::get<Pile>(a).idx == std::get<Pile>(b).idx;
        break;
    default:
        assert(false);
        break;
    }
}

std::array<sf::RenderTexture, NUM_CARDS + 1> card_textures;
std::vector<Card> cards;
sf::Font font("assets/font/joystix_mono.otf");
Config config;
Statistic overall_stats;

struct Range {
    iterator begin;
    iterator end;
    Place place;

    constexpr bool isRun() const {
        if (size() <= 1) return true;
        for (iterator it = begin; it != end - 1; ++it) {
            if (!cards[*it].fits(cards[it[1]])) return false;
        }
        return true;
    }

    constexpr bool isLeftToRight() const {
        return (std::holds_alternative<Row>(place) && std::get<Row>(place).idx >= 4)
            || (std::holds_alternative<Extra>(place) && std::get<Extra>(place).idx == 1);
    }

    constexpr unsigned size() const {
        return end - begin;
    }
};

struct Duration {
    unsigned mins;
    unsigned secs;
};

class Game : public sf::Drawable {
    struct Move {
        Place from;
        unsigned size;
        Place to;
        bool reversed;
    };

    class Stats {
        friend Game;

        sf::Text text_left{font, "", FONT_SIZE};
        sf::Text text_right{font, "", FONT_SIZE};
        sf::Text reshuffle_info{font, "Press Ctrl+S to shuffle cards", FONT_SIZE};
        unsigned consecutive_undos = 0;
        bool won = false;
        chrono_time_point start = std::chrono::steady_clock::now();
    public:
        unsigned real_moves = 0;
        unsigned moves = 0;
        bool used_undo = false;

        unsigned getMoves() const {
            return config.real_moves ? real_moves : moves;
        }

        Duration timeElapsed() const {
            chrono_time_point end = std::chrono::steady_clock::now();
            auto duration = end - start;
            long long nsecs = duration.count();
            unsigned secs = nsecs / NSEC_PER_SEC;
            unsigned mins = secs / 60;
            secs %= 60;
            return {mins, secs};
        }

        void registerMove(Move move) {
            ++moves;
            real_moves += move.size > 1 && !move.reversed ? 2 * move.size : move.size;
            consecutive_undos = 0;
        }

        void registerUndo(Move move) {
            --moves;
            if (++consecutive_undos > config.num_cons_undos_allow) {
                used_undo = true;
            }
            real_moves -= move.size > 1 && !move.reversed ? 2 * move.size : move.size;
        }

        ///call only once
        void registerWin() {
            assert(!won);
            won = true;
            if (!used_undo || config.consider_undo_wins) {
                overall_stats.recordWin(getMoves());
            }
            Duration dur = timeElapsed();
            snprintf(strbuf, sizeof(strbuf), "Time  %u:%u\n"
                                             "Moves %u\n"
                                             "Undo  %s",
                    dur.mins, dur.secs, getMoves(), used_undo ? "Yes" : "No");
            text_left = sf::Text(font, strbuf, FONT_SIZE);
            sf::Vector2f text_size = text_left.getGlobalBounds().size;
            text_left.setPosition({(WINDOW_WIDTH / 2.f - text_size.x) / 2, (WINDOW_HEIGHT / 2.f - text_size.y) / 2});
            text_size = reshuffle_info.getGlobalBounds().size;
            reshuffle_info.setPosition({(WINDOW_WIDTH - text_size.x) / 2, 0});
            unsigned games = overall_stats.wins + overall_stats.losses;
            unsigned winrate = ((double)overall_stats.wins / games) * 100;
            snprintf(strbuf, sizeof(strbuf), "Games     %u\n"
                                             "Wins      %u\n"
                                             "Winrate   %u%%\n"
                                             "Avg.Moves %u",
                    games, overall_stats.wins, winrate, (unsigned)overall_stats.moves_avg);
            text_right = sf::Text(font, strbuf, FONT_SIZE);
            text_size = text_right.getGlobalBounds().size;
            text_right.setPosition({WINDOW_WIDTH * 0.75f - text_size.x / 2, (WINDOW_HEIGHT / 2.f - text_size.y) / 2});
        }
    };

    std::vector<Move> history;
    std::array<std::vector<char>, 4> piles;
    std::array<std::vector<char>, 8> rows;
    std::array<std::vector<char>, 2> extra;
    std::vector<char> cellar;

public:
    Stats stats;

    static void setPilePositions(Range range, sf::Vector2f pos)
    {
        bool stacked = std::holds_alternative<Row>(range.place);
        bool left_to_right = range.isLeftToRight();
        if (std::holds_alternative<Pile>(range.place)) {
            cards[*(range.end - 1)].sprite.setPosition(pos);
        } else {
            for (iterator it = range.begin; it != range.end; ++it) {
                Card &card = cards[*it];
                card.sprite.setPosition(pos);
                if (card.isVacant()) continue;
                float dx = stacked ? CARD_WR : CARD_WS + CARD_MARGIN;
                pos.x += left_to_right ? dx : -dx;
            }
        }
    }

    static void initPile(
            std::vector<char> &vec,
            std::list<char> &shuffled,
            unsigned n,
            char *next_vacant)
    {
        assert(vec.size() == 0);
        vec.reserve(n + 1);
        if (next_vacant) {
            vec.push_back((*next_vacant)++);
        }
        for (unsigned i = 0; i < n; ++i) {
            char c = shuffled.back();
            shuffled.pop_back();
            vec.push_back(c);
        }
    }

    Game() {
        char next_vacant = NUM_CARDS;
        std::array<char, NUM_CARDS> tmp;
        for (unsigned i = 0; i < NUM_CARDS; ++i) {
            tmp[i] = i;
        }
        static std::random_device rd;
        static std::mt19937 g(rd());
        //static std::mt19937 g(0);
        std::shuffle(tmp.begin(), tmp.end(), g);
        std::list<char> shuffled;
        for (unsigned i = 0; i < NUM_CARDS; ++i) {
            shuffled.push_back(tmp[i]);
        }
        sf::Vector2f pos = {
            (WINDOW_WIDTH - CARD_WS) / 2,
            START_Y,
        };
        char c = shuffled.back() % CARDS_PER_SUIT;
        for (unsigned i = 0; i < 4; ++i) {
            shuffled.remove(c);
            cards[c].sprite.setPosition(pos);
            piles[i].reserve(CARDS_PER_SUIT);
            piles[i].push_back(c);
            c += CARDS_PER_SUIT;
            pos.y += CARD_HS + ROW_MARGIN;
        }
        pos.y = START_Y;
        pos.x -= CARD_MARGIN + CARD_WS;
        for (unsigned i = 0; i < 4; ++i) {
            initPile(rows[i], shuffled, 5, &next_vacant);
            setPilePositions({rows[i].begin(), rows[i].end(), Row(i)}, pos);
            pos.y += CARD_HS + ROW_MARGIN;
        }
        initPile(extra[0], shuffled, 4, &next_vacant);
        setPilePositions({extra[0].begin(), extra[0].end(), Extra(0)}, pos);
        pos.y = START_Y;
        pos.x += (CARD_WS + CARD_MARGIN) * 2;
        for (unsigned i = 4; i < 8; ++i) {
            initPile(rows[i], shuffled, 5, &next_vacant);
            setPilePositions({rows[i].begin(), rows[i].end(), Row(i)}, pos);
            pos.y += CARD_HS + ROW_MARGIN;
        }
        initPile(extra[1], shuffled, 4, &next_vacant);
        setPilePositions({extra[1].begin(), extra[1].end(), Extra(1)}, pos);
        pos.x -= CARD_MARGIN + CARD_WS;
        cards[next_vacant].sprite.setPosition(pos);
        cellar.reserve(2);
        cellar.push_back(next_vacant);
    }

    constexpr bool won() const {
        return stats.won;
    }

    char numVacantRows() const {
        char count = 0;
        for (const auto &row : rows) {
            if (row.size() == 1) ++count;
        }
        return count;
    }

    char numVacantExtra() const {
        char count = 0;
        for (const auto &row : extra) {
            if (row.size() == 1) ++count;
        }
        return count;
    }

    std::optional<Range> select(sf::Vector2i mouse_pos) {
        sf::Vector2f pos = {(float)mouse_pos.x, (float)mouse_pos.y};
        for (unsigned i = 8; i-- > 0;) {
            std::vector<char> &row = rows[i];
            for (iterator it = row.end(); it-- != row.begin();) {
                Card &card = cards[*it];
                if (card.selected) continue;
                if (card.sprite.getGlobalBounds().contains(pos)) {
                    Range range(it, row.end(), Row(i));
                    if (range.size() == 1 || (numVacantRows() && range.isRun())) {
                        return range;
                    }
                    return {};
                }
            }
        }
        for (unsigned i = 2; i-- > 0;) {
            std::vector<char> &row = extra[i];
            for (iterator it = row.end(); it-- != row.begin();) {
                Card &card = cards[*it];
                if (card.selected || card.isVacant()) continue;
                if (card.sprite.getGlobalBounds().contains(pos)) {
                    return it == row.end() - 1 ? Range(it, row.end(), Extra(i)) : std::optional<Range>();
                }
            }
        }
        for (iterator it = cellar.end(); it-- != cellar.begin();) {
            Card &card = cards[*it];
            if (card.selected) continue;
            if (card.sprite.getGlobalBounds().contains(pos)) {
                assert(cellar.size() <= 2);
                return Range(it, cellar.end(), Cellar());
            }
        }
        for (unsigned i = 0; i < 4; ++i) {
            Card &card = cards[piles[i].back()];
            if (card.sprite.getGlobalBounds().contains(pos)) {
                return Range(piles[i].end() - 1, piles[i].end(), Pile(i));
            }
        }
        return {};
    }

    constexpr std::vector<char> &getPlace(Place place) {
        if (std::holds_alternative<Row>(place)) {
            return rows[std::get<Row>(place).idx];
        } else if (std::holds_alternative<Extra>(place)) {
            return extra[std::get<Extra>(place).idx];
        } else if (std::holds_alternative<Pile>(place)) {
            return piles[std::get<Pile>(place).idx];
        } else {
            return cellar;
        }
    }

    bool tryMove(const Range &from, const Range &to, bool reversed) {
        bool result = false;
        Card &last = cards[*(to.end - 1)];
        Card &first = cards[*from.begin];
        std::vector<char> &row_from = getPlace(from.place);
        std::vector<char> &row_to = getPlace(to.place);
        unsigned size_from = from.size();
        first.selected = first.hovered = false;
        if ((std::holds_alternative<Cellar>(to.place) 
                && last.isVacant()
                && size_from == 1
                && !std::holds_alternative<Extra>(from.place))
            || ((std::holds_alternative<Row>(to.place) || std::holds_alternative<Pile>(to.place))
                && last.fits(first))
            || (std::holds_alternative<Row>(to.place)
                && last.isVacant()
                && (size_from == 1 || numVacantRows() >= 2 || reversed)))
        {
            Move move(from.place, size_from, to.place, reversed);
            history.push_back(move);
            stats.registerMove(move);
            result = true;
            last.selected = last.hovered = false;
            Card &tmp = cards[*to.begin];
            tmp.selected = tmp.hovered = false;
            for (iterator it = from.begin; it != from.end; ++it) {
                row_to.push_back(*it);
            }
            for (unsigned i = 0; i < size_from; ++i) {
                row_from.pop_back();
            }
        }
        if (result) {
            Game::setPilePositions(
                    {row_to.begin(), row_to.end(), to.place},
                    cards[*row_to.begin()].sprite.getPosition());

            if (numVacantRows() == 8 
                    && numVacantExtra() == 2 
                    && cellar.size() == 1)
            {
                stats.registerWin();
            }
        } else {
            Game::setPilePositions(
                    {row_from.begin(), row_from.end(), from.place},
                    cards[*row_from.begin()].sprite.getPosition());
        }
        return result;
    }

    ///don't call while dragging
    bool undo() {
        if (history.size() == 0) return false;
        //inefficient, but I don't care anymore
        for (Card &card : cards) {
            card.hovered = card.selected = false;
        }
        Move move = history.back();
        history.pop_back();
        stats.registerUndo(move);
        std::vector<char> &row_from = getPlace(move.from);
        std::vector<char> &row_to = getPlace(move.to);
        if (move.reversed) {
            for (iterator it = row_to.end(); it-- != row_to.end() - move.size;) {
                row_from.push_back(*it);
            }
        } else {
            for (iterator it = row_to.end() - move.size; it != row_to.end(); ++it) {
                row_from.push_back(*it);
            }
        }
        for (unsigned i = 0; i < move.size; ++i) {
            row_to.pop_back();
        }
        setPilePositions(
                {row_from.begin(), row_from.end(), move.from},
                cards[row_from.front()].sprite.getPosition());
        return true;
    }

    virtual void draw(sf::RenderTarget &target, sf::RenderStates) const override {
        for (auto &row : rows) {
            for (char c : row) {
                target.draw(cards[c]);
            }
        }
        for (auto &row : extra) {
            for (char c : row) {
                target.draw(cards[c]);
            }
        }
        for (char c : cellar) {
            target.draw(cards[c]);
        }
        target.draw(cards[piles[0].back()]);
        target.draw(cards[piles[1].back()]);
        target.draw(cards[piles[2].back()]);
        target.draw(cards[piles[3].back()]);
        if (won()) {
            target.draw(stats.text_left);
            target.draw(stats.text_right);
            target.draw(stats.reshuffle_info);
        } else {
            Duration dur = stats.timeElapsed();
            snprintf(strbuf, sizeof(strbuf), "%u:%u", dur.mins, dur.secs);
            sf::Text text(font, strbuf, FONT_SIZE);
            target.draw(text);
        }
    }
};

void loadCards() {
    cards.reserve(NUM_CARDS + NUM_VACANT);
    sf::Vector2u texture_size = {(unsigned)CARD_WS + 2 * OUTLINE_WIDTH, (unsigned)CARD_HS + 2 * OUTLINE_WIDTH};
    const char *suit_str, *rank_str;
    for (unsigned i = 0; i < NUM_CARDS; ++i) {
        char numeric[2] = {};
        Suit suit = (Suit)(i / CARDS_PER_SUIT);
        unsigned rank = i % CARDS_PER_SUIT;
        switch (rank) {
        case Jack:
            rank_str = "jack";
            break;
        case Queen:
            rank_str = "queen";
            break;
        case King:
            rank_str = "king";
            break;
        case Ace:
            rank_str = "ace";
            break;
        case 10:
            rank_str = "10";
            break;
        default:
            numeric[0] = '0' + rank;
            rank_str = numeric;
            break;
        }
        switch (suit) {
            case Clubs:
                suit_str = "clubs";
                break;
            case Hearts:
                suit_str = "hearts";
                break;
            case Spades:
                suit_str = "spades";
                break;
            case Diamonds:
                suit_str = "diamonds";
                break;
        }
        std::sprintf(strbuf, "assets/%s_of_%s.png", rank_str, suit_str);
        sf::RenderTexture texture(texture_size);
        texture.clear(COLOR_CARD);
        sf::RectangleShape rect({CARD_WS, CARD_HS});
        rect.setPosition({OUTLINE_WIDTH, OUTLINE_WIDTH});
        rect.setOutlineColor(COLOR_OUTLINE);
        rect.setOutlineThickness(OUTLINE_WIDTH);
        texture.draw(rect);
        sf::Texture tmp(strbuf);
        sf::Sprite sprite(tmp);
        sprite.setPosition({OUTLINE_WIDTH, OUTLINE_WIDTH});
        sprite.scale({CARD_SCALE, CARD_SCALE});
        texture.draw(sprite);
        texture.display();
        card_textures[i] = std::move(texture);
        cards.emplace_back(card_textures[i].getTexture(), i);
    }
    sf::RenderTexture texture(texture_size);
    texture.clear(sf::Color::Transparent);
    texture.display();
    card_textures[NUM_CARDS] = std::move(texture);
    for (unsigned i = 0; i < NUM_VACANT; ++i) {
        cards.emplace_back(card_textures[NUM_CARDS].getTexture(), NUM_CARDS + i);
    }
    assert(cards.size() == 63);
}

int main() {
    config.parse(CONFIG_FILE);
    overall_stats.load(STATS_FILE, config);
    loadCards();
    Game game;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "SFML");
    std::optional<sf::Vector2u> window_size({WINDOW_WIDTH, WINDOW_HEIGHT});
    window.setMaximumSize(window_size);
    window.setMinimumSize(window_size);
    sf::Vector2u desktop_size = sf::VideoMode::getDesktopMode().size;
    window.setPosition({
            (int)(desktop_size.x - WINDOW_WIDTH) / 2,
            (int)(desktop_size.y - WINDOW_HEIGHT) / 2
    });
    window.setFramerateLimit(FPS);
    std::optional<Range> sel, drag, hover;
    bool was_dragged, reversed, should_close;
    was_dragged = reversed = should_close = false;
    sf::Vector2i last_pos;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (should_close || event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }
            bool mouse_left = false;
            bool mouse_right = false;
            bool mouse_left_released = event->is<sf::Event::MouseButtonReleased>();
            if (auto mouse_pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                using enum sf::Mouse::Button;
                mouse_left = mouse_pressed->button == Left;
                mouse_right = mouse_pressed->button == Right;
            } else if (auto mouse_released = event->getIf<sf::Event::MouseButtonReleased>()) {
                mouse_left_released = mouse_released->button == sf::Mouse::Button::Left;
            }
            if (event->is<sf::Event::MouseMoved>()) {
                sf::Vector2i pos = sf::Mouse::getPosition(window);
                if (drag) {
                    was_dragged = true;
                    sf::Vector2i delta = pos - last_pos;
                    last_pos = pos;
                    sf::Vector2f new_pos = cards[*drag->begin].update(delta);
                    Game::setPilePositions(*drag, new_pos);
                    int inc_x;
                    if (drag->size() == 1) {
                        inc_x = CARD_WS / 2;
                    } else if (drag->isLeftToRight()) {
                        inc_x = CARD_WS / 8;
                    } else {
                        inc_x = CARD_WS * 7 / 8;
                    }
                    pos = {
                        (int)new_pos.x + inc_x,
                        (int)new_pos.y + (int)(CARD_HS / 2),
                    };
                }
                if (hover) {
                    cards[*hover->begin].hovered = false;
                    cards[hover->end[-1]].hovered = false;
                }
                hover = game.select(pos);
                if (hover) {
                    cards[sel ? hover->end[-1] : *hover->begin].hovered = true;
                }
            } else if (mouse_left || mouse_left_released) {
                last_pos = sf::Mouse::getPosition(window);
                if (mouse_left) {
                    if (sel) {
                        cards[*sel->begin].selected = false;
                    }
                    hover = game.select(last_pos);
                }
                std::optional<Range> last_sel = sel;
                std::optional<Range> last_hover = hover;
                if (hover) {
                    Card &card = cards[*hover->begin];
                    if (mouse_left
                            && !std::holds_alternative<Pile>(hover->place)
                            && (!std::holds_alternative<Cellar>(hover->place) || game.numVacantExtra())
                            && !card.isVacant()) 
                    {
                        card.selected = true;
                        card.hovered = false;
                        drag = sel = hover;
                        cards[hover->end[-1]].hovered = false;
                        hover = {};
                    }
                    if (last_sel && !same(last_sel->place, last_hover->place)) {
                        if (game.tryMove(*last_sel, *last_hover, reversed)) {
                            drag = hover = sel = {};
                        }
                    }
                } else if (mouse_left) {
                    sel = {};
                } 
                if (was_dragged && drag && mouse_left_released) {
                    std::vector<char> &row = game.getPlace(drag->place);
                    Game::setPilePositions(
                            {row.begin(), row.end(), drag->place},
                            cards[*row.begin()].sprite.getPosition());
                    cards[*drag->begin].selected = false;
                    if (reversed) {
                        std::reverse(drag->begin, drag->end);
                        Game::setPilePositions(*drag, cards[drag->end[-1]].sprite.getPosition());
                    }
                    sel = {};
                }
                if (mouse_left_released) {
                    reversed = was_dragged = false;
                    drag = {};
                }
            } else if (mouse_right || event->is<sf::Event::KeyPressed>()) {
                using enum sf::Keyboard::Key;
                auto key = event->getIf<sf::Event::KeyPressed>();
                if (mouse_right || key->code == R) {
                    if (drag && drag->size() > 1 && game.numVacantRows()) {
                        cards[*drag->begin].selected = false;
                        std::reverse(drag->begin, drag->end);
                        Game::setPilePositions(*drag, cards[drag->end[-1]].sprite.getPosition());
                        cards[*drag->begin].selected = true;
                        reversed = !reversed;
                    }
                } else if (key->code == Backspace && config.enable_undo && !drag && game.undo()) {
                    drag = hover = sel = {};
                } else if (key->code == S && key->control && !drag) {
                    bool reshuffle = true;
                    if (!game.won()) {
                        sf::Text text(font, "Press Enter to confirm reshuffle", FONT_SIZE);
                        text.setPosition({(WINDOW_WIDTH - text.getGlobalBounds().size.x) / 2, 0});
                        while (true) {
                            window.clear(COLOR_BG);
                            window.draw(game);
                            window.draw(text);
                            window.display();
                            std::optional event = window.pollEvent();
                            if (!event) continue;
                            if (event->is<sf::Event::Closed>()) {
                                should_close = true;
                                break;
                            } else if (auto key = event->getIf<sf::Event::KeyPressed>()) {
                                if (key->code == Enter) {
                                    overall_stats.recordLoss();
                                } else {
                                    reshuffle = false;
                                }
                                break;
                            } else if (event->is<sf::Event::MouseButtonPressed>()) {
                                reshuffle = false;
                                break;
                            }
                        }
                    }
                    if (reshuffle) {
                        game = Game();
                        drag = hover = sel = {};
                        was_dragged = reversed = false;
                        for (Card &card : cards) {
                            card.selected = card.hovered = false;
                        }
                    }
                }
                //else if (key->code == W) {
                //    game.stats.registerWin();
                //}
            }
        }

        window.clear(COLOR_BG);
        window.draw(game);
        if (sel) {
            for (iterator it = sel->begin; it != sel->end; ++it) {
                window.draw(cards[*it]);
            }
        }
        window.display();
    }
    if (!game.won() && config.close_is_loss) {
        overall_stats.recordLoss();
    }
    overall_stats.save(STATS_FILE, config);
    return 0;
}
