#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

// ============================================================
// Matriz 4x4 column-major (convencao OpenGL)
// ============================================================
struct Mat4 {
    float m[16];
    Mat4() { for (int i = 0; i < 16; i++) m[i] = 0.0f; }
    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
};

Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                r.m[c*4+row] += a.m[k*4+row] * b.m[c*4+k];
    return r;
}

Mat4 translate(float tx, float ty, float tz) {
    Mat4 r = Mat4::identity();
    r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
    return r;
}

Mat4 scaleM(float s) {
    Mat4 r = Mat4::identity();
    r.m[0] = r.m[5] = r.m[10] = s;
    return r;
}

Mat4 rotateX(float a) {
    Mat4 r = Mat4::identity();
    r.m[5]  =  cosf(a); r.m[9]  = -sinf(a);
    r.m[6]  =  sinf(a); r.m[10] =  cosf(a);
    return r;
}

Mat4 rotateY(float a) {
    Mat4 r = Mat4::identity();
    r.m[0]  =  cosf(a); r.m[8]  =  sinf(a);
    r.m[2]  = -sinf(a); r.m[10] =  cosf(a);
    return r;
}

Mat4 rotateZ(float a) {
    Mat4 r = Mat4::identity();
    r.m[0] =  cosf(a); r.m[4] = -sinf(a);
    r.m[1] =  sinf(a); r.m[5] =  cosf(a);
    return r;
}

Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
    Mat4 r;
    float f = 1.0f / tanf(fovY * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

Mat4 lookAt(float ex, float ey, float ez,
            float cx, float cy, float cz,
            float upx, float upy, float upz) {
    float fx = cx-ex, fy = cy-ey, fz = cz-ez;
    float fl = sqrtf(fx*fx+fy*fy+fz*fz);
    fx/=fl; fy/=fl; fz/=fl;

    float rx = fy*upz - fz*upy;
    float ry = fz*upx - fx*upz;
    float rz = fx*upy - fy*upx;
    float rl = sqrtf(rx*rx+ry*ry+rz*rz);
    rx/=rl; ry/=rl; rz/=rl;

    float ux = ry*fz - rz*fy;
    float uy = rz*fx - rx*fz;
    float uz = rx*fy - ry*fx;

    Mat4 r;
    r.m[0] = rx;  r.m[4] = ry;  r.m[8]  = rz;  r.m[12] = -(rx*ex + ry*ey + rz*ez);
    r.m[1] = ux;  r.m[5] = uy;  r.m[9]  = uz;  r.m[13] = -(ux*ex + uy*ey + uz*ez);
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz; r.m[14] =  (fx*ex + fy*ey + fz*ez);
    r.m[15] = 1.0f;
    return r;
}

// ============================================================
// Estado do objeto controlavel
// ============================================================
static float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
static float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
static float scl  = 1.0f;

const float MOVE_SPEED  = 2.0f;
const float ROT_SPEED   = 2.0f;
const float SCALE_SPEED = 1.0f;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) rotX += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) rotY += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) rotZ += ROT_SPEED * dt;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) posX -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) posX += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) posZ -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) posZ += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) posY += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) posY -= MOVE_SPEED * dt;

    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        scl = std::max(0.1f, scl - SCALE_SPEED * dt);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        scl = std::min(5.0f, scl + SCALE_SPEED * dt);
}

// ============================================================
// Compilacao dos shaders
// ============================================================
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileAndLinkShaders() {
    std::string vertexCode   = loadShaderSource("shaders/shader.vert");
    std::string fragmentCode = loadShaderSource("shaders/shader.frag");

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    int success;
    char infoLog[512];

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, nullptr);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader error: " << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader error: " << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program link error: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

// ============================================================
// Leitura do arquivo MTL — retorna o nome do arquivo de textura
// ============================================================
std::string loadMTL(const std::string& mtlPath) {
    std::ifstream file(mtlPath.c_str());
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir MTL: " << mtlPath << std::endl;
        return "";
    }
    std::string line, textureName;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string keyword;
        ss >> keyword;
        if (keyword == "map_Kd") {
            ss >> textureName;
            break;
        }
    }
    return textureName;
}

