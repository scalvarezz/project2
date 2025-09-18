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
#include <thread>
#include <mutex>
#include <future>
#include <algorithm>
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
        BLACK = 4,
        RED = 5
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

        std::string rankStr;
        if (rank == TWO) rankStr = "2";
        else if (rank == TEN) rankStr = "10";
        else if (rank == JACK) rankStr = "jack";
        else if (rank == QUEEN)  rankStr = "queen";
        else if (rank == KING) rankStr = "king";
        else if (rank == ACE) rankStr = "ace";
        else if (rank == JOKER_RANK) {
            rankStr = "joker";
            if (suit == BLACK) suitStr = "black";
            if (suit == RED) suitStr = "red";
        }


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
        stbi_set_flip_vertically_on_load(true);
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
        for (int i = 0; i < cards.size(); ++i) {
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
    std::mutex aiMutex;


    // Простой расчет без многопоточности
    int calculateSimpleAIMove(bool isAttackTurn) {
        if (isAttackTurn) {
            return findBestAttackCard();
        }
        else {
            return findBestDefenseCard();
        }
    }

    // Многопоточный расчет
    int calculateMultiThreadAIMove(bool isAttackTurn, int numThreads) {
        std::vector<std::future<int>> futures;
        std::vector<int> results;

        if (isAttackTurn) {
            // Для атаки: каждый поток ищет лучшую карту для атаки
            int cardsPerThread = std::max(1, (int)computerCards.size() / numThreads);

            for (int i = 0; i < numThreads; ++i) {
                int startIdx = i * cardsPerThread;
                int endIdx = std::min((i + 1) * cardsPerThread, (int)computerCards.size());

                if (startIdx >= endIdx) break;

                futures.push_back(std::async(std::launch::async, [this, startIdx, endIdx]() {
                    return findBestAttackCardInRange(startIdx, endIdx);
                    }));
            }
        }
        else {
            // Для защиты: анализируем разные стратегии защиты
            futures.push_back(std::async(std::launch::async, [this]() {
                return findMinimalDefenseCard(); // Минимальная карта для отбоя
                }));

            futures.push_back(std::async(std::launch::async, [this]() {
                return findStrategicDefenseCard(); // Стратегическая карта
                }));
        }

        // Собираем результаты
        for (auto& future : futures) {
            int result = future.get();
            if (result != -1) {
                results.push_back(result);
            }
        }

        if (results.empty()) return -1;

        // Выбираем лучший результат на основе эвристики
        return selectBestMove(results, isAttackTurn);
    }

    int findBestAttackCard() {
        int bestCardIndex = -1;
        int bestScore = INT_MIN;

        for (int i = 0; i < computerCards.size(); ++i) {
            if (canAttackWithCard(computerCards[i])) {
                int score = evaluateAttackCard(computerCards[i], i);
                if (score > bestScore) {
                    bestScore = score;
                    bestCardIndex = i;
                }
            }
        }

        return bestCardIndex;
    }

    int findBestAttackCardInRange(int startIdx, int endIdx) {
        int bestCardIndex = -1;
        int bestScore = INT_MIN;

        for (int i = startIdx; i < endIdx; ++i) {
            if (canAttackWithCard(computerCards[i])) {
                int score = evaluateAttackCard(computerCards[i], i);
                if (score > bestScore) {
                    bestScore = score;
                    bestCardIndex = i;
                }
            }
        }

        return bestCardIndex;
    }

    int findBestDefenseCard() {
        if (tableCards.empty()) return -1;

        Card attackCard = tableCards.back();
        int bestCardIndex = -1;
        int bestScore = INT_MIN;

        for (int i = 0; i < computerCards.size(); ++i) {
            if (canBeatCard(attackCard, computerCards[i], trumpCard)) {
                int score = evaluateDefenseCard(computerCards[i], attackCard, i);
                if (score > bestScore) {
                    bestScore = score;
                    bestCardIndex = i;
                }
            }
        }

        return bestCardIndex;
    }

    int findMinimalDefenseCard() {
        if (tableCards.empty()) return -1;

        Card attackCard = tableCards.back();
        int bestCardIndex = -1;
        int minValue = INT_MAX;

        for (int i = 0; i < computerCards.size(); ++i) {
            if (canBeatCard(attackCard, computerCards[i], trumpCard)) {
                int value = getCardValue(computerCards[i]);
                if (value < minValue) {
                    minValue = value;
                    bestCardIndex = i;
                }
            }
        }

        return bestCardIndex;
    }

    int findStrategicDefenseCard() {
        if (tableCards.empty()) return -1;

        Card attackCard = tableCards.back();
        int bestCardIndex = -1;
        int bestScore = INT_MIN;

        for (int i = 0; i < computerCards.size(); ++i) {
            if (canBeatCard(attackCard, computerCards[i], trumpCard)) {
                int score = evaluateStrategicDefense(computerCards[i], attackCard, i);
                if (score > bestScore) {
                    bestScore = score;
                    bestCardIndex = i;
                }
            }
        }

        return bestCardIndex;
    }

    int evaluateAttackCard(const Card& card, int index) {
        int score = 0;

        // Предпочтение сброса ненужных карт
        score -= getCardValue(card) * 2; // Чем слабее карта, тем лучше

        // Бонус за сброс некозырных карт
        if (card.suit != trumpCard.suit) {
            score += 50;
        }

        // Штраф за использование козыря в атаке
        if (card.suit == trumpCard.suit) {
            score -= 30;
        }

        // Учет возможности подкидывания
        if (canCardBeUsedForAdding(card)) {
            score += 20;
        }

        return score;
    }

    int evaluateDefenseCard(const Card& card, const Card& attackCard, int index) {
        int score = 0;

        // Предпочтение минимальной карты для отбоя
        score -= getCardValue(card) * 3;

        // Штраф за использование козыря если есть альтернатива
        if (card.suit == trumpCard.suit && hasNonTrumpAlternative(attackCard)) {
            score -= 40;
        }

        // Бонус за использование козыря если это необходимо
        if (card.suit == trumpCard.suit && !hasNonTrumpAlternative(attackCard)) {
            score += 30;
        }

        // Сохранение сильных карт
        if (getCardValue(card) > 10) {
            score -= 25;
        }

        return score;
    }

    int evaluateStrategicDefense(const Card& card, const Card& attackCard, int index) {
        int score = 0;

        // Стратегическое сохранение карт
        score -= getCardValue(card); // Использовать weaker cards

        // Избегание использования козыря без необходимости
        if (card.suit == trumpCard.suit && hasNonTrumpAlternative(attackCard)) {
            score -= 100;
        }

        // Предпочтение карт той же масти что и атака
        if (card.suit == attackCard.suit) {
            score += 20;
        }

        return score;
    }

    int selectBestMove(const std::vector<int>& candidateIndices, bool isAttackTurn) {
        if (candidateIndices.empty()) return -1;
        if (candidateIndices.size() == 1) return candidateIndices[0];

        // Для атаки выбираем карту с наименьшей ценностью
        if (isAttackTurn) {
            int bestIndex = candidateIndices[0];
            int minValue = getCardValue(computerCards[bestIndex]);

            for (int i = 1; i < candidateIndices.size(); ++i) {
                int value = getCardValue(computerCards[candidateIndices[i]]);
                if (value < minValue) {
                    minValue = value;
                    bestIndex = candidateIndices[i];
                }
            }
            return bestIndex;
        }

        // Для защиты выбираем карту с оптимальным балансом
        int bestIndex = candidateIndices[0];
        int bestScore = evaluateDefenseCard(computerCards[bestIndex], tableCards.back(), bestIndex);

        for (int i = 1; i < candidateIndices.size(); ++i) {
            int score = evaluateDefenseCard(computerCards[candidateIndices[i]], tableCards.back(), candidateIndices[i]);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = candidateIndices[i];
            }
        }

        return bestIndex;
    }

    bool hasNonTrumpAlternative(const Card& attackCard) {
        for (const auto& card : computerCards) {
            if (card.suit != trumpCard.suit &&
                canBeatCard(attackCard, card, trumpCard)) {
                return true;
            }
        }
        return false;
    }

    bool canCardBeUsedForAdding(const Card& card) {
        // Проверяем, можно ли этой картой подкинуть на существующие карты на столе
        for (const auto& tableCard : tableCards) {
            if (tableCard.rank == card.rank) {
                return true;
            }
        }
        return false;
    }


public:
    GameLogic() : rng(std::random_device{}()), Deck(), playerCards(),
        computerCards(), tableCards(), trumpCard(), aiMutex() {
    }

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
        Deck.emplace_back(Card::BLACK, Card::JOKER_RANK, id++);
        Deck.emplace_back(Card::RED, Card::JOKER_RANK, id++);

        return Deck;
    }

    // Перемешивание колоды
    void shuffleDeck(std::vector<Card>& Deck) {
        std::shuffle(Deck.begin(), Deck.end(), rng);
    }

    // Раздача карт
    void firstdealCards(std::vector<Card>& Deck) {

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
            while (Deck.back().rank == 6)
                shuffleDeck(Deck);
            Card trump = Deck.back();
            Deck.pop_back();
            trumpCard = trump;
        }
    }

    void dealCards(std::vector<Card>& Deck) {

        while (playerCards.size() < 6 && computerCards.size() < 6 && !Deck.empty()) {
            playerCards.push_back(Deck.back());
            Deck.pop_back();
            computerCards.push_back(Deck.back());
            Deck.pop_back();
        }

        while (playerCards.size() < 6 && !Deck.empty()) {
            playerCards.push_back(Deck.back());
            Deck.pop_back();
        }

        while (computerCards.size() < 6 && !Deck.empty()) {
            computerCards.push_back(Deck.back());
            Deck.pop_back();
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

    int calculateAIMove(bool isAttackTurn, int numThreads = 2) {
        if (computerCards.empty()) return -1;

        // Если один поток или мало карт - используем простой расчет
        if (numThreads <= 1 || computerCards.size() <= 2) {
            return calculateSimpleAIMove(isAttackTurn);
        }

        return calculateMultiThreadAIMove(isAttackTurn, numThreads);
    }
    
    void computergettablecards() {
        while (tableCards.size() > 0) {
            computerCards.push_back(tableCards.back());
            tableCards.pop_back();
        }
    }

    void playergettablecards() {
        while (tableCards.size() > 0) {
            playerCards.push_back(tableCards.back());
            tableCards.pop_back();
        }
    }

    void freetablecards() {
        while (tableCards.size() > 0)
            tableCards.pop_back();
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
    int selectedCardIndex = -1;
public:
    // Конструктор, принимающий ссылку на GameTable
    MouseManager() : selectedCardIndex(-1) {}

    // Проверка, попал ли клик по карте
    int getCardAtPosition(double xpos, double ypos, std::vector<Card> playercards) {
        // ypos от верхнего левого угла -> в нижний левый
        ypos = WINDOW_HEIGHT - ypos;

        // Проверяем карты игрока (в порядке справа налево, чтобы верхние карты проверялись первыми)
        for (int i = playercards.size() - 1; i >= 0; --i) {
            if (i >= 6) continue;

            const Card& card = playercards[i];

            if (xpos >= card.position.x &&
                xpos <= card.position.x + CARD_WIDTH &&
                ypos >= card.position.y &&
                ypos <= card.position.y + CARD_HEIGHT) {
                return i; // Возвращаем индекс найденной карты
            }
        }
        
        
        
            if (xpos >= 0.0f && xpos <= 100.0f &&
               ypos >= 0.0f && ypos <= 100.0f) {
               return -2;
            }
        
        
            if (WINDOW_WIDTH - xpos >= 0.0f && WINDOW_WIDTH - xpos <= 100.0f && WINDOW_HEIGHT - ypos >= 0.0f && WINDOW_HEIGHT - ypos <= 100.0f) {
                return -3;
            }

        return -1;
    }

    void onMouseClick(double xpos, double ypos, std::vector<Card> playercards) {
        int clickedCardIndex = getCardAtPosition(xpos, ypos, playercards);
        if (clickedCardIndex >= 0) {
            std::cout << "Click on card #" << clickedCardIndex << std::endl;
            selectedCardIndex = clickedCardIndex;
            onCardSelected(clickedCardIndex, playercards);
        }
        else if (clickedCardIndex == -2) {
            selectedCardIndex = -2;
            std::cout << "Move end" << std::endl;
        }
        else if (clickedCardIndex == -3) {
            selectedCardIndex = -3;
            std::cout << "Move end" << std::endl;
        }
        else {
            selectedCardIndex = -1;
            std::cout << "Not on card click " << xpos << " " << ypos << std::endl;
        }
    }

    void onCardSelected(int cardIndex, std::vector<Card> playercards) {
        if (cardIndex >= 0 && cardIndex < static_cast<int>(playercards.size())) {
            std::cout << "Chosen card: " << playercards[cardIndex].getTextureName() << std::endl;
        }
    }
    // Метод для получения выбранной карты
    int getSelectedCardIndex() const { return selectedCardIndex; }

    // Метод для сброса выбора
    void clearSelection() { selectedCardIndex = -1; }
};

class GameTable {
private:
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
    }

    void render(std::vector<Card>& playercards, std::vector<Card>& computercards, std::vector<Card>& tablecards, Card& trump) {
        float startX = (WINDOW_WIDTH - playercards.size() * CARD_WIDTH) / 2.0f;

        // Карты игрока (низ экрана)
        float playerY = 50.0f;
        for (int i = 0; i < playercards.size(); ++i) {
            playercards[i].position = glm::vec2(
                startX + i * CARD_WIDTH,
                playerY
            );
            playercards[i].isFaceUp = true;
        }

        startX = (WINDOW_WIDTH - computercards.size() * CARD_WIDTH) / 2.0f;
        // Карты компьютера (верх экрана)
        float computerY = WINDOW_HEIGHT - 50.0f - CARD_HEIGHT;
        for (int i = 0; i < computercards.size(); ++i) {
            computercards[i].position = glm::vec2(
                startX + i * CARD_WIDTH,
                computerY
            );
            computercards[i].isFaceUp = false;
        }

        // Карты на столе (центр)
        float tableStartX = (WINDOW_WIDTH - 4 * CARD_WIDTH) / 2.0f;
        float tableY = WINDOW_HEIGHT / 2.0f - CARD_HEIGHT / 2.0f;

        for (int i = 0; i < tablecards.size() && i < 8; ++i) {
            int row = i / 4;
            int col = i % 4;
            tablecards[i].position = glm::vec2(
                tableStartX + col * CARD_WIDTH,
                tableY - row * (CARD_HEIGHT + 10)
            );
            tablecards[i].isFaceUp = true;
        }

        // Козырь
        if (trump.rank != Card::JOKER_RANK) {
            trump.position = glm::vec2(
                WINDOW_WIDTH - CARD_WIDTH - 20,
                WINDOW_HEIGHT / 2 - CARD_HEIGHT / 2
            );
            trump.isFaceUp = true;
        }

        // Очищаем экран
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Рендерим фон
        renderer.renderBackground("C:/textures/table.jpg");

        // Рендерим все карты с общей матрицей проекции
        renderer.renderCards(computercards, projectionMatrix);
        renderer.renderCards(tablecards, projectionMatrix);
        renderer.renderCards(playercards, projectionMatrix);

        // Рендерим козырь отдельно, если нужно
        if (trump.rank != Card::JOKER_RANK) {
            renderer.renderCard(trump, projectionMatrix);
        }
    }
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
        audioManager.playAudio(audioManager.loadAudio("C:/textures/Deep_Cover.mp3"));
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
            mouseManager.onMouseClick(xpos, ypos, gameLogic.getPlayerCards());
            break;
        case GameState::PLAYER_TURN_DEFEND:
            mouseManager.onMouseClick(xpos, ypos, gameLogic.getPlayerCards());
            break;
        case GameState::COMPUTER_TURN_ATTACK:
            break;
        case GameState::COMPUTER_TURN_DEFEND:
            break;
        case GameState::GAME_OVER:
            break;
        }
    }

    void updatePlayerAttack() {
        gameTable.render(gameLogic.getPlayerCards(), gameLogic.getComputerCards(), gameLogic.getTableCards(), gameLogic.getTrumpCard());
        int selectedCard = mouseManager.getSelectedCardIndex();
        if (selectedCard >= 0) {
            if (gameLogic.playerAttack(selectedCard)) {
                std::cout << "Player attacks with card #" << selectedCard << std::endl;
                currentState = GameState::COMPUTER_TURN_DEFEND;
            }
            mouseManager.clearSelection();
        }
        else if (selectedCard == -2) {
            gameLogic.freetablecards();
            gameLogic.dealCards(gameLogic.getDeck());
            currentState = GameState::COMPUTER_TURN_ATTACK;
            std::cout << "Player end move" << std::endl;
        }
    }

    void updatePlayerDefend() {
        gameTable.render(gameLogic.getPlayerCards(), gameLogic.getComputerCards(), gameLogic.getTableCards(), gameLogic.getTrumpCard());
        int selectedCard = mouseManager.getSelectedCardIndex();
        if (selectedCard >= 0) {
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
        else if (selectedCard == -3) {
            gameLogic.playergettablecards();
            gameLogic.dealCards(gameLogic.getDeck());
            std::cout << "Player end move" << std::endl;
            currentState = GameState::COMPUTER_TURN_ATTACK;
        }
    }

    void updateComputerAttack() {
        int cardIndex = gameLogic.calculateAIMove(true, 2); // 2 потока для атаки
        if (cardIndex != -1) {
            Card attackingCard = gameLogic.getComputerCards()[cardIndex];
            gameLogic.getTableCards().push_back(attackingCard);
            gameLogic.getComputerCards().erase(gameLogic.getComputerCards().begin() + cardIndex);
            std::cout << "Computer attacks with card #" << cardIndex << std::endl;
            currentState = GameState::PLAYER_TURN_DEFEND;
        }
        else {
            gameLogic.dealCards(gameLogic.getDeck());
            gameLogic.freetablecards();
            currentState = GameState::PLAYER_TURN_ATTACK;
            std::cout << "Computer end move" << std::endl;
        }
    }

    void updateComputerDefend() {
        int cardIndex = gameLogic.calculateAIMove(false, 2); // 2 потока для защиты

        if (cardIndex != -1) {
            Card& attackCard = gameLogic.getTableCards().back();
            Card defendCard = gameLogic.getComputerCards()[cardIndex];

            attackCard.isFaceUp = false;
            gameLogic.getTableCards().push_back(defendCard);
            gameLogic.getComputerCards().erase(gameLogic.getComputerCards().begin() + cardIndex);

            std::cout << "Computer defends with card #" << cardIndex << std::endl;
        }
        else {
            gameLogic.computergettablecards();
            gameLogic.dealCards(gameLogic.getDeck());
            std::cout << "Computer end move" << std::endl;
        }

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
        gameLogic.firstdealCards(gameLogic.getDeck());
        gameTable.render(gameLogic.getPlayerCards(), gameLogic.getComputerCards(), gameLogic.getTableCards(), gameLogic.getTrumpCard());
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
