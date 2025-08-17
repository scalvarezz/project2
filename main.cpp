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
        HEARTS,         // Ракеты - армия - черви
        DIAMONDS,       // Звёзды - пропаганда - бубны
        CLUBS,          // Астероиды - разрушение - трефы
        JOKER           // Коммунизм - джокер - для обозначения его цвета, но тут ещё надо подумать, как это сделать не так по-уебански
    };

    enum Rank {
        TWO = 2,        // Народные массы
        TEN = 10,       // Пионер
        JACK = 11,      // Октябрёнок
        QUEEN = 12,     // Комсомолка
        KING = 13,      // Вождь
        ACE = 14,       // Революция
        JOKER_RANK = 15 // Коммунизм
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

        return "C:/try/test20072025/textures/" + rankStr + "_" + suitStr + ".png";
    }
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

class CardManager {
private:
    std::vector<Card> playerCards;
    std::vector<Card> computerCards;
    std::vector<Card> tableCards;
    int selectedCardIndex = -1;

public:
    // Проверка, попал ли клик по карте
    int getCardAtPosition(double xpos, double ypos) {
        // ypos от верхнего левого угла -> в нижний левый
        ypos = WINDOW_HEIGHT - ypos;

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

        return -1; // где карта
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
            std::cout << "Клик мимо карт" << std::endl; // лол
        }
    }

    void onCardSelected(int cardIndex) {
        // возможно здесь будет логика игры TODO?
        std::cout << "Выбрана карта: " << playerCards[cardIndex].getTextureName() << std::endl;
    }
};

CardManager cardManager;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        cardManager.onMouseClick(xpos, ypos);
    }
}

int main() { // это возможно будет вынесена в класс GameConstructor или вроде того
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Space Side Suicide", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    AudioManager audioManager;
    ALuint musicBuffer = audioManager.loadAudio("C:/try/test20072025/textures/Deep_Cover.mp3");
    if (musicBuffer == 0) {
        std::cout << "Warning: Failed to load background music" << std::endl;
    }
    else {
        ALuint musicSource = audioManager.playAudio(musicBuffer);
        alSourcei(musicSource, AL_LOOPING, AL_TRUE);
        audioManager.setVolume(musicSource, 0.5f);

        std::cout << "Background music started playing" << std::endl;
    }

    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    GameTable gameTable;
    gameTable.init();

    // Основной цикл
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) // TODO: тут ещё недописана обработка ввода, он просто пока понимает, на что нажали (наверное понимает)
            glfwSetWindowShouldClose(window, true);

        gameTable.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}