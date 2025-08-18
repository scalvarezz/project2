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

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;

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
            return "C:/try/test20072025/textures/card_back.jpg";
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

        return "C:/try/test20072025/textures/" + rankStr + "_" + suitStr + ".png"; // джокеры пока названы так, что эта штука их не найдёт
    }
};

enum class GameState {
    MAIN_MENU = 0,
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

class CardRenderer {
private:
    unsigned int VAO, VBO, EBO;
    unsigned int shaderProgram;
    TextureManager textureManager;

    float vertices[20] = {
        // координаты (x, y, z)    // текстурные координаты (в каком порядке мы натягиваем текстуру)
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // верхний правый (правый верх текстуры)
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // нижний правый (правый низ текстуры) 
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // нижний левый (левый низ текстуры)
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f   // верхний левый (левый верх текстуры)
    };

    unsigned int indices[6] = {
        0, 1, 3,   
        1, 2, 3   
    };

public:
    void init() { // надо будет разобрать точнее что тут вообще происходит (ТРОГАТЬ НЕ НАДО)
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // Атрибуты вершин
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        createShaders();
    }

    void createShaders() {
        // Вершинный шейдер
        const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        
        out vec2 TexCoord;
        uniform mat4 model;
        uniform mat4 projection;
        
        void main()
        {
            gl_Position = projection * model * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        }
        )";

        // Фрагментный шейдер
        const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        void main()
        {
            FragColor = texture(ourTexture, TexCoord);
        }
        )";
    }

    void renderCard(const Card& card) { // ещё поразбирайся с этими матрицами !!!
        // Создание матрицы трансформации
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(card.position.x, card.position.y, 0.0f));
        model = glm::scale(model, glm::vec3(CARD_WIDTH, CARD_HEIGHT, 1.0f));

        // Матрица проекции (ортографическая)
        glm::mat4 projection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);

        // Установка шейдеров
        glUseProgram(shaderProgram);

        // Передача матриц в шейдер
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Загрузка текстуры
        unsigned int texture = textureManager.loadTexture(card.getTextureName());
        glBindTexture(GL_TEXTURE_2D, texture);

        // Рендеринг
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void renderCards(const std::vector<Card>& cards) {
        for (const auto& card : cards) { // просто проходка по всему массиву карт, если я правильно понимаю
            renderCard(card);
        }
    }
};

class GameTable {
private:
    CardRenderer cardRenderer;
    std::vector<Card> playerCards;
    std::vector<Card> computerCards;
    std::vector<Card> tableCards; // на карты на столе надо будет потом поставить ограничение в условные 8-10 (?) штук
    // ещё как-то надо, чтобы программа определяла, что карта действительно побита, а не просто лежит на столе, думаю о массиве списков длинной 2
    // но звучит пиздец запарно реализовывать списки чисто для этого (хотя вроде в плюсах это делается быстро)
    Card trumpCard; // козырь

public:
    // Явный конструктор по умолчанию НЕ УДАЛЯТЬ!!! по сути абсолютно бесполезен (НО В НЁМ МОЖЕТ БЫТЬ ПРОБЛЕМА!!!)
    GameTable() : cardRenderer(), playerCards(), computerCards(),
        tableCards(), trumpCard() {
        createTestDeck();
    }
    void init() {
        cardRenderer.init();
        setupInitialPositions();
    }

    // Создание тестовой колоды для проверки, нахуя ?
    void createTestDeck() {
        // Очищаем существующие карты
        playerCards.clear();
        computerCards.clear();
        tableCards.clear();

        // Создаем тестовые карты игрока
        for (int i = 0; i < 6; ++i) {
            Card::Rank rank = static_cast<Card::Rank>(Card::TWO + (i % 13));
            Card::Suit suit = static_cast<Card::Suit>(i % 4);
            playerCards.emplace_back(suit, rank, i);
        }

        // Создаем тестовые карты компьютера
        for (int i = 0; i < 6; ++i) {
            Card::Rank rank = static_cast<Card::Rank>(Card::TWO + (i % 13));
            Card::Suit suit = static_cast<Card::Suit>((i + 1) % 4);
            computerCards.emplace_back(suit, rank, i + 100);
        }

        // Козырь
        trumpCard = Card(Card::SPADES, Card::ACE, 200);
    }


