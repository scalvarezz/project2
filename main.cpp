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

    unsigned int uiVAO, uiVBO, uiEBO;
    unsigned int uiShaderProgram;

    TextureManager textureManager;

    // Вершины для карт
    float cardVertices[20] = {
        // координаты (x, y, z)    // текстурные координаты
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // верхний правый
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // нижний правый 
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // нижний левый
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f   // верхний левый
    };

    unsigned int cardIndices[6] = {
        0, 1, 3,
        1, 2, 3
    };

public:
    void init() {
        initCardRendering();
        initBackgroundRendering();
        initUIRendering();
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
        
        void main()
        {
            gl_Position = projection * model * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        }
        )";

        const char* cardFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        void main()
        {
            FragColor = texture(ourTexture, TexCoord);
        }
        )";

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

        unsigned int backgroundIndices[] = {
            0, 1, 3,
            1, 2, 3
        };

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
        void main()
        {
            gl_Position = vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        }
        )";

        const char* backgroundFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D backgroundTexture;
        void main()
        {
            FragColor = texture(backgroundTexture, TexCoord);
        }
        )";

        backgroundShaderProgram = createShaderProgram(backgroundVertexShader, backgroundFragmentShader);
    }

    void initUIRendering() {
        glGenVertexArrays(1, &uiVAO);
        glGenBuffers(1, &uiVBO);
        glGenBuffers(1, &uiEBO);

        // Пока базовая инициализация, можно расширить позже
        float uiVertices[] = {
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };

        unsigned int uiIndices[] = {
            0, 1, 3,
            1, 2, 3
        };

        glBindVertexArray(uiVAO);

        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(uiVertices), uiVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uiIndices), uiIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        const char* uiVertexShader = R"(
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

        const char* uiFragmentShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uiTexture;
        uniform vec4 color;
        void main()
        {
            if (uiTexture == 0) {
                FragColor = color;
            } else {
                FragColor = texture(uiTexture, TexCoord) * color;
            }
        }
        )";

        uiShaderProgram = createShaderProgram(uiVertexShader, uiFragmentShader);
    }

