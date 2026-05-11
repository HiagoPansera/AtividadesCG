#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Camera.h"

// ============================================================
// Trajetoria linear ciclica
// ============================================================
struct Trajectory {
    std::vector<Vec3> points;
    float progress;  // [0, points.size())
    float speed;     // segmentos por segundo

    Trajectory() : progress(0.f), speed(0.4f) {}

    void addPoint(Vec3 p) { points.push_back(p); }

    void removeLast() {
        if (!points.empty()) {
            points.pop_back();
            if (!points.empty() && progress >= (float)points.size())
                progress = 0.f;
        }
    }

    void clear() { points.clear(); progress = 0.f; }

    void update(float dt) {
        if (points.size() < 2) return;
        progress += speed * dt;
        while (progress >= (float)points.size())
            progress -= (float)points.size();
    }

    // Retorna posicao interpolada; se sem trajetoria, retorna fallback
    Vec3 getPosition(Vec3 fallback) const {
        if (points.empty()) return fallback;
        if (points.size() == 1) return points[0];
        int n = (int)points.size();
        int i = (int)progress % n;
        int j = (i + 1) % n;
        float t = progress - (float)(int)progress;
        Vec3 a = points[i], b = points[j];
        return Vec3(a.x + t*(b.x-a.x), a.y + t*(b.y-a.y), a.z + t*(b.z-a.z));
    }
};

// ============================================================
// Material e objeto da cena
// ============================================================
struct Material {
    float Ka[3] = {0.2f, 0.2f, 0.2f};
    float Kd[3] = {0.8f, 0.8f, 0.8f};
    float Ks[3] = {1.0f, 1.0f, 1.0f};
    float Ns    = 32.0f;
    std::string textureName;
};

struct SceneObject {
    GLuint vao;
    int    nVerts;
    GLuint texID;
    Material mat;
    std::string name;
    Vec3  basePosition;
    Vec3  color;    // tint aplicado sobre a textura
    float scale;
    Trajectory trajectory;
};

// ============================================================
// Estado global
// ============================================================
static Camera camera(Vec3(0.f, 3.f, 14.f), -90.f, -10.f, 5.f, 0.1f);
static bool   firstMouse = true;
static float  lastX = 400.f, lastY = 300.f;

static std::vector<SceneObject> objects;
static int   selectedIdx = 0;
static Vec3  cursor(0.f, 0.f, 0.f);
static float cursorSpeed = 4.f;

// ============================================================
// Forward declarations
// ============================================================
void saveTrajectories(const std::string& path);
void loadTrajectories(const std::string& path);

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

    if (key == GLFW_KEY_TAB && !objects.empty()) {
        selectedIdx = (selectedIdx + 1) % (int)objects.size();
        std::cout << "Selecionado: " << objects[selectedIdx].name << "\n";
    }
    if (key == GLFW_KEY_P && !objects.empty()) {
        objects[selectedIdx].trajectory.addPoint(cursor);
        int n = (int)objects[selectedIdx].trajectory.points.size();
        std::cout << "[" << objects[selectedIdx].name << "] waypoint #" << n
                  << " em (" << cursor.x << ", " << cursor.y << ", " << cursor.z << ")\n";
    }
    if (key == GLFW_KEY_BACKSPACE && !objects.empty()) {
        objects[selectedIdx].trajectory.removeLast();
        std::cout << "[" << objects[selectedIdx].name << "] ultimo waypoint removido ("
                  << objects[selectedIdx].trajectory.points.size() << " restantes)\n";
    }
    if (key == GLFW_KEY_C && !objects.empty()) {
        objects[selectedIdx].trajectory.clear();
        std::cout << "[" << objects[selectedIdx].name << "] trajetoria limpa\n";
    }
    if (key == GLFW_KEY_F5) saveTrajectories("assets/trajectories.txt");
    if (key == GLFW_KEY_F9) loadTrajectories("assets/trajectories.txt");
}

