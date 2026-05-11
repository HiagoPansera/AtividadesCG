#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "Camera.h"

// ============================================================
// Camera global + estado do mouse
// ============================================================
static Camera camera(Vec3(0.f, 1.f, 8.f), -90.f, 0.f, 4.f, 0.1f);
static bool  firstMouse = true;
static float lastX = 400.f, lastY = 300.f;

void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    float fx = (float)xpos, fy = (float)ypos;
    if (firstMouse) { lastX = fx; lastY = fy; firstMouse = false; }
    float dx =  fx - lastX;
    float dy = -(fy - lastY);  // invertido: y da tela cresce para baixo
    lastX = fx; lastY = fy;
    camera.rotate(dx, dy);
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float fwd = 0.f, side = 0.f, vert = 0.f;
    if (glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS) fwd  += 1.f;
    if (glfwGetKey(window, GLFW_KEY_S)            == GLFW_PRESS) fwd  -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_A)            == GLFW_PRESS) side -= 1.f;
    if (glfwGetKey(window, GLFW_KEY_D)            == GLFW_PRESS) side += 1.f;
    if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) vert += 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vert -= 1.f;

    camera.move(fwd, side, vert, dt);
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
// Material e MTL
// ============================================================
struct Material {
    float Ka[3] = {0.2f, 0.2f, 0.2f};
    float Kd[3] = {0.8f, 0.8f, 0.8f};
    float Ks[3] = {1.0f, 1.0f, 1.0f};
    float Ns    = 32.0f;
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

// ============================================================
// Carregamento OBJ — 8 floats/vertice: xyz, st, nxnynz
// ============================================================
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
    if (!f.is_open()) {
        std::cerr << "OBJ nao encontrado: " << filePath << "\n";
        nVerts = 0; return 0;
    }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line); std::string w; ss >> w;
        if      (w == "mtllib") { ss >> mtlFile; }
        else if (w == "v")  { Pos p; ss >> p.x >> p.y >> p.z; positions.push_back(p); }
        else if (w == "vt") { UV  t; ss >> t.s >> t.t;        texCoords.push_back(t); }
        else if (w == "vn") { Nrm n; ss >> n.x >> n.y >> n.z; normals.push_back(n);   }
        else if (w == "f") {
            while (ss >> w) {
                int vi=0, ti=0, ni=0;
                std::istringstream is(w); std::string idx;
                if (std::getline(is, idx, '/')) vi = !idx.empty() ? std::stoi(idx)-1 : 0;
                if (std::getline(is, idx, '/')) ti = !idx.empty() ? std::stoi(idx)-1 : 0;
                if (std::getline(is, idx))      ni = !idx.empty() ? std::stoi(idx)-1 : 0;
                buf.push_back(positions[vi].x); buf.push_back(positions[vi].y); buf.push_back(positions[vi].z);
                if (ti >= 0 && ti < (int)texCoords.size()) { buf.push_back(texCoords[ti].s); buf.push_back(texCoords[ti].t); }
                else { buf.push_back(0.f); buf.push_back(0.f); }
                if (ni >= 0 && ni < (int)normals.size()) { buf.push_back(normals[ni].x); buf.push_back(normals[ni].y); buf.push_back(normals[ni].z); }
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
// Carregamento de textura
// ============================================================
GLuint loadTexture(const std::string& path) {
    GLuint id; glGenTextures(1, &id);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
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
// Main
// ============================================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "Hello3D - Camera Primeira Pessoa - Hiago Pansera", nullptr, nullptr);
    if (!window) { std::cerr << "Falha ao criar janela GLFW\n"; glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // captura o cursor

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD\n"; return -1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint shader = compileAndLinkShaders();

    int nVerts = 0;
    Material mat;
    GLuint vao = loadOBJ("assets/cube.obj", nVerts, mat);

    GLuint texID = 0;
    if (!mat.textureName.empty())
        texID = loadTexture("assets/" + mat.textureName);

    int modelLoc    = glGetUniformLocation(shader, "model");
    int viewLoc     = glGetUniformLocation(shader, "view");
    int projLoc     = glGetUniformLocation(shader, "projection");
    int tex1Loc     = glGetUniformLocation(shader, "texture1");
    int lightPosLoc = glGetUniformLocation(shader, "lightPos");
    int viewPosLoc  = glGetUniformLocation(shader, "viewPos");
    int kaLoc       = glGetUniformLocation(shader, "Ka");
    int kdLoc       = glGetUniformLocation(shader, "Kd");
    int ksLoc       = glGetUniformLocation(shader, "Ks");
    int nsLoc       = glGetUniformLocation(shader, "Ns");

    // Posicoes dos cubos espalhados pela cena para explorar
    float cubePos[][3] = {
        { 0.f,  0.f,  0.f},
        { 4.f,  0.f, -2.f},
        {-4.f,  0.f, -2.f},
        { 0.f,  0.f, -6.f},
        { 6.f,  0.f, -6.f},
        {-6.f,  0.f, -6.f},
        { 2.f,  2.f, -4.f},
        {-2.f, -1.f, -4.f},
        { 0.f,  0.f, -12.f},
        { 4.f,  1.f, -10.f},
    };
    int nCubes = sizeof(cubePos) / sizeof(cubePos[0]);

    float lightPos[3] = {4.f, 6.f, 4.f};

    std::cout << "=== Controles ===\n";
    std::cout << "W / S        : avancar / recuar\n";
    std::cout << "A / D        : mover para os lados\n";
    std::cout << "Espaco / Ctrl: subir / descer\n";
    std::cout << "Mouse        : olhar ao redor\n";
    std::cout << "ESC          : sair\n";

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window, dt);

        glClearColor(0.1f, 0.1f, 0.15f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        glUniform1i(tex1Loc, 0);

        Mat4 view = camera.getViewMatrix();
        Mat4 proj = perspective(45.f * 3.14159265f / 180.f, 800.f / 600.f, 0.1f, 100.f);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);

        glUniform3fv(lightPosLoc, 1, lightPos);
        Vec3 camPos = camera.position;
        glUniform3f(viewPosLoc, camPos.x, camPos.y, camPos.z);

        glUniform3fv(kaLoc, 1, mat.Ka);
        glUniform3fv(kdLoc, 1, mat.Kd);
        glUniform3fv(ksLoc, 1, mat.Ks);
        glUniform1f(nsLoc, mat.Ns);

        glBindVertexArray(vao);
        for (int i = 0; i < nCubes; i++) {
            float angle = (float)i * 20.f * 3.14159265f / 180.f;
            Mat4 model = mul(
                translate(cubePos[i][0], cubePos[i][1], cubePos[i][2]),
                rotateY(angle)
            );
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);
            glDrawArrays(GL_TRIANGLES, 0, nVerts);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &texID);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