    void setupInitialPositions() {
        // карты игрока 
        float startX = (WINDOW_WIDTH - 5 * CARD_WIDTH) / 2.0f; // центрируем 5 карт по ширине окна
        float y = 50.0f; // 50 пикселей от нижнего края В ПИКСЕЛЯХ ИЗ-ЗА glm::mat4 projection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);
        // эта штука создает матрицу проекции, которая преобразует пиксели в OpenGL коорды [-1, 1] 

        for (int i = 0; i < playerCards.size() && i < 6; ++i) {
            playerCards[i].position = glm::vec2(startX + i * CARD_WIDTH, y);
            playerCards[i].isFaceUp = true;
        }

        y = WINDOW_HEIGHT - 50.0f - CARD_HEIGHT;
        for (int i = 0; i < computerCards.size() && i < 6; ++i) {
            computerCards[i].position = glm::vec2(startX + i * CARD_WIDTH, y);
            computerCards[i].isFaceUp = false; 
        }

        // карты на столе
        startX = (WINDOW_WIDTH - 3 * CARD_WIDTH) / 2.0f;
        y = WINDOW_HEIGHT / 2.0f - CARD_HEIGHT / 2.0f;

        for (int i = 0; i < tableCards.size() && i < 8; ++i) {
            int row = i / 4;
            int col = i % 4;
            tableCards[i].position = glm::vec2(startX + col * CARD_WIDTH, y - row * (CARD_HEIGHT + 10));
            tableCards[i].isFaceUp = true;
        }
    }

    void render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        cardRenderer.renderCards(computerCards);
        cardRenderer.renderCards(tableCards);
        cardRenderer.renderCards(playerCards);

        if (trumpCard.suit != Card::JOKER) {
            trumpCard.position = glm::vec2(WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2);
            trumpCard.isFaceUp = true;
            cardRenderer.renderCard(trumpCard);
        }
    }

    // могут понадобиться для логики игры хз
    std::vector<Card>& getPlayerCards() { return playerCards; }
    std::vector<Card>& getComputerCards() { return computerCards; }
    std::vector<Card>& getTableCards() { return tableCards; }
    Card& getTrumpCard() { return trumpCard; }

    void setPlayerCards(const std::vector<Card>& cards) { playerCards = cards; }
    void setComputerCards(const std::vector<Card>& cards) { computerCards = cards; }
    void setTableCards(const std::vector<Card>& cards) { tableCards = cards; }
    void setTrumpCard(const Card& card) { trumpCard = card; }
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

class KeyboardManager {
private:
    GameTable& gameTable; // Ссылка на GameTable вместо дублирования векторов
    int selectedCardIndex = -1;

public:
    // Конструктор, принимающий ссылку на GameTable
    KeyboardManager(GameTable& table) : gameTable(table), selectedCardIndex(-1) {}

    // Проверка, попал ли клик по карте
    int getCardAtPosition(double xpos, double ypos) {
        // ypos от верхнего левого угла -> в нижний левый
        ypos = WINDOW_HEIGHT - ypos;

        // Получаем карты игрока из GameTable
        std::vector<Card>& playerCards = gameTable.getPlayerCards();
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
            std::cout << "Клик по карте #" << clickedCardIndex << std::endl;
            selectedCardIndex = clickedCardIndex;
            onCardSelected(clickedCardIndex);
        }
        else {
            selectedCardIndex = -1;
            std::cout << "Клик мимо карт" << std::endl;
        }
    }

    void onCardSelected(int cardIndex) {
        // Получаем карту из GameTable
        std::vector<Card>& playerCards = gameTable.getPlayerCards();
        if (cardIndex >= 0 && cardIndex < static_cast<int>(playerCards.size())) {
            std::cout << "Выбрана карта: " << playerCards[cardIndex].getTextureName() << std::endl;
        }
    }
    // Метод для получения выбранной карты
    int getSelectedCardIndex() const { return selectedCardIndex; }

    // Метод для сброса выбора
    void clearSelection() { selectedCardIndex = -1; }
};

KeyboardManager keyManager;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) { // это зачем ваще?
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        keyManager.onMouseClick(xpos, ypos);
    }
}

class GameLogic {
private:
    GameTable& gameTable;
    std::mt19937 rng; // Генератор случайных чисел

public:
    GameLogic(GameTable& table) : gameTable(table), rng(std::random_device{}()) {}