public:
    // Рендеринг одной карты
    void renderCard(const Card& card, float cardWidth, float cardHeight) {
        // Создание матрицы трансформации
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(card.position.x, card.position.y, 0.0f));
        model = glm::scale(model, glm::vec3(cardWidth, cardHeight, 1.0f));

        // Матрица проекции (ортографическая)
        glm::mat4 projection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);

        // Установка шейдеров
        glUseProgram(cardShaderProgram);

        // Передача матриц в шейдер
        unsigned int modelLoc = glGetUniformLocation(cardShaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        unsigned int projLoc = glGetUniformLocation(cardShaderProgram, "projection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Загрузка текстуры
        unsigned int texture = textureManager.loadTexture(card.getTextureName());
        glBindTexture(GL_TEXTURE_2D, texture);

        // Рендеринг
        glBindVertexArray(cardVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Рендеринг множества карт
    void renderCards(const std::vector<Card>& cards, float cardWidth, float cardHeight) {
        for (const auto& card : cards) {
            renderCard(card, cardWidth, cardHeight);
        }
    }

    // Рендеринг фона
    void renderBackground(const std::string& texturePath) {
        unsigned int backgroundTexture = textureManager.loadTexture(texturePath);
        if (backgroundTexture == 0) return;

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(backgroundShaderProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        glUniform1i(glGetUniformLocation(backgroundShaderProgram, "backgroundTexture"), 0);

        glBindVertexArray(backgroundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);
    }

    // Рендеринг главного меню
    void renderMainMenu() {
        // Рендерим фон меню
        renderBackground("C:/try/test20072025/textures/menu_background.jpg");

        // Здесь можно добавить рендеринг текста меню, кнопок и т.д.
        // Пока просто заглушка
        std::cout << "Rendering main menu..." << std::endl;
    }

    // Рендеринг UI элементов
    void renderUI(const std::string& text = "", const glm::vec2& position = glm::vec2(0, 0)) {
        // Пока базовая реализация
        // Можно расширить для рендеринга текста, кнопок, счетчиков и т.д.
    }

    TextureManager& getTextureManager() {
        return textureManager;
    }
};

class GameTable {
private:
    Renderer renderer;
    std::vector<Card> playerCards;
    std::vector<Card> computerCards;
    std::vector<Card> tableCards; // на карты на столе надо будет потом поставить ограничение в условные 8-10 (?) штук
    Card trumpCard; // козырь

public:
    // Явный конструктор по умолчанию НЕ УДАЛЯТЬ!!! по сути абсолютно бесполезен (НО В НЁМ МОЖЕТ БЫТЬ ПРОБЛЕМА!!!)
    GameTable() : renderer(), playerCards(), computerCards(),
        tableCards(), trumpCard() {}
    void init() {
        renderer.init();
        setupInitialPositions();
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

        renderer.renderCards(computerCards, CARD_WIDTH, CARD_HEIGHT);
        renderer.renderCards(tableCards, CARD_WIDTH, CARD_HEIGHT);
        renderer.renderCards(playerCards, CARD_WIDTH, CARD_HEIGHT);

        if (trumpCard.suit != Card::JOKER) {
            trumpCard.position = glm::vec2(WINDOW_WIDTH - 100, WINDOW_HEIGHT / 2);
            trumpCard.isFaceUp = true;
            renderer.renderCard(trumpCard, CARD_WIDTH, CARD_HEIGHT);
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


/*
KeyboardManager keyManager;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) { // это зачем ваще?
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        keyManager.onMouseClick(xpos, ypos);
    }
}
*/

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
    GameLogic gameLogic;
    AudioManager audioManager;
    KeyboardManager KeyManager; 
    Renderer renderer; 
    ALuint backgroundMusic;
    GameState currentState;

public:
    Game() : window(nullptr), gameTable(), gameLogic(gameTable),
        KeyManager(gameTable), renderer(),
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
        renderer.init(); 
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

        return true;
    }

    void run() {
        while (!glfwWindowShouldClose(window)) {
            update(); // весь рендер надо будет засунуть в апдейт и связанные с ним функции

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

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
            renderer.renderMainMenu();
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
        case GameState::MAIN_MENU:
            handleMainMenuClick(xpos, ypos);
            break;
        case GameState::PLAYER_TURN_ATTACK:
            KeyManager.onMouseClick(xpos, ypos);
            break;
        case GameState::PLAYER_TURN_DEFEND:
            KeyManager.onMouseClick(xpos, ypos);
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
        renderer.renderBackground("C:/try/test20072025/textures/table.jpg"); // такого рода, тут ещё каждая функция недописана для рендера
        int selectedCard = KeyManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            if (gameLogic.playerAttack(selectedCard)) {
                std::cout << "Игрок атакует картой #" << selectedCard << std::endl;
                currentState = GameState::COMPUTER_TURN_DEFEND;
            }
            KeyManager.clearSelection();
        }
    }

    void updatePlayerDefend() {
        int selectedCard = KeyManager.getSelectedCardIndex();
        if (selectedCard != -1) {
            auto& tableCards = gameTable.getTableCards();
            if (!tableCards.empty()) {
                int attackCardIndex = static_cast<int>(tableCards.size()) - 1;
                if (gameLogic.playerDefend(attackCardIndex, selectedCard)) {
                    std::cout << "Игрок защищается картой #" << selectedCard << std::endl;
                    currentState = GameState::COMPUTER_TURN_ATTACK;
                }
            }
            KeyManager.clearSelection();
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

    void handleMainMenuClick(double xpos, double ypos) {
        currentState = GameState::PLAYER_TURN_ATTACK;
    }

    void restartGame() {
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
