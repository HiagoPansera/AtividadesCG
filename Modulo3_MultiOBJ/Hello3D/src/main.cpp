// ============================================================
// Modulo 3 - Desafio: Multiplos modelos OBJ com selecao e transformacoes
// Autor: Hiago Pansera
// ============================================================

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
const float PI = 3.14159265358979f;

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

Mat4 scaleM(float sx, float sy, float sz) {
    Mat4 r = Mat4::identity();
    r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
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
// Struct OBJ: geometria + estado de transformacao individual
// ============================================================
struct ObjModel {
    GLuint VAO      = 0;
    int    nVerts   = 0;
    std::string name;

    // Translacao
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    // Rotacao (acumulada em radianos)
    float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
    // Escala por eixo
    float sclX = 1.0f, sclY = 1.0f, sclZ = 1.0f;

    // Cor de exibicao (sem textura)
    float r = 1.0f, g = 1.0f, b = 1.0f;
};

// ============================================================
// Globals de estado da aplicacao
// ============================================================
std::vector<ObjModel> objects;
int selectedObj = 0;

enum TransformMode { TRANSLATE, ROTATE, SCALE };
TransformMode mode = TRANSLATE;

// Paleta de cores para os objetos
const float COLORS[][3] = {
    {0.2f, 0.8f, 1.0f},  // ciano
    {1.0f, 0.5f, 0.1f},  // laranja
    {0.3f, 0.9f, 0.3f},  // verde
    {0.9f, 0.2f, 0.8f},  // roxo
    {0.9f, 0.9f, 0.2f},  // amarelo
    {0.9f, 0.3f, 0.3f},  // vermelho
};
const int NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);

// ============================================================
// Leitura de arquivo OBJ (sem textura, apenas posicoes)
// Suporta faces com 3+ vertices (triangulacao em leque)
// Suporta indices negativos (relativos)
// ============================================================
GLuint loadOBJ(const std::string& path, int& nVertices) {
    struct Vec3 { float x, y, z; };
    std::vector<Vec3> positions;
    std::vector<float> vBuffer;

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "[OBJ] Erro ao abrir: " << path << std::endl;
        nVertices = 0;
        return 0;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v") {
            Vec3 v;
            ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (token == "f") {
            std::vector<int> faceIdx;
            std::string word;
            while (ss >> word) {
                // Extrai apenas o indice de vertice (antes da primeira '/')
                size_t slash = word.find('/');
                int vi = std::stoi(word.substr(0, slash));
                // Converte: positivo = 1-indexado; negativo = relativo
                if (vi > 0) vi -= 1;
                else        vi  = (int)positions.size() + vi;
                faceIdx.push_back(vi);
            }
            // Triangulacao em leque a partir do primeiro vertice
            for (int i = 1; i + 1 < (int)faceIdx.size(); i++) {
                int idx[3] = { faceIdx[0], faceIdx[i], faceIdx[i+1] };
                for (int k = 0; k < 3; k++) {
                    vBuffer.push_back(positions[idx[k]].x);
                    vBuffer.push_back(positions[idx[k]].y);
                    vBuffer.push_back(positions[idx[k]].z);
                }
            }
        }
    }

    if (vBuffer.empty()) {
        std::cerr << "[OBJ] Nenhum vertice carregado em: " << path << std::endl;
        nVertices = 0;
        return 0;
    }

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(float), vBuffer.data(), GL_STATIC_DRAW);

    // Atributo 0: posicao (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)(vBuffer.size() / 3);
    std::cout << "[OBJ] Carregado: " << path << " | " << nVertices << " vertices" << std::endl;
    return VAO;
}

// ============================================================
// Compilacao de shaders
// ============================================================
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Shader] Erro ao abrir: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileAndLinkShaders() {
    std::string vertCode = loadShaderSource("shaders/shader.vert");
    std::string fragCode = loadShaderSource("shaders/shader.frag");
    const char* vSrc = vertCode.c_str();
    const char* fSrc = fragCode.c_str();

    int success; char log[512];

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vSrc, nullptr);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(vert, 512, nullptr, log); std::cerr << "Vert: " << log << std::endl; }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fSrc, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(frag, 512, nullptr, log); std::cerr << "Frag: " << log << std::endl; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) { glGetProgramInfoLog(prog, 512, nullptr, log); std::cerr << "Link: " << log << std::endl; }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ============================================================
// Callbacks
// ============================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

