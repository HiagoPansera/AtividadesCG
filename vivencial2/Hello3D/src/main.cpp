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

#include "Camera.h"

// ============================================================
// Estado global da camera
// ============================================================
static Camera camera(Vec3(0.f, 2.f, 8.f), -90.f, -10.f, 5.f, 0.1f);
static bool  firstMouse = true;
static float lastX = 400.f, lastY = 300.f;

// ============================================================
// Estado do objeto principal
// ============================================================
static float posX = 0.f, posY = 0.f, posZ = 0.f;
static float rotX = 0.f, rotY = 0.f, rotZ = 0.f;
static float scl  = 1.f;

const float MOVE_SPEED  = 2.f;
const float ROT_SPEED   = 1.5f;
const float SCALE_SPEED = 1.f;

// ============================================================
// Estado das 3 luzes (ligada/desligada)
// ============================================================
// [0] Key Light | [1] Fill Light | [2] Back Light
static bool lightEnabled[3] = {true, true, true};

// Cores calibradas por funcao:
//   Key  : branco quente, intensidade maxima
//   Fill : azul frio, intensidade ~40%
//   Back : branco frio, intensidade ~70%
static const float lightColors[3][3] = {
    {1.00f, 0.90f, 0.75f},   // Key  - quente
    {0.40f, 0.42f, 0.55f},   // Fill - frio e suave
    {0.70f, 0.72f, 0.90f},   // Back - frio, rim effect
};

// Indicadores visuais das luzes (cores dos cubinhos)
static const float indicatorColors[3][3] = {
    {1.0f, 0.9f, 0.3f},   // Key  - amarelo
    {0.3f, 0.6f, 1.0f},   // Fill - azul
    {0.9f, 0.9f, 1.0f},   // Back - branco
};

// ============================================================
// Calcula as 3 posicoes de luz a partir do objeto
// ============================================================
//  Key  : frente-esquerda, elevada      (mais intensa)
//  Fill : frente-direita, menos elevada (suave, preenche sombras)
//  Back : atras, elevada               (separa objeto do fundo)
void computeLightPositions(float px, float py, float pz, float s, Vec3 out[3]) {
    out[0] = Vec3(px - 1.8f*s,  py + 1.6f*s,  pz + 2.5f*s);  // Key
    out[1] = Vec3(px + 2.2f*s,  py + 1.0f*s,  pz + 2.0f*s);  // Fill
    out[2] = Vec3(px + 0.0f,    py + 1.8f*s,  pz - 2.8f*s);  // Back
}

// ============================================================
// Callbacks
// ============================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    float fx = (float)xpos, fy = (float)ypos;
    if (firstMouse) { lastX = fx; lastY = fy; firstMouse = false; }
    camera.rotate(fx - lastX, -(fy - lastY));
    lastX = fx; lastY = fy;
}

void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_1) {
        lightEnabled[0] = !lightEnabled[0];
        std::cout << "Key Light  (1): " << (lightEnabled[0] ? "ON" : "OFF") << "\n";
    }
    if (key == GLFW_KEY_2) {
        lightEnabled[1] = !lightEnabled[1];
        std::cout << "Fill Light (2): " << (lightEnabled[1] ? "ON" : "OFF") << "\n";
    }
    if (key == GLFW_KEY_3) {
        lightEnabled[2] = !lightEnabled[2];
        std::cout << "Back Light (3): " << (lightEnabled[2] ? "ON" : "OFF") << "\n";
    }
}

// ============================================================
// Input continuo
// ============================================================
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera
    float fwd = 0.f, side = 0.f, vert = 0.f;
    if (glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS) fwd  += 1.f;
    if (glfwGetKey(window, GLFW_KEY_S)            == GLFW_PRESS) fwd  -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_A)            == GLFW_PRESS) side -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_D)            == GLFW_PRESS) side += 1.f;
    if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) vert += 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vert -= 1.f;
    camera.move(fwd, side, vert, dt);

    // Rotacao do objeto
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) rotX += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) rotY += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) rotZ += ROT_SPEED * dt;

    // Translacao do objeto
    if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) posX -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) posX += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) posZ -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) posZ += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_I)     == GLFW_PRESS) posY += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_J)     == GLFW_PRESS) posY -= MOVE_SPEED * dt;

    // Escala
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS)
        scl = std::max(0.1f, scl - SCALE_SPEED * dt);
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        scl = std::min(5.f,  scl + SCALE_SPEED * dt);
}

