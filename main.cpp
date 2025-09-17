#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <AL/al.h>
#include <AL/alc.h>
#include <sndfile.hh>
#include <cstdlib>
#include <set>
#include <random>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


const float WINDOW_WIDTH = 1024.0f;
const float WINDOW_HEIGHT = 768.0f;

const float CARD_WIDTH = 80.0f;
const float CARD_HEIGHT = 120.0f;

class Card {
public:
    enum Suit {
        SPADES = 0,     // Спутники - наука - пики
        HEARTS = 1,         // Ракеты - армия - черви
        DIAMONDS = 2,       // Звёзды - пропаганда - бубны
        CLUBS = 3,          // Астероиды - разрушение - трефы
        JOKER = 4        // Коммунизм - джокер - для обозначения его цвета, но тут ещё надо подумать, как это сделать не так по-уебански
    };

    enum Rank {
        TWO = 0,        // Народные массы
        TEN = 1,       // Пионер
        JACK = 2,      // Октябрёнок
        QUEEN = 3,     // Комсомолка
        KING = 4,      // Вождь
        ACE = 5,       // Революция
        JOKER_RANK = 6 // Коммунизм
    };

    Suit suit;
    Rank rank;
    bool isFaceUp; // то есть твоя карта или чужая (лицом к верху?)
    glm::vec2 position;
    int id; // Уникальный ID для идентификации

    // Конструктор по умолчанию
    Card() : suit(SPADES), rank(TWO), isFaceUp(true), position(0.0f, 0.0f), id(-1) {}

    Card(Suit s, Rank r, int cardId) : suit(s), rank(r), isFaceUp(true), id(cardId) {
        position = glm::vec2(0.0f, 0.0f);
    }

    std::string getTextureName() const {
        if (!isFaceUp) {
            return "C:/textures/card_back.jpg";
        }

        std::string suitStr;
        if (suit == SPADES) suitStr = "spades";
        else if (suit == HEARTS) suitStr = "hearts";
        else if (suit == DIAMONDS) suitStr = "diamonds";
        else if (suit == CLUBS) suitStr = "clubs";
        else if (suit == JOKER) suitStr = "joker";

        std::string rankStr;
        if (rank == TWO) rankStr = "2";
        else if (rank == TEN) rankStr = "10";
        else if (rank == JACK) rankStr = "J";
        else if (rank == QUEEN)  rankStr = "Q";
        else if (rank == KING) rankStr = "K";
        else if (rank == ACE) rankStr = "A";
        else if (rank == JOKER_RANK) rankStr = "Communism";
        // else rankStr = std::to_string(rank);  пидорас

        return "C:/textures/" + rankStr + "_of_" + suitStr + ".png"; // джокеры пока названы так, что эта штука их не найдёт
    }
};

enum class GameState {
    START_GAME = 0,
    PLAYER_TURN_ATTACK = 1,
    PLAYER_TURN_DEFEND = 2,
    COMPUTER_TURN_ATTACK = 3,
    COMPUTER_TURN_DEFEND = 4,
    GAME_OVER = 5
};


class TextureManager {
private:
    std::map<std::string, unsigned int> textures;

public:
    ~TextureManager() {
        // Очистка всех текстур
        for (auto& pair : textures) {
            glDeleteTextures(1, &pair.second);
        }
    }

    unsigned int loadTexture(const std::string& filename) {
        // Проверяем, загружена ли уже текстура
        if (textures.find(filename) != textures.end()) {
            return textures[filename];
        }

        // Генерируем ID для новой текстуры
        unsigned int textureID;
        glGenTextures(1, &textureID);

        // Загружаем изображение
        int width, height, nrChannels;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);