    // Создание полной колоды карт
    std::vector<Card> createFullDeck() {
        std::vector<Card> deck;
        int id = 0;

        // Создаем стандартные карты (без джокеров)
        for (int suit = 0; suit < 4; ++suit) {
            for (int rank = 0; rank < 6; ++rank) { // TWO до ACE
                deck.emplace_back(static_cast<Card::Suit>(suit),
                    static_cast<Card::Rank>(rank), id++);
            }
        }

        // Добавляем джокеров
        deck.emplace_back(Card::JOKER, Card::JOKER_RANK, id++);
        deck.emplace_back(Card::JOKER, Card::JOKER_RANK, id++);

        return deck;
    }

    // Перемешивание колоды
    void shuffleDeck(std::vector<Card>& deck) {
        std::shuffle(deck.begin(), deck.end(), rng);
    }

    // Раздача карт
    void dealCards(std::vector<Card>& deck) {
        auto& playerCards = gameTable.getPlayerCards();
        auto& computerCards = gameTable.getComputerCards();

        playerCards.clear();
        computerCards.clear();

        // Раздаем по 6 карт каждому
        for (int i = 0; i < 6; ++i) {
            if (!deck.empty()) {
                playerCards.push_back(deck.back());
                deck.pop_back();
            }
            if (!deck.empty()) {
                computerCards.push_back(deck.back());
                deck.pop_back();
            }
        }

        // Определяем козырь
        if (!deck.empty()) {
            Card trump = deck.back();
            deck.pop_back();
            gameTable.setTrumpCard(trump);
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
        auto& computerCards = gameTable.getComputerCards();
        auto& tableCards = gameTable.getTableCards();

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
        auto& computerCards = gameTable.getComputerCards();
        const Card& trumpCard = gameTable.getTrumpCard();

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
        auto& playerCards = gameTable.getPlayerCards();
        auto& tableCards = gameTable.getTableCards();

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
        auto& tableCards = gameTable.getTableCards();

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
        auto& playerCards = gameTable.getPlayerCards();
        auto& tableCards = gameTable.getTableCards();
        const Card& trumpCard = gameTable.getTrumpCard();

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
                std::cout << "Компьютер атакует картой #" << cardIndex << std::endl;
            }
        }
        else if (currentState == GameState::COMPUTER_TURN_DEFEND) {
            auto& tableCards = gameTable.getTableCards();
            if (!tableCards.empty()) {
                // Ищем последнюю непобитую карту
                int attackCardIndex = static_cast<int>(tableCards.size()) - 1;
                int defendCardIndex = getComputerDefendCard(tableCards[attackCardIndex]);

                if (defendCardIndex != -1) {
                    std::cout << "Компьютер защищается картой #" << defendCardIndex << std::endl;
                }
            }
        }
    }


    // Проверка окончания игры
    bool isGameOver() {
        auto& playerCards = gameTable.getPlayerCards();
        auto& computerCards = gameTable.getComputerCards();

        return playerCards.empty() || computerCards.empty();
    }

    // Определение победителя
    std::string getWinner() {
        auto& playerCards = gameTable.getPlayerCards();
        auto& computerCards = gameTable.getComputerCards();

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
};

class Game {
private:
    GLFWwindow* window;
    GameTable gameTable;
    AudioManager audioManager;
    KeyboardManager keyManager;
    ALuint backgroundMusic;
    GameState currentState; 

public:
    Game() : window(nullptr), gameTable(), keyManager(gameTable),
        backgroundMusic(0), currentState(GameState::MAIN_MENU) {
    }