// ============================================================
// Leitura do arquivo OBJ com coordenadas de textura
// Buffer: x, y, z, s, t  (5 floats por vertice)
// Retorna o VAO gerado; nVertices e o nome da textura via referencia
// ============================================================
GLuint loadSimpleOBJ(const std::string& filePath, int& nVertices, std::string& textureFile) {
    std::vector<float[3]> verticesList;  // posicoes
    std::vector<float[2]> texCoordsList; // coordenadas de textura
    std::vector<float>    vBuffer;       // buffer final intercalado

    // Para armazenar os vetores dinamicamente usamos glm-style manual
    struct Vec3 { float x, y, z; };
    struct Vec2 { float s, t; };

    std::vector<Vec3> positions;
    std::vector<Vec2> texCoords;

    std::string mtlFile;

    std::ifstream arqEntrada(filePath.c_str());
    if (!arqEntrada.is_open()) {
        std::cerr << "Erro ao abrir OBJ: " << filePath << std::endl;
        nVertices = 0;
        return 0;
    }

    // Diretorio base do OBJ (para localizar o MTL e a textura)
    std::string baseDir;
    size_t sep = filePath.find_last_of("/\\");
    if (sep != std::string::npos)
        baseDir = filePath.substr(0, sep + 1);

    std::string line;
    while (std::getline(arqEntrada, line)) {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;

        if (word == "mtllib") {
            ssline >> mtlFile;
        }
        else if (word == "v") {
            Vec3 v;
            ssline >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (word == "vt") {
            Vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "f") {
            // Cada token de face: vi/ti ou vi/ti/ni
            while (ssline >> word) {
                int vi = 0, ti = 0;
                std::istringstream ss(word);
                std::string index;

                if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index, '/')) ti = !index.empty() ? std::stoi(index) - 1 : 0;

                // Posicao
                vBuffer.push_back(positions[vi].x);
                vBuffer.push_back(positions[vi].y);
                vBuffer.push_back(positions[vi].z);

                // Coordenada de textura
                if (ti >= 0 && ti < (int)texCoords.size()) {
                    vBuffer.push_back(texCoords[ti].s);
                    vBuffer.push_back(texCoords[ti].t);
                } else {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }
            }
        }
    }
    arqEntrada.close();

    // Leitura do MTL para obter o nome da textura
    if (!mtlFile.empty()) {
        std::string texName = loadMTL(baseDir + mtlFile);
        if (!texName.empty())
            textureFile = baseDir + texName;
    }

    // Configuracao do VAO/VBO
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(float), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0: posicao (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: coordenada de textura (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 5);
    return VAO;
}

// ============================================================
// Carregamento de textura com stb_image
// ============================================================
GLuint loadTexture(const std::string& path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        std::cout << "Textura carregada: " << path
                  << " (" << width << "x" << height << ", " << nrChannels << " canais)" << std::endl;
    } else {
        std::cerr << "Falha ao carregar textura: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// ============================================================
// Main
// ============================================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "Hello3D - Texturizado - Hiago Pansera", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = compileAndLinkShaders();

    // Carrega o OBJ (shaders e assets estao copiados para build/)
    int nVertices = 0;
    std::string textureFile;
    GLuint objVAO = loadSimpleOBJ("assets/cube.obj", nVertices, textureFile);

    // Carrega a textura indicada pelo MTL
    GLuint texID = 0;
    if (!textureFile.empty())
        texID = loadTexture(textureFile);

    // Localizacoes dos uniforms
    int modelLoc   = glGetUniformLocation(shaderProgram, "model");
    int viewLoc    = glGetUniformLocation(shaderProgram, "view");
    int projLoc    = glGetUniformLocation(shaderProgram, "projection");
    int tex1Loc    = glGetUniformLocation(shaderProgram, "texture1");

    std::cout << "=== Controles ===" << std::endl;
    std::cout << "X / Y / Z  : rotacionar nos eixos X, Y, Z" << std::endl;
    std::cout << "A / D      : transladar no eixo X" << std::endl;
    std::cout << "W / S      : transladar no eixo Z" << std::endl;
    std::cout << "I / J      : transladar no eixo Y" << std::endl;
    std::cout << "[ / ]      : diminuir / aumentar escala" << std::endl;
    std::cout << "ESC        : sair" << std::endl;

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        processInput(window, dt);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Ativa textura na unidade 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        glUniform1i(tex1Loc, 0);

        // Camera
        Mat4 view = lookAt(0.0f, 2.5f, 6.0f,
                           0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f);
        Mat4 proj = perspective(
            45.0f * 3.14159265f / 180.0f,
            800.0f / 600.0f,
            0.1f, 100.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);

        glBindVertexArray(objVAO);

        // Objeto controlado pelo teclado
        Mat4 model = mul(translate(posX, posY, posZ),
                    mul(rotateX(rotX),
                    mul(rotateY(rotY),
                    mul(rotateZ(rotZ),
                        scaleM(scl)))));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
        glDrawArrays(GL_TRIANGLES, 0, nVertices);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &objVAO);
    glDeleteTextures(1, &texID);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