// ============================================================
// Input continuo
// ============================================================
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Camera - WASD + espaco/ctrl
    float fwd = 0.f, side = 0.f, vert = 0.f;
    if (glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS) fwd  += 1.f;
    if (glfwGetKey(window, GLFW_KEY_S)            == GLFW_PRESS) fwd  -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_A)            == GLFW_PRESS) side -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_D)            == GLFW_PRESS) side += 1.f;
    if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) vert += 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vert -= 1.f;
    camera.move(fwd, side, vert, dt);

    // Cursor 3D - setas (XZ) + PgUp/PgDn (Y)
    if (glfwGetKey(window, GLFW_KEY_UP)        == GLFW_PRESS) cursor.z -= cursorSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_DOWN)      == GLFW_PRESS) cursor.z += cursorSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT)      == GLFW_PRESS) cursor.x -= cursorSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_RIGHT)     == GLFW_PRESS) cursor.x += cursorSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS) cursor.y += cursorSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) cursor.y -= cursorSpeed * dt;
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
// MTL e OBJ
// ============================================================
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

// ============================================================
// Textura
// ============================================================
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
        std::cout << "Textura carregada: " << path << " (" << w << "x" << h << ")\n";
    } else {
        std::cerr << "Falha ao carregar textura: " << path << "\n";
    }
    stbi_image_free(data);
    return id;
}

// ============================================================
// Salvar / Carregar trajetorias em arquivo texto
// ============================================================
void saveTrajectories(const std::string& path) {
    std::ofstream f(path.c_str());
    f << "# Modulo 6 - trajetorias\n";
    f << "# formato: indice_objeto x y z\n";
    for (int i = 0; i < (int)objects.size(); i++)
        for (const Vec3& p : objects[i].trajectory.points)
            f << i << " " << p.x << " " << p.y << " " << p.z << "\n";
    std::cout << "Trajetorias salvas em: " << path << "\n";
}