        if (data) {
            GLenum format;
            if (nrChannels == 1)
                format = GL_RED;
            else if (nrChannels == 3)
                format = GL_RGB;
            else if (nrChannels == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);

            // Загружаем текстуру
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Параметры текстуры
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Сохраняем текстуру в кэш
            textures[filename] = textureID;

            std::cout << "Successfully loaded texture: " << filename << std::endl;
        }
        else {
            std::cout << "Failed to load texture: " << filename << std::endl;
            std::cout << "STB error: " << stbi_failure_reason() << std::endl;
            glDeleteTextures(1, &textureID); // Удаляем пустую текстуру
            textureID = 0; // Возвращаем 0 как индикатор ошибки
        }

        // Освобождаем память
        stbi_image_free(data);
        return textureID;
    }
};

static unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return id;
}

static unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) { // эти 2 функции статиковые вроде уже должны быть рабочими
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}


class Renderer {
private:
    unsigned int cardVAO, cardVBO, cardEBO;
    unsigned int cardShaderProgram;
    unsigned int backgroundVAO, backgroundVBO, backgroundEBO;
    unsigned int backgroundShaderProgram;
    TextureManager textureManager;

    float cardVertices[20] = {
        // pos(x,y,z)     // tex coords
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
    };

    unsigned int cardIndices[6] = { 0, 1, 3, 1, 2, 3 };

public:
    void init() {
        initCardRendering();
        initBackgroundRendering();
    }

private:
    void initCardRendering() {
        glGenVertexArrays(1, &cardVAO);
        glGenBuffers(1, &cardVBO);
        glGenBuffers(1, &cardEBO);

        glBindVertexArray(cardVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cardVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cardVertices), cardVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cardEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cardIndices), cardIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        const char* cardVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        uniform mat4 model;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * model * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        })";

        const char* cardFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        void main() {
            FragColor = texture(ourTexture, TexCoord);
        })";

        cardShaderProgram = createShaderProgram(cardVertexShader, cardFragmentShader);
    }

    void initBackgroundRendering() {
        glGenVertexArrays(1, &backgroundVAO);
        glGenBuffers(1, &backgroundVBO);
        glGenBuffers(1, &backgroundEBO);

        float backgroundVertices[] = {
            -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
             1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,  1.0f, 1.0f
        };

        unsigned int backgroundIndices[] = { 0, 1, 3, 1, 2, 3 };

        glBindVertexArray(backgroundVAO);
        glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVertices), backgroundVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backgroundEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(backgroundIndices), backgroundIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        const char* backgroundVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        })";

        const char* backgroundFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D backgroundTexture;
        void main() {
            FragColor = texture(backgroundTexture, TexCoord);
        })";

        backgroundShaderProgram = createShaderProgram(backgroundVertexShader, backgroundFragmentShader);
    }