    ~Game() {
        if (backgroundMusic) {
            audioManager.stopAudio(backgroundMusic);
        }
        if (window) {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }

    bool initialize() {
        // Инициализация GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        // Создание окна
        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Side Suicide", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(window);

        // Инициализация GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }

        // Настройка OpenGL
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Sound
        ALuint musicBuffer = audioManager.loadAudio("C:/try/test20072025/textures/Deep_Cover.mp3");
        if (musicBuffer == 0) {
            std::cout << "Warning: Failed to load background music" << std::endl;
        }
        else {
            ALuint musicSource = audioManager.playAudio(musicBuffer);
            alSourcei(musicSource, AL_LOOPING, AL_TRUE);
            audioManager.setVolume(musicSource, 0.5f);
        }

        // Инициализация игровых объектов
        gameTable.init();

        // Установка callback'ов
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                double xpos, ypos;
                glfwGetCursorPos(w, &xpos, &ypos);
                auto game = static_cast<Game*>(glfwGetWindowUserPointer(w));
                game->handleMouseClick(xpos, ypos);
            }
            });

        // Установка callback для клавиатуры
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
            auto game = static_cast<Game*>(glfwGetWindowUserPointer(w));
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                game->handleKeyPress(key);
            }
            });

        return true;
    }

    void run() {
        while (!glfwWindowShouldClose(window)) {
            update();
            render();

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    // Методы для управления состоянием игры
    void setState(GameState newState) {
        currentState = newState;
    }

    GameState getCurrentState() const {
        return currentState;
    }

private:
    void update() {
        // Обновление игрового состояния в зависимости от currentState
        switch (currentState) {
        case GameState::MAIN_MENU:
            updateMainMenu();
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

    void render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Рендеринг в зависимости от состояния
        switch (currentState) {
        case GameState::MAIN_MENU:
            renderMainMenu();
            break;
        default:
            gameTable.render();
            renderUI();
            break;
        }
    }

    void handleMouseClick(double xpos, double ypos) {
        switch (currentState) {
        case GameState::MAIN_MENU:
            // Обработка кликов в меню
            handleMainMenuClick(xpos, ypos);
            break;
        case GameState::PLAYER_TURN_ATTACK:
            // Обработка выбора карты для атаки
            keyManager.onMouseClick(xpos, ypos);
            break;
        case GameState::PLAYER_TURN_DEFEND:
            // Обработка выбора карты для защиты
            keyManager.onMouseClick(xpos, ypos);
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
    // я не уверена, стоит ли так усложнять игру, ею наверное можно просто мышкой управлять
    void handleKeyPress(int key) {
        switch (currentState) {
        case GameState::MAIN_MENU:
            if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
                currentState = GameState::PLAYER_TURN_ATTACK;
            }
            break;
        case GameState::PLAYER_TURN_ATTACK:
        case GameState::PLAYER_TURN_DEFEND:
            if (key == GLFW_KEY_ESCAPE) {
                currentState = GameState::MAIN_MENU;
            }
            break;
        case GameState::GAME_OVER:
            if (key == GLFW_KEY_R) {
                // Перезапуск игры
                restartGame();
            }
            break;
        }
    }
    

    // Методы для конкретных состояний игры
    void updateMainMenu() { // это скорее всего не понадобится 
        // Логика главного меню
    }

    void updatePlayerAttack() {
        // Логика атаки игрока
        // Проверяем выбранную карту через cardManager
        int selectedCard = keyManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            // Обрабатываем выбор карты для атаки
            std::cout << "Игрок атакует картой #" << selectedCard << std::endl;
            // Здесь будет логика игры...
            keyManager.clearSelection();
        }
    }

    void updatePlayerDefend() {
        // Логика защиты игрока
        int selectedCard = keyManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            // Обрабатываем выбор карты для защиты
            std::cout << "Игрок защищается картой #" << selectedCard << std::endl;
            // Здесь будет логика игры...
            keyManager.clearSelection();
        }
    }

    void updateComputerAttack() {
        // Логика атаки компьютера
        // После завершения хода компьютера меняем состояние
        // currentState = GameState::PLAYER_TURN_DEFEND;
    }

    void updateComputerDefend() {
        // Логика защиты компьютера
        // После завершения хода компьютера меняем состояние
        // currentState = GameState::PLAYER_TURN_ATTACK;
    }

    void updateGameOver() {
        // Логика конца игры
    }

    void renderMainMenu() {
        // Рендеринг главного меню
        std::cout << "Отображение главного меню" << std::endl;
        // Здесь будет рендеринг меню
    }

    void renderUI() {
        // Рендеринг интерфейса (кнопки, текст и т.д.)
        switch (currentState) {
        case GameState::PLAYER_TURN_ATTACK:
            std::cout << "Ход игрока: атака" << std::endl;
            break;
        case GameState::PLAYER_TURN_DEFEND:
            std::cout << "Ход игрока: защита" << std::endl;
            break;
        case GameState::COMPUTER_TURN_ATTACK:
            std::cout << "Ход компьютера: атака" << std::endl;
            break;
        case GameState::COMPUTER_TURN_DEFEND:
            std::cout << "Ход компьютера: защита" << std::endl;
            break;
        }
    }

    void handleMainMenuClick(double xpos, double ypos) {
        // Обработка кликов в главном меню
        // Пока просто переход в игру
        currentState = GameState::PLAYER_TURN_ATTACK;
    }

    void restartGame() {
        // Перезапуск игры
        gameTable = GameTable();
        gameTable.init();
        currentState = GameState::MAIN_MENU;
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