void loadTrajectories(const std::string& path) {
    for (auto& obj : objects) obj.trajectory.clear();
    std::ifstream f(path.c_str());
    if (!f.is_open()) { std::cout << "Arquivo nao encontrado: " << path << "\n"; return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        int idx; Vec3 p;
        if (ss >> idx >> p.x >> p.y >> p.z)
            if (idx >= 0 && idx < (int)objects.size())
                objects[idx].trajectory.addPoint(p);
    }
    std::cout << "Trajetorias carregadas de: " << path << "\n";
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
        "Hello3D - Trajetorias - Hiago Pansera", nullptr, nullptr);
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

    int modelLoc     = glGetUniformLocation(shader, "model");
    int viewLoc      = glGetUniformLocation(shader, "view");
    int projLoc      = glGetUniformLocation(shader, "projection");
    int tex1Loc      = glGetUniformLocation(shader, "texture1");
    int lightPosLoc  = glGetUniformLocation(shader, "lightPos");
    int viewPosLoc   = glGetUniformLocation(shader, "viewPos");
    int kaLoc        = glGetUniformLocation(shader, "Ka");
    int kdLoc        = glGetUniformLocation(shader, "Kd");
    int ksLoc        = glGetUniformLocation(shader, "Ks");
    int nsLoc        = glGetUniformLocation(shader, "Ns");
    int objColorLoc  = glGetUniformLocation(shader, "objColor");
    int useFlatLoc   = glGetUniformLocation(shader, "useFlatColor");
    int flatColorLoc = glGetUniformLocation(shader, "flatColor");

    // Geometria e textura compartilhadas entre os objetos
    int nVerts = 0;
    Material baseMat;
    GLuint sharedVAO = loadOBJ("assets/cube.obj", nVerts, baseMat);
    GLuint sharedTex = 0;
    if (!baseMat.textureName.empty())
        sharedTex = loadTexture("assets/" + baseMat.textureName);

    // Criar 3 objetos da cena com cores distintas
    auto makeObj = [&](const char* name, Vec3 base, Vec3 color) {
        SceneObject o;
        o.vao = sharedVAO; o.nVerts = nVerts;
        o.texID = sharedTex; o.mat = baseMat;
        o.name = name; o.basePosition = base;
        o.color = color; o.scale = 1.f;
        objects.push_back(o);
    };
    makeObj("Cubo A", Vec3( 0.f, 0.f,  0.f), Vec3(1.f,  0.65f, 0.65f));  // avermelhado
    makeObj("Cubo B", Vec3( 6.f, 0.f,  0.f), Vec3(0.65f, 1.f,  0.65f));  // esverdeado
    makeObj("Cubo C", Vec3(-6.f, 0.f,  0.f), Vec3(0.65f, 0.65f, 1.f ));  // azulado

    loadTrajectories("assets/trajectories.txt");

    float lightPos[3] = {4.f, 8.f, 6.f};
    const float PI = 3.14159265f;
    float lastTime = (float)glfwGetTime();

    std::cout << "\n=== Controles ===\n";
    std::cout << "W/A/S/D + Mouse    : mover camera\n";
    std::cout << "Espaco / Ctrl      : subir / descer camera\n";
    std::cout << "Setas              : mover cursor 3D (eixos X e Z)\n";
    std::cout << "PgUp / PgDn        : mover cursor 3D (eixo Y)\n";
    std::cout << "TAB                : selecionar proximo objeto\n";
    std::cout << "P                  : adicionar waypoint na posicao do cursor\n";
    std::cout << "Backspace          : remover ultimo waypoint do objeto\n";
    std::cout << "C                  : limpar trajetoria do objeto selecionado\n";
    std::cout << "F5                 : salvar trajetorias em arquivo\n";
    std::cout << "F9                 : recarregar trajetorias do arquivo\n";
    std::cout << "ESC                : sair\n";
    std::cout << "\nObjeto selecionado: " << objects[selectedIdx].name << "\n";
    std::cout << "Cursor em: (0, 0, 0) - mova com as setas e PgUp/PgDn\n\n";

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window, dt);
        for (auto& obj : objects)
            obj.trajectory.update(dt);

        glClearColor(0.08f, 0.08f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        Mat4 view = camera.getViewMatrix();
        Mat4 proj = perspective(45.f * PI / 180.f, 800.f / 600.f, 0.1f, 200.f);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);
        glUniform3fv(lightPosLoc, 1, lightPos);
        glUniform3f(viewPosLoc, camera.position.x, camera.position.y, camera.position.z);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sharedTex);
        glUniform1i(tex1Loc, 0);

        glBindVertexArray(sharedVAO);

        // --- Objetos da cena com Phong + tint ---
        glUniform1i(useFlatLoc, 0);
        for (int i = 0; i < (int)objects.size(); i++) {
            SceneObject& obj = objects[i];
            Vec3 pos = obj.trajectory.getPosition(obj.basePosition);

            // Objeto selecionado recebe tint amarelo
            Vec3 tint = (i == selectedIdx) ? Vec3(1.f, 1.f, 0.4f) : obj.color;
            glUniform3f(objColorLoc, tint.x, tint.y, tint.z);
            glUniform3fv(kaLoc, 1, obj.mat.Ka);
            glUniform3fv(kdLoc, 1, obj.mat.Kd);
            glUniform3fv(ksLoc, 1, obj.mat.Ks);
            glUniform1f(nsLoc,     obj.mat.Ns);

            Mat4 model = mul(translate(pos.x, pos.y, pos.z), scaleM(obj.scale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, obj.nVerts);
        }

        // --- Cursor 3D: cubo pequeno amarelo brilhante ---
        glUniform1i(useFlatLoc, 1);
        glUniform3f(flatColorLoc, 1.f, 1.f, 0.f);
        {
            Mat4 model = mul(translate(cursor.x, cursor.y, cursor.z), scaleM(0.18f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, nVerts);
        }

        // --- Waypoints do objeto selecionado: cubos pequenos vermelhos ---
        glUniform3f(flatColorLoc, 1.f, 0.15f, 0.15f);
        for (const Vec3& wp : objects[selectedIdx].trajectory.points) {
            Mat4 model = mul(translate(wp.x, wp.y, wp.z), scaleM(0.12f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, nVerts);
        }

        // --- Piso de referencia: cubo achatado cinza ---
        glUniform3f(flatColorLoc, 0.25f, 0.25f, 0.28f);
        {
            Mat4 model = mul(translate(0.f, -1.1f, -5.f), scaleXYZ(30.f, 0.1f, 30.f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, nVerts);
        }

        glUniform1i(useFlatLoc, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sharedVAO);
    glDeleteTextures(1, &sharedTex);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
