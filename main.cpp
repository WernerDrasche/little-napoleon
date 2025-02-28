#include <SFML/Graphics.hpp>
#include <random>

#define WINDOW_WIDTH  1200
#define WINDOW_HEIGHT 750
#define FPS           60
#define OUTLINE_WIDTH 2
#define CARD_SCALE    0.4

#define CARD_WIDTH     222
#define CARD_HEIGHT    323
#define CARDS_PER_SUIT 13

const unsigned NUM_CARDS = 4 * CARDS_PER_SUIT;
const float CARD_WS = CARD_WIDTH * CARD_SCALE;
const float CARD_HS = CARD_HEIGHT * CARD_SCALE;
const sf::Color COLOR_BG = {40, 150, 80};
const sf::Color COLOR_CARD = sf::Color::White;
const sf::Color COLOR_OUTLINE = sf::Color::Black;

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

class Card : public sf::Drawable {
    sf::Sprite sprite;

public:
    Card(const sf::Texture &texture) : sprite(texture) {}

    virtual void draw(sf::RenderTarget &target, sf::RenderStates) const override {
        target.draw(sprite);
    }
};

std::array<sf::RenderTexture, NUM_CARDS> card_textures;
std::vector<Card> cards;

class Game : public sf::Drawable {
    std::array<std::vector<char>, 4> piles;
    std::array<std::vector<char>, 8> rows;
    std::array<std::vector<char>, 2> extra;
    char cellar;

    static void initPile(
            std::vector<char> &vec,
            std::array<char, NUM_CARDS> &used,
            unsigned n)
    {
        vec.reserve(n);
        for (unsigned i = 0; i < n; ++i) {
            char c = randomCard();
            for (; used[c] != -1; c = (c + 1) % CARDS_PER_SUIT);
            used[c] = -1;
            vec.push_back(c);
        }
    }

public:
    Game() {
        cellar = -1;
        std::array<char, NUM_CARDS> used;
        for (unsigned i = 0; i < NUM_CARDS; ++i) {
            used[i] = i;
        }
        char c = randomCard() % CARDS_PER_SUIT;
        for (unsigned i = 0; i < 4; ++i) {
            used[c] = -1;
            piles[i].reserve(CARDS_PER_SUIT);
            piles[i].push_back(c);
            c += CARDS_PER_SUIT;
        }
        for (auto &row : rows) {
            initPile(row, used, 5);
        }
        for (auto &row : extra) {
            initPile(row, used, 4);
        }
    }

    virtual void draw(sf::RenderTarget &target, sf::RenderStates) const override {
        target.draw(cards[2], sf::Transform());
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
        sf::RenderTexture texture({(unsigned)CARD_WS, (unsigned)CARD_HS});
        texture.clear(COLOR_CARD);
        sf::RectangleShape rect({CARD_WS - 2 * OUTLINE_WIDTH, CARD_HS - 2 * OUTLINE_WIDTH});
        rect.setPosition({OUTLINE_WIDTH, OUTLINE_WIDTH});
        rect.setOutlineColor(COLOR_OUTLINE);
        rect.setOutlineThickness(OUTLINE_WIDTH);
        texture.draw(rect);
        sf::Texture tmp(strbuf);
        sf::Sprite sprite(tmp);
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
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        window.clear(COLOR_BG);
        window.draw(game);
        window.display();
    }
}