// ============================================================
// Shaders
// ============================================================
std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
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

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vSrc, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(vs, 512, nullptr, log); std::cerr << "VS: " << log << "\n"; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fSrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(fs, 512, nullptr, log); std::cerr << "FS: " << log << "\n"; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) { glGetProgramInfoLog(prog, 512, nullptr, log); std::cerr << "Link: " << log << "\n"; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ============================================================
// Material e OBJ
// ============================================================
struct Material {
    float Ka[3] = {0.15f, 0.15f, 0.15f};
    float Kd[3] = {0.8f,  0.8f,  0.8f };
    float Ks[3] = {1.0f,  1.0f,  1.0f };
    float Ns    = 64.f;
    std::string textureName;
};

Material loadMTL(const std::string& path) {
    Material mat;
    std::ifstream f(path.c_str());
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line); std::string kw; ss >> kw;
        if      (kw == "Ka")     ss >> mat.Ka[0] >> mat.Ka[1] >> mat.Ka[2];
        else if (kw == "Kd")     ss >> mat.Kd[0] >> mat.Kd[1] >> mat.Kd[2];
        else if (kw == "Ks")     ss >> mat.Ks[0] >> mat.Ks[1] >> mat.Ks[2];
        else if (kw == "Ns")     ss >> mat.Ns;
        else if (kw == "map_Kd") ss >> mat.textureName;
    }
    return mat;
}

GLuint loadOBJ(const std::string& filePath, int& nVerts, Material& mat) {
    struct Pos { float x, y, z; };
    struct UV  { float s, t; };
    struct Nrm { float x, y, z; };

    std::vector<Pos> positions;
    std::vector<UV>  texCoords;
    std::vector<Nrm> normals;
    std::vector<float> buf;
    std::string mtlFile;

    size_t sep = filePath.find_last_of("/\\");
    std::string baseDir = (sep != std::string::npos) ? filePath.substr(0, sep+1) : "";

    std::ifstream f(filePath.c_str());
    if (!f.is_open()) { std::cerr << "OBJ nao encontrado: " << filePath << "\n"; nVerts=0; return 0; }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line); std::string w; ss >> w;
        if      (w == "mtllib") { ss >> mtlFile; }
        else if (w == "v")  { Pos p; ss>>p.x>>p.y>>p.z; positions.push_back(p); }
        else if (w == "vt") { UV  t; ss>>t.s>>t.t;       texCoords.push_back(t); }
        else if (w == "vn") { Nrm n; ss>>n.x>>n.y>>n.z;  normals.push_back(n);   }
        else if (w == "f") {
            while (ss >> w) {
                int vi=0, ti=0, ni=0;
                std::istringstream is(w); std::string idx;
                if (std::getline(is, idx, '/')) vi = !idx.empty() ? std::stoi(idx)-1 : 0;
                if (std::getline(is, idx, '/')) ti = !idx.empty() ? std::stoi(idx)-1 : 0;
                if (std::getline(is, idx))      ni = !idx.empty() ? std::stoi(idx)-1 : 0;
                buf.push_back(positions[vi].x); buf.push_back(positions[vi].y); buf.push_back(positions[vi].z);
                if (ti>=0&&ti<(int)texCoords.size()) { buf.push_back(texCoords[ti].s); buf.push_back(texCoords[ti].t); }
                else { buf.push_back(0.f); buf.push_back(0.f); }
                if (ni>=0&&ni<(int)normals.size()) { buf.push_back(normals[ni].x); buf.push_back(normals[ni].y); buf.push_back(normals[ni].z); }
                else { buf.push_back(0.f); buf.push_back(1.f); buf.push_back(0.f); }
            }
        }
    }
    if (!mtlFile.empty()) mat = loadMTL(baseDir + mtlFile);

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buf.size()*sizeof(float), buf.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (GLvoid*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (GLvoid*)(5*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    nVerts = (int)(buf.size() / 8);
    return VAO;
}

GLuint loadTexture(const std::string& path) {
    GLuint id; glGenTextures(1, &id);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch==4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        std::cout << "Textura: " << path << " (" << w << "x" << h << ")\n";
    } else {
        std::cerr << "Falha ao carregar textura: " << path << "\n";
    }
    stbi_image_free(data);
    return id;
}

// ============================================================
// Helpers para enviar uniforms de array ao shader
// ============================================================
static int lPosLoc[3], lColLoc[3], lEnLoc[3];

void setupLightUniforms(GLuint shader) {
    for (int i = 0; i < 3; i++) {
        char buf[64];
        std::sprintf(buf, "lightPos[%d]",     i); lPosLoc[i] = glGetUniformLocation(shader, buf);
        std::sprintf(buf, "lightColor[%d]",   i); lColLoc[i] = glGetUniformLocation(shader, buf);
        std::sprintf(buf, "lightEnabled[%d]", i); lEnLoc[i]  = glGetUniformLocation(shader, buf);
    }
}