const char* modeName() {
    switch (mode) {
        case TRANSLATE: return "TRANSLACAO  (A/D=X  W/seta=Z  I/J=Y)";
        case ROTATE:    return "ROTACAO     (X / Y / Z  |  Shift = inverso)";
        case SCALE:     return "ESCALA      ([ / ] uniforme  |  X/Y/Z eixo  |  Shift = diminuir)";
    }
    return "";
}

void printStatus() {
    if (objects.empty()) return;
    std::cout << "Objeto: [" << selectedObj + 1 << "/" << objects.size() << "] "
              << objects[selectedObj].name
              << "  |  Modo: " << modeName() << std::endl;
}

// Eventos de tecla: selecao de objeto e troca de modo (disparo unico)
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, true);

    // TAB: cicla para o proximo objeto
    if (key == GLFW_KEY_TAB && !objects.empty()) {
        selectedObj = (selectedObj + 1) % (int)objects.size();
        printStatus();
    }

    // T / R / S: muda o modo de transformacao
    if (key == GLFW_KEY_T) { mode = TRANSLATE; printStatus(); }
    if (key == GLFW_KEY_R) { mode = ROTATE;    printStatus(); }
    if (key == GLFW_KEY_S) { mode = SCALE;     printStatus(); }
}

// Transformacoes continuas (mantendo a tecla pressionada)
void processInput(GLFWwindow* window, float dt) {
    if (objects.empty()) return;
    ObjModel& obj = objects[selectedObj];

    const float MOVE = 2.0f;
    const float ROT  = 2.0f;
    const float SCL  = 1.0f;
    const float MIN_SCL = 0.05f;

    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    if (mode == TRANSLATE) {
        // Eixo X: A / D  ou  seta esquerda / direita
        if (glfwGetKey(window, GLFW_KEY_A)     == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) obj.posX -= MOVE * dt;
        if (glfwGetKey(window, GLFW_KEY_D)     == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) obj.posX += MOVE * dt;

        // Eixo Z: W / seta cima = frente; seta baixo = atras
        if (glfwGetKey(window, GLFW_KEY_W)  == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) obj.posZ -= MOVE * dt;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) obj.posZ += MOVE * dt;

        // Eixo Y: I = sobe, J = desce
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) obj.posY += MOVE * dt;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) obj.posY -= MOVE * dt;
    }
    else if (mode == ROTATE) {
        // X / Y / Z: rotaciona no eixo; Shift inverte o sentido
        float dir = shift ? -1.0f : 1.0f;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) obj.rotX += dir * ROT * dt;
        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) obj.rotY += dir * ROT * dt;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) obj.rotZ += dir * ROT * dt;
    }
    else if (mode == SCALE) {
        // Escala uniforme: ] aumenta, [ diminui  (ou setas UP/DOWN)
        bool scaleUp   = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_UP)            == GLFW_PRESS;
        bool scaleDown = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_DOWN)          == GLFW_PRESS;

        if (scaleUp) {
            obj.sclX += SCL * dt;
            obj.sclY += SCL * dt;
            obj.sclZ += SCL * dt;
        }
        if (scaleDown) {
            obj.sclX = std::max(MIN_SCL, obj.sclX - SCL * dt);
            obj.sclY = std::max(MIN_SCL, obj.sclY - SCL * dt);
            obj.sclZ = std::max(MIN_SCL, obj.sclZ - SCL * dt);
        }

        // Escala por eixo individual: X / Y / Z; Shift diminui
        float dir = shift ? -1.0f : 1.0f;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
            obj.sclX = std::max(MIN_SCL, obj.sclX + dir * SCL * dt);
        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
            obj.sclY = std::max(MIN_SCL, obj.sclY + dir * SCL * dt);
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
            obj.sclZ = std::max(MIN_SCL, obj.sclZ + dir * SCL * dt);
    }
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
        "Hello3D - Multi-OBJ - Hiago Pansera", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar janela GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = compileAndLinkShaders();

    // --------------------------------------------------------
    // Lista de arquivos OBJ a carregar
    // Coloque os arquivos .obj na pasta assets/ do build
    // Modelos recomendados: suzanne.obj, cube.obj, etc.
    // --------------------------------------------------------
    const char* objFiles[] = {
        "assets/suzanne.obj",
        "assets/cube.obj",
        // Adicione mais arquivos aqui, ex: "assets/teapot.obj"
    };
    const int numFiles = sizeof(objFiles) / sizeof(objFiles[0]);

    for (int i = 0; i < numFiles; i++) {
        ObjModel obj;

        obj.VAO = loadOBJ(objFiles[i], obj.nVerts);
        if (obj.VAO == 0) continue;

        // Nome amigavel: apenas o arquivo sem o diretorio
        obj.name = objFiles[i];
        size_t slash = obj.name.find_last_of("/\\");
        if (slash != std::string::npos)
            obj.name = obj.name.substr(slash + 1);

        // Cor unica por objeto
        int ci = i % NUM_COLORS;
        obj.r = COLORS[ci][0];
        obj.g = COLORS[ci][1];
        obj.b = COLORS[ci][2];

        // Posicao inicial: distribui horizontalmente para nao sobrepor
        obj.posX = (float)(i - (numFiles - 1) * 0.5f) * 3.0f;

        objects.push_back(obj);
    }

    if (objects.empty()) {
        std::cerr << "\nNenhum OBJ carregado. Coloque arquivos .obj em assets/ e recompile." << std::endl;
        glfwTerminate();
        return -1;
    }

    // Localizacoes dos uniforms
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");
    int colorLoc = glGetUniformLocation(shaderProgram, "objColor");

    // Instrucoes no console
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Hello3D - Multi-OBJ com Selecao ===" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "TAB        : selecionar proximo objeto" << std::endl;
    std::cout << "T          : modo Translacao" << std::endl;
    std::cout << "R          : modo Rotacao" << std::endl;
    std::cout << "S          : modo Escala" << std::endl;
    std::cout << "--- Translacao ---" << std::endl;
    std::cout << "A/D ou </> : eixo X" << std::endl;
    std::cout << "W / seta   : eixo Z (frente/tras)" << std::endl;
    std::cout << "I / J      : eixo Y (cima/baixo)" << std::endl;
    std::cout << "--- Rotacao ---" << std::endl;
    std::cout << "X / Y / Z  : rotacionar no eixo (+Shift = inverso)" << std::endl;
    std::cout << "--- Escala ---" << std::endl;
    std::cout << "] / [      : escala uniforme (setas UP/DOWN tambem)" << std::endl;
    std::cout << "X/Y/Z      : escala por eixo (+Shift = diminuir)" << std::endl;
    std::cout << "ESC        : sair" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Objetos com brilho CHEIO = selecionado | ESCURO = nao selecionado" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    printStatus();

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window, dt);

        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Camera fixa levemente acima e atras da origem
        Mat4 view = lookAt(0.0f, 3.0f, 8.0f,
                           0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f);
        Mat4 proj = perspective(45.0f * PI / 180.0f, 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);

        for (int i = 0; i < (int)objects.size(); i++) {
            ObjModel& obj = objects[i];

            // Objeto selecionado: cor cheia; nao selecionado: escurecido
            float brightness = (i == selectedObj) ? 1.0f : 0.35f;
            glUniform3f(colorLoc, obj.r * brightness, obj.g * brightness, obj.b * brightness);

            // Modelo: Translate * RotX * RotY * RotZ * Scale
            Mat4 model = mul(translate(obj.posX, obj.posY, obj.posZ),
                        mul(rotateX(obj.rotX),
                        mul(rotateY(obj.rotY),
                        mul(rotateZ(obj.rotZ),
                            scaleM(obj.sclX, obj.sclY, obj.sclZ)))));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glBindVertexArray(obj.VAO);
            glDrawArrays(GL_TRIANGLES, 0, obj.nVerts);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& obj : objects)
        glDeleteVertexArrays(1, &obj.VAO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
