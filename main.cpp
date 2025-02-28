#include <SFML/Graphics.hpp>
#include <random>

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 800
#define FPS           60
#define OUTLINE_WIDTH 2
#define CARD_SCALE    0.4
#define CARD_MARGIN   10
#define ROW_MARGIN    20

#define CARD_WIDTH     222
#define CARD_HEIGHT    323
#define CARDS_PER_SUIT 13

using iterator = std::vector<char>::iterator;

const unsigned NUM_CARDS = 4 * CARDS_PER_SUIT;
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

char randomCard() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> distr(0, NUM_CARDS - 1);
    return distr(gen);
}

struct Card : public sf::Drawable {
    sf::Sprite sprite;
    bool selected;

    Card(const sf::Texture &texture) 
        : sprite(texture)
        , selected(false)
    {}

    sf::Vector2f update(sf::Vector2i delta) {
        sf::Vector2f pos = sprite.getPosition();
        pos.x += delta.x;
        pos.y += delta.y;
        sprite.setPosition(pos);
        return pos;
    }

    virtual void draw(sf::RenderTarget &target, sf::RenderStates) const override {
        if (selected) {
            sf::FloatRect bounds = sprite.getGlobalBounds();
            sf::RectangleShape rect(bounds.size);
            rect.setPosition(bounds.position);
            rect.setOutlineColor(COLOR_SELECT);
            rect.setOutlineThickness(OUTLINE_WIDTH);
            target.draw(rect);
        }
        target.draw(sprite);
    }
};

struct Range {
    iterator begin;
    iterator end;
    bool left_to_right;
};

std::array<sf::RenderTexture, NUM_CARDS> card_textures;
std::vector<Card> cards;

class Game : public sf::Drawable {
    std::array<std::vector<char>, 4> piles;
    std::array<std::vector<char>, 8> rows;
    std::array<std::vector<char>, 2> extra;
    std::vector<char> cellar;

public:
    static void setPilePositions(
            iterator begin,
            iterator end,
            sf::Vector2f pos,
            bool left_to_right,
            bool stacked = true)
    {
        for (iterator it = begin; it != end; ++it) {
            cards[*it].sprite.setPosition(pos);
            float dx = stacked ? CARD_WR : CARD_WS + CARD_MARGIN;
            pos.x += left_to_right ? dx : -dx;
        }
    }

    static void initPile(
            std::vector<char> &vec,
            std::array<char, NUM_CARDS> &used,
            unsigned n)
    {
        vec.reserve(n);
        for (unsigned i = 0; i < n; ++i) {
            char c = randomCard();
            for (; used[c] == -1; c = (c + 1) % NUM_CARDS);
            used[c] = -1;
            vec.push_back(c);
        }
    }

    Game() {
        cellar.reserve(1);
        std::array<char, NUM_CARDS> used;
        for (unsigned i = 0; i < NUM_CARDS; ++i) {
            used[i] = i;
        }
        sf::Vector2f pos = {
            (WINDOW_WIDTH - CARD_WS) / 2,
            START_Y,
        };
        char c = randomCard() % CARDS_PER_SUIT;
        for (unsigned i = 0; i < 4; ++i) {
            cards[c].sprite.setPosition(pos);
            used[c] = -1;
            piles[i].reserve(CARDS_PER_SUIT);
            piles[i].push_back(c);
            c += CARDS_PER_SUIT;
            pos.y += CARD_HS + ROW_MARGIN;
        }
        pos.y = START_Y;
        pos.x -= CARD_MARGIN + CARD_WS;
        for (unsigned i = 0; i < 4; ++i) {
            initPile(rows[i], used, 5);
            setPilePositions(rows[i].begin(), rows[i].end(), pos, false);
            pos.y += CARD_HS + ROW_MARGIN;
        }
        initPile(extra[0], used, 4);
        setPilePositions(extra[0].begin(), extra[0].end(), pos, false, false);
        pos.y = START_Y;
        pos.x += (CARD_WS + CARD_MARGIN) * 2;
        for (unsigned i = 4; i < 8; ++i) {
            initPile(rows[i], used, 5);
            setPilePositions(rows[i].begin(), rows[i].end(), pos, true);
            pos.y += CARD_HS + ROW_MARGIN;
        }
        initPile(extra[1], used, 4);
        setPilePositions(extra[1].begin(), extra[1].end(), pos, true, false);
    }

    std::optional<Range> select(sf::Vector2i mouse_pos) {
        sf::Vector2f pos = {(float)mouse_pos.x, (float)mouse_pos.y};
        for (unsigned i = 8; i-- > 0;) {
            std::vector<char> &row = rows[i];
            for (iterator it = row.end(); it-- != row.begin();) {
                char c = *it;
                if (cards[c].sprite.getGlobalBounds().contains(pos)) {
                    //TODO: check for run
                    if (it == row.end() - 1) {
                        cards[c].selected = true;
                        return Range(it, row.end(), i >= 4);
                    }
                    return {};
                }
            }
        }
        for (unsigned i = 2; i-- > 0;) {
            std::vector<char> &row = extra[i];
            for (iterator it = row.end(); it-- != row.begin();) {
                char c = *it;
                if (cards[c].sprite.getGlobalBounds().contains(pos)) {
                    if (it == row.end() - 1) {
                        cards[c].selected = true;
                        return Range(it, row.end());
                    }
                    return {};
                }
            }
        }
        return {};
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
        target.draw(cards[piles[0].back()]);
        target.draw(cards[piles[1].back()]);
        target.draw(cards[piles[2].back()]);
        target.draw(cards[piles[3].back()]);
    }
};

void loadCards() {
    cards.reserve(NUM_CARDS);
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
        sf::RenderTexture texture({(unsigned)CARD_WS + 2 * OUTLINE_WIDTH, (unsigned)CARD_HS + 2 * OUTLINE_WIDTH});
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
        cards.emplace_back(card_textures[i].getTexture());
    }
}

int main() {
    loadCards();
    Game game;

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "SFML");
    window.setFramerateLimit(FPS);
    std::optional<Range> sel;
    sf::Vector2i last_pos;
    while (window.isOpen()) {
        if (sel) {
            sf::Vector2i pos = sf::Mouse::getPosition(window);
            sf::Vector2i delta = pos - last_pos;
            last_pos = pos;
            sf::Vector2f new_pos = cards[*sel->begin].update(delta);
            Game::setPilePositions(sel->begin, sel->end, new_pos, sel->left_to_right);
        }

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (event->is<sf::Event::MouseButtonPressed>()) {
                last_pos = sf::Mouse::getPosition(window);
                sel = game.select(last_pos);
            } else if (event->is<sf::Event::MouseButtonReleased>()) {
                sel = {};
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
}