public:
    void renderCard(const Card& card, const glm::mat4& projection) {
        glm::mat4 model = glm::mat4(1.0f);
        // Позиция в пикселях + смещение на половину размера для центрирования
        model = glm::translate(model, glm::vec3(
            card.position.x + CARD_WIDTH / 2,
            card.position.y + CARD_HEIGHT / 2,
            0.0f
        ));
        model = glm::scale(model, glm::vec3(CARD_WIDTH, CARD_HEIGHT, 1.0f));

        glUseProgram(cardShaderProgram);

        unsigned int modelLoc = glGetUniformLocation(cardShaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        unsigned int projLoc = glGetUniformLocation(cardShaderProgram, "projection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        unsigned int texture = textureManager.loadTexture(card.getTextureName());
        if (texture != 0) {
            glBindTexture(GL_TEXTURE_2D, texture);
        }

        glBindVertexArray(cardVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void renderCards(const std::vector<Card>& cards, const glm::mat4& projection) {
        std::cout << "cards[i].suit <<  << cards[i].rank" << std::endl;
        for (int i = 0; i < cards.size(); i++) {
            renderCard(cards[i], projection);
        }
    }

    void renderBackground(const std::string& texturePath) {
        unsigned int texture = textureManager.loadTexture(texturePath);
        if (texture == 0) return;

        glUseProgram(backgroundShaderProgram);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(backgroundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
};


class AudioManager { // идентичный старому проекту, работает 
    ALCdevice* device;
    ALCcontext* context;
    ALuint buffer;
    ALuint source;
public:
    AudioManager() {
        device = alcOpenDevice(nullptr);
        context = alcCreateContext(device, nullptr);
        buffer = 0;
        source = 0;
        alcMakeContextCurrent(context);
    };
    ~AudioManager() {
        alcDestroyContext(context);
        alcCloseDevice(device);
    }
    ALuint loadAudio(const std::string& filePath) {
        SF_INFO fileInfo;
        SNDFILE* file = sf_open(filePath.c_str(), SFM_READ, &fileInfo);
        if (!file) {
            std::cerr << "Failed to open audio file: " << filePath << std::endl;
            return 0;
        }

        std::vector<short> audioData(fileInfo.frames * fileInfo.channels);
        sf_readf_short(file, audioData.data(), fileInfo.frames);
        sf_close(file);

        ALuint buffer;
        alGenBuffers(1, &buffer);
        alBufferData(buffer, fileInfo.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
            audioData.data(), static_cast<ALsizei>(audioData.size() * sizeof(short)), static_cast<ALsizei>(fileInfo.samplerate));

        return buffer;
    }
    ;
    ALuint playAudio(ALuint buffer) {
        ALuint source;
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer);
        alSourcePlay(source);
        return source;
    };

    void stopAudio(ALuint source) {
        alSourceStop(source);
    }
    void setVolume(ALuint source, float volume) {
        alSourcef(source, AL_GAIN, volume);
    };
};

class GameLogic {
private:
    std::mt19937 rng; // Генератор случайных чисел
    std::vector<Card> Deck;
    std::vector<Card> playerCards;
    std::vector<Card> computerCards;
    std::vector<Card> tableCards; // на карты на столе надо будет потом поставить ограничение в условные 8-10 (?) штук
    Card trumpCard; // козырь

public:
    GameLogic() : rng(std::random_device{}()) {}

    // Создание полной колоды карт
    std::vector<Card> createFullDeck() {
        int id = 0;

        // Создаем стандартные карты (без джокеров)
        for (int suit = 0; suit < 4; ++suit) {
            for (int rank = 0; rank < 6; ++rank) { // TWO до ACE
                Deck.emplace_back(static_cast<Card::Suit>(suit),
                    static_cast<Card::Rank>(rank), id++);
            }
        }

        // Добавляем джокеров
        Deck.emplace_back(Card::JOKER, Card::JOKER_RANK, id++);
        Deck.emplace_back(Card::JOKER, Card::JOKER_RANK, id++);

        return Deck;
    }

    // Перемешивание колоды
    void shuffleDeck(std::vector<Card>& Deck) {
        std::shuffle(Deck.begin(), Deck.end(), rng);
    }

    // Раздача карт
    void dealCards(std::vector<Card>& Deck) {

        playerCards.clear();
        computerCards.clear();

        // Раздаем по 6 карт каждому
        for (int i = 0; i < 6; ++i) {
            if (!Deck.empty()) {
                playerCards.push_back(Deck.back());
                Deck.pop_back();
            }
            if (!Deck.empty()) {
                computerCards.push_back(Deck.back());
                Deck.pop_back();
            }
        }

        // Определяем козырь
        if (!Deck.empty()) {
            Card trump = Deck.back();
            Deck.pop_back();
            trumpCard = trump;
        }
    }

    // Логика битвы карт

    // Проверка, можно ли побить карту attackCard картой defendCard
    bool canBeatCard(const Card& attackCard, const Card& defendCard, const Card& trumpCard) {
        // Джокер бьет все карты
        if (defendCard.rank == Card::JOKER_RANK) {
            return true;
        }

        // Джокера можно побить только другим джокером
        if (attackCard.rank == Card::JOKER_RANK) {
            return defendCard.rank == Card::JOKER_RANK;
        }

        // Если козырь
        if (defendCard.suit == trumpCard.suit) {
            // Козырь бьет все некозырные карты
            if (attackCard.suit != trumpCard.suit) {
                return true;
            }
            // Козырь бьет козырь только если старше
            else {
                return getCardValue(defendCard) > getCardValue(attackCard);
            }
        }
        // Если обе карты некозырные
        else if (attackCard.suit == defendCard.suit) {
            // Одинаковая масть - сравниваем достоинства
            return getCardValue(defendCard) > getCardValue(attackCard);
        }

        // Разные масти, защита не козырь - нельзя побить
        return false;
    }

    // Получение числового значения карты для сравнения
    int getCardValue(const Card& card) {
        if (card.rank == Card::JOKER_RANK) return 100; // Джокер самый старший

        switch (card.rank) {
        case Card::TWO: return 2;
        case Card::TEN: return 10;
        case Card::JACK: return 11;
        case Card::QUEEN: return 12;
        case Card::KING: return 13;
        case Card::ACE: return 14;
        default: return 1;
        }
    }

    // ========================================================================================
    // Логика ИИ компьютера ТУТ НАДО ПРИКРУТИТЬ ПАРАЛЛЕЛИЗМА ОБЯЗАТЕЛЬНО!!!!!!!!!!!!!!!!!!!!!!!
    // ========================================================================================

    // Выбор карты для атаки компьютером
    int getComputerAttackCard() {

        if (computerCards.empty()) return -1;

        // Если стол пуст - атакуем любой картой
        if (tableCards.empty()) {
            return 0; // Просто первую карту
        }

        // Ищем карту, которой можно атаковать (по рангу уже лежащих на столе)
        std::set<Card::Rank> tableRanks;
        for (const auto& card : tableCards) {
            tableRanks.insert(card.rank);
        }

        for (size_t i = 0; i < computerCards.size(); ++i) {
            if (tableRanks.find(computerCards[i].rank) != tableRanks.end()) {
                return static_cast<int>(i);
            }
        }

        return -1; // Нет подходящей карты
    }

    // Выбор карты для защиты компьютером
    int getComputerDefendCard(const Card& attackCard) {

        if (computerCards.empty()) return -1;

        int bestCardIndex = -1;
        int bestCardValue = INT_MAX; // Ищем минимальную подходящую карту

        for (size_t i = 0; i < computerCards.size(); ++i) {
            if (canBeatCard(attackCard, computerCards[i], trumpCard)) {
                int value = getCardValue(computerCards[i]);
                if (value < bestCardValue) {
                    bestCardValue = value;
                    bestCardIndex = static_cast<int>(i);
                }
            }
        }

        return bestCardIndex;
    }

    // Управление ходом игры

    // Игрок атакует
    bool playerAttack(int cardIndex) {

        if (cardIndex < 0 || cardIndex >= static_cast<int>(playerCards.size())) {
            return false;
        }

        // Проверяем, можно ли атаковать этой картой
        if (tableCards.empty() || canAttackWithCard(playerCards[cardIndex])) {
            Card attackingCard = playerCards[cardIndex];
            attackingCard.id = cardIndex; // Сохраняем индекс для последующего удаления

            tableCards.push_back(attackingCard);
            playerCards.erase(playerCards.begin() + cardIndex);

            return true;
        }

        return false;
    }

    // Проверка, можно ли атаковать выбранной картой
    bool canAttackWithCard(const Card& card) {

        if (tableCards.empty()) return true; // Первая карта всегда может атаковать

        // Проверяем, есть ли на столе карта с таким же рангом
        for (const auto& tableCard : tableCards) {
            if (tableCard.rank == card.rank) {
                return true;
            }
        }

        return false;
    }

    // Игрок защищается
    bool playerDefend(int attackCardIndex, int defendCardIndex) {

        if (attackCardIndex < 0 || attackCardIndex >= static_cast<int>(tableCards.size()) ||
            defendCardIndex < 0 || defendCardIndex >= static_cast<int>(playerCards.size())) {
            return false;
        }

        Card& attackCard = tableCards[attackCardIndex];
        Card& defendCard = playerCards[defendCardIndex];

        if (canBeatCard(attackCard, defendCard, trumpCard)) {
            // Помечаем, что карта побита
            attackCard.isFaceUp = false; // Временно используем для пометки
            tableCards.push_back(defendCard);
            playerCards.erase(playerCards.begin() + defendCardIndex);

            return true;
        }

        return false;
    }

    // Компьютер делает ход
    void computerMakeMove(GameState currentState) {
        if (currentState == GameState::COMPUTER_TURN_ATTACK) {
            int cardIndex = getComputerAttackCard();
            if (cardIndex != -1) {
                // Здесь должна быть логика атаки компьютера
                std::cout << "Computer attacks with #" << cardIndex << std::endl;
            }
        }
        else if (currentState == GameState::COMPUTER_TURN_DEFEND) {
            if (!tableCards.empty()) {
                // Ищем последнюю непобитую карту
                int attackCardIndex = static_cast<int>(tableCards.size()) - 1;
                int defendCardIndex = getComputerDefendCard(tableCards[attackCardIndex]);

                if (defendCardIndex != -1) {
                    std::cout << "Computer defends with #" << defendCardIndex << std::endl;
                }
            }
        }
    }


    // Проверка окончания игры
    bool isGameOver() {
        return playerCards.empty() || computerCards.empty();
    }

    // Определение победителя
    std::string getWinner() {

        if (playerCards.empty() && computerCards.empty()) {
            return "Ничья!";
        }
        else if (playerCards.empty()) {
            return "Игрок победил!";
        }
        else if (computerCards.empty()) {
            return "Компьютер победил!";
        }

        return "Игра продолжается";
    }

    std::vector<Card>& getPlayerCards() { return playerCards; }
    std::vector<Card>& getDeck() { return Deck; }
    std::vector<Card>& getComputerCards() { return computerCards; }
    std::vector<Card>& getTableCards() { return tableCards; }
    Card& getTrumpCard() { return trumpCard; }
    int getsize(std::vector<Card>& vec) { return vec.size(); }


    void setPlayerCards(const std::vector<Card>& cards) { playerCards = cards; }
    void setComputerCards(const std::vector<Card>& cards) { computerCards = cards; }
    void setTableCards(const std::vector<Card>& cards) { tableCards = cards; }
    void setTrumpCard(const Card& card) { trumpCard = card; }

};

class MouseManager {
private:
    GameLogic gameLogic; // Ссылка на GameTable вместо дублирования векторов
    int selectedCardIndex = -1;

public:
    // Конструктор, принимающий ссылку на GameTable
    MouseManager() : gameLogic(), selectedCardIndex(-1) {}

    // Проверка, попал ли клик по карте
    int getCardAtPosition(double xpos, double ypos) {
        // ypos от верхнего левого угла -> в нижний левый
        ypos = WINDOW_HEIGHT - ypos;

        // Получаем карты игрока из GameTable
        std::vector<Card>& playerCards = gameLogic.getPlayerCards();
        // Проверяем карты игрока (в порядке справа налево, чтобы верхние карты проверялись первыми)
        for (int i = playerCards.size() - 1; i >= 0; --i) {
            if (i >= 6) continue;

            const Card& card = playerCards[i];

            if (xpos >= card.position.x &&
                xpos <= card.position.x + CARD_WIDTH &&
                ypos >= card.position.y &&
                ypos <= card.position.y + CARD_HEIGHT) {
                return i; // Возвращаем индекс найденной карты
            }
        }

        return -1;
    }

    void onMouseClick(double xpos, double ypos) {
        int clickedCardIndex = getCardAtPosition(xpos, ypos);
        if (clickedCardIndex != -1) {
            std::cout << "Click on card #" << clickedCardIndex << std::endl;
            selectedCardIndex = clickedCardIndex;
            onCardSelected(clickedCardIndex);
        }
        else {
            selectedCardIndex = -1;
            std::cout << "Not on card click" << std::endl;
        }
    }

    void onCardSelected(int cardIndex) {
        // Получаем карту из GameTable
        std::vector<Card>& playerCards = gameLogic.getPlayerCards();
        if (cardIndex >= 0 && cardIndex < static_cast<int>(playerCards.size())) {
            std::cout << "Chosen card: " << playerCards[cardIndex].getTextureName() << std::endl;
        }
    }
    // Метод для получения выбранной карты
    int getSelectedCardIndex() const { return selectedCardIndex; }

    // Метод для сброса выбора
    void clearSelection() { selectedCardIndex = -1; }
};

class GameTable {
private:
    GameLogic gameLogic;
    Renderer renderer;
    glm::mat4 projectionMatrix;

public:
    GameTable() {
        // Создаем матрицу проекции один раз при инициализации
        projectionMatrix = glm::ortho(
            0.0f, static_cast<float>(WINDOW_WIDTH),
            0.0f, static_cast<float>(WINDOW_HEIGHT),
            -1.0f, 1.0f
        );
    }

    void init() {
        renderer.init();
        setupInitialPositions();
    }

    void setupInitialPositions() {
        float startX = (WINDOW_WIDTH - 6 * CARD_WIDTH) / 2.0f;

        // Карты игрока (низ экрана)
        float playerY = 50.0f;
        for (int i = 0; i < gameLogic.getPlayerCards().size() && i < 6; ++i) {
            gameLogic.getPlayerCards()[i].position = glm::vec2(
                startX + i * CARD_WIDTH,
                playerY
            );
            gameLogic.getPlayerCards()[i].isFaceUp = true;
        }

        // Карты компьютера (верх экрана)
        float computerY = WINDOW_HEIGHT - 50.0f - CARD_HEIGHT;
        for (int i = 0; i < gameLogic.getComputerCards().size() && i < 6; ++i) {
            gameLogic.getComputerCards()[i].position = glm::vec2(
                startX + i * CARD_WIDTH,
                computerY
            );
            gameLogic.getComputerCards()[i].isFaceUp = false;
        }

        // Карты на столе (центр)
        float tableStartX = (WINDOW_WIDTH - 4 * CARD_WIDTH) / 2.0f;
        float tableY = WINDOW_HEIGHT / 2.0f - CARD_HEIGHT / 2.0f;

        for (int i = 0; i < gameLogic.getTableCards().size() && i < 8; ++i) {
            int row = i / 4;
            int col = i % 4;
            gameLogic.getTableCards()[i].position = glm::vec2(
                tableStartX + col * CARD_WIDTH,
                tableY - row * (CARD_HEIGHT + 10)
            );
            gameLogic.getTableCards()[i].isFaceUp = true;
        }

        // Козырь
        if (gameLogic.getTrumpCard().suit != Card::JOKER) {
            gameLogic.getTrumpCard().position = glm::vec2(
                WINDOW_WIDTH - CARD_WIDTH - 20,
                WINDOW_HEIGHT / 2 - CARD_HEIGHT / 2
            );
            gameLogic.getTrumpCard().isFaceUp = true;
        }
    }

    void render() {
        // Очищаем экран
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Рендерим фон
        renderer.renderBackground("C:/textures/table.jpg");

        // Рендерим все карты с общей матрицей проекции
        renderer.renderCards(gameLogic.getTableCards(), projectionMatrix);
        renderer.renderCards(gameLogic.getPlayerCards(), projectionMatrix);

        // Рендерим козырь отдельно, если нужно
        if (gameLogic.getTrumpCard().suit != Card::JOKER) {
            renderer.renderCard(gameLogic.getTrumpCard(), projectionMatrix);
        }
    }

    // Геттеры для доступа к логике игры
    GameLogic& getGameLogic() { return gameLogic; }
};


class Game {
private:
    GLFWwindow* window;
    GameTable gameTable;
    GameLogic gameLogic;
    Renderer renderer;
    AudioManager audioManager;
    MouseManager mouseManager;
    GameState currentState;

public:
    Game() : window(nullptr), currentState(GameState::START_GAME) {}

    bool initialize() {
        if (!glfwInit()) return false;

        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Card Game", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            glfwTerminate();
            return false;
        }

        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Инициализируем игровой стол
        gameTable.init();

        // Настройка обработки мыши
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                double xpos, ypos;
                glfwGetCursorPos(w, &xpos, &ypos);
                auto game = static_cast<Game*>(glfwGetWindowUserPointer(w));
                game->handleMouseClick(xpos, ypos);
            }
            });

        return true;
    }

    void run() {
        while (!glfwWindowShouldClose(window)) {
            update();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

private:
    void update() {
        // Обновление игрового состояния в зависимости от currentState
        switch (currentState) {
        case GameState::START_GAME:
            updatestartgame();
            break;
        case GameState::PLAYER_TURN_ATTACK:
            updatePlayerAttack();
            break;
        case GameState::PLAYER_TURN_DEFEND:
            updatePlayerDefend();
            break;
        case GameState::COMPUTER_TURN_ATTACK:
            updateComputerAttack();
            break;
        case GameState::COMPUTER_TURN_DEFEND:
            updateComputerDefend();
            break;
        case GameState::GAME_OVER:
            updateGameOver();
            break;
        }
    }

    void handleMouseClick(double xpos, double ypos) {
        switch (currentState) {
        case GameState::PLAYER_TURN_ATTACK:
            mouseManager.onMouseClick(xpos, ypos);
            break;
        case GameState::PLAYER_TURN_DEFEND:
            mouseManager.onMouseClick(xpos, ypos);
            break;
        case GameState::COMPUTER_TURN_ATTACK:
        case GameState::COMPUTER_TURN_DEFEND:
            // Возможно, обработка кликов во время хода компьютера
            break;
        case GameState::GAME_OVER:
            // Обработка кликов в конце игры
            break;
        }
    }

    void updatePlayerAttack() {
        gameTable.render();
        int selectedCard = mouseManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            if (gameLogic.playerAttack(selectedCard)) {
                std::cout << "Player attacks with card #" << selectedCard << std::endl;
                currentState = GameState::COMPUTER_TURN_DEFEND;
            }
            mouseManager.clearSelection();
        }
    }

    void updatePlayerDefend() {
        int selectedCard = mouseManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            auto& tableCards = gameLogic.getTableCards();
            if (!tableCards.empty()) {
                int attackCardIndex = static_cast<int>(tableCards.size()) - 1;
                if (gameLogic.playerDefend(attackCardIndex, selectedCard)) {
                    std::cout << "Player defends with card #" << selectedCard << std::endl;
                    currentState = GameState::COMPUTER_TURN_ATTACK;
                }
            }
            mouseManager.clearSelection();
        }
    }

    void updateComputerAttack() {
        gameLogic.computerMakeMove(GameState::COMPUTER_TURN_ATTACK);
        currentState = GameState::PLAYER_TURN_DEFEND;
    }

    void updateComputerDefend() {
        gameLogic.computerMakeMove(GameState::COMPUTER_TURN_DEFEND);
        currentState = GameState::PLAYER_TURN_ATTACK;
    }

    void updateGameOver() {
        // Логика конца игры
        if (gameLogic.isGameOver()) {
            std::cout << gameLogic.getWinner() << std::endl;
        }
    }

    void updatestartgame() {
        gameLogic.createFullDeck();
        gameLogic.shuffleDeck(gameLogic.getDeck());
        gameLogic.dealCards(gameLogic.getDeck());
        gameTable.render();
        int x;
        std::cin >> x;
        currentState = GameState::PLAYER_TURN_ATTACK;
    }

    void restartGame() {
        gameTable = GameTable();
        gameTable.init();
        currentState = GameState::PLAYER_TURN_ATTACK;
    }
};

int main() {
    Game game;

    if (!game.initialize()) {
        std::cerr << "Failed to initialize game!" << std::endl;
        return -1;
    }

    game.run();

    return 0;
}