void sendLightUniforms(const Vec3 lpos[3]) {
    for (int i = 0; i < 3; i++) {
        glUniform3f(lPosLoc[i], lpos[i].x, lpos[i].y, lpos[i].z);
        glUniform3fv(lColLoc[i], 1, lightColors[i]);
        glUniform1i(lEnLoc[i], lightEnabled[i] ? 1 : 0);
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
        "Hello3D - Iluminacao 3 Pontos - Hiago Pansera", nullptr, nullptr);
    if (!window) { std::cerr << "Falha ao criar janela GLFW\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);

    GLuint shader = compileAndLinkShaders();

    // Locais dos uniforms fixos
    int modelLoc    = glGetUniformLocation(shader, "model");
    int viewLoc     = glGetUniformLocation(shader, "view");
    int projLoc     = glGetUniformLocation(shader, "projection");
    int tex1Loc     = glGetUniformLocation(shader, "texture1");
    int viewPosLoc  = glGetUniformLocation(shader, "viewPos");
    int kaLoc       = glGetUniformLocation(shader, "Ka");
    int kdLoc       = glGetUniformLocation(shader, "Kd");
    int ksLoc       = glGetUniformLocation(shader, "Ks");
    int nsLoc       = glGetUniformLocation(shader, "Ns");
    int useFlatLoc  = glGetUniformLocation(shader, "useFlatColor");
    int flatColLoc  = glGetUniformLocation(shader, "flatColor");

    setupLightUniforms(shader);

    // Carregar OBJ e textura
    int nVerts = 0;
    Material mat;
    GLuint vao = loadOBJ("assets/cube.obj", nVerts, mat);
    GLuint tex = 0;
    if (!mat.textureName.empty())
        tex = loadTexture("assets/" + mat.textureName);

    const float PI = 3.14159265f;
    float lastTime = (float)glfwGetTime();

    std::cout << "\n=== Iluminacao de 3 Pontos ===\n";
    std::cout << "W/A/S/D + Mouse  : mover camera\n";
    std::cout << "Espaco / Ctrl    : subir / descer camera\n";
    std::cout << "X / Y / Z        : rotacionar objeto\n";
    std::cout << "Setas / I / J    : transladar objeto (XZ / Y)\n";
    std::cout << "[ / ]            : diminuir / aumentar escala\n";
    std::cout << "1                : toggle Key Light  (principal)\n";
    std::cout << "2                : toggle Fill Light (preenchimento)\n";
    std::cout << "3                : toggle Back Light (fundo)\n";
    std::cout << "ESC              : sair\n\n";
    std::cout << "As luzes se reposicionam automaticamente com o objeto.\n";
    std::cout << "Indicadores: amarelo=Key, azul=Fill, branco=Back\n\n";

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window, dt);

        // Recomputa posicoes das 3 luzes a partir do objeto
        Vec3 lpos[3];
        computeLightPositions(posX, posY, posZ, scl, lpos);

        glClearColor(0.05f, 0.05f, 0.08f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        Mat4 view = camera.getViewMatrix();
        Mat4 proj = perspective(45.f * PI / 180.f, 800.f/600.f, 0.1f, 100.f);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);
        glUniform3f(viewPosLoc, camera.position.x, camera.position.y, camera.position.z);

        sendLightUniforms(lpos);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(tex1Loc, 0);
        glUniform1i(useFlatLoc, 0);

        glBindVertexArray(vao);

        // --- Objeto principal com Phong ---
        glUniform3fv(kaLoc, 1, mat.Ka);
        glUniform3fv(kdLoc, 1, mat.Kd);
        glUniform3fv(ksLoc, 1, mat.Ks);
        glUniform1f(nsLoc,     mat.Ns);

        Mat4 model = mul(translate(posX, posY, posZ),
                    mul(rotateX(rotX),
                    mul(rotateY(rotY),
                    mul(rotateZ(rotZ),
                        scaleM(scl)))));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
        glDrawArrays(GL_TRIANGLES, 0, nVerts);

        // --- Indicadores visuais das luzes ---
        glUniform1i(useFlatLoc, 1);
        for (int i = 0; i < 3; i++) {
            if (!lightEnabled[i]) continue;
            glUniform3fv(flatColLoc, 1, indicatorColors[i]);
            Mat4 lmodel = mul(translate(lpos[i].x, lpos[i].y, lpos[i].z), scaleM(0.12f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, lmodel.m);
            glDrawArrays(GL_TRIANGLES, 0, nVerts);
        }
        glUniform1i(useFlatLoc, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
