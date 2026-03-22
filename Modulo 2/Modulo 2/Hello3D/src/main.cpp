#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
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

// Rotacao em torno do eixo X
Mat4 rotateX(float a) {
    Mat4 r = Mat4::identity();
    r.m[5]  =  cosf(a); r.m[9]  = -sinf(a);
    r.m[6]  =  sinf(a); r.m[10] =  cosf(a);
    return r;
}

// Rotacao em torno do eixo Y
Mat4 rotateY(float a) {
    Mat4 r = Mat4::identity();
    r.m[0]  =  cosf(a); r.m[8]  =  sinf(a);
    r.m[2]  = -sinf(a); r.m[10] =  cosf(a);
    return r;
}

// Rotacao em torno do eixo Z
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
// Estado do cubo controlavel
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

    // Rotacao nos eixos
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) rotX += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) rotY += ROT_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) rotZ += ROT_SPEED * dt;

    // Translacao: WASD (eixos X e Z), IJ (eixo Y)
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) posX -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) posX += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) posZ -= MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) posZ += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) posY += MOVE_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) posY -= MOVE_SPEED * dt;

    // Escala uniforme: [ diminui, ] aumenta
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
// Main
// ============================================================
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600,
        "Hello3D - Instanciando Cubos - Hiago Pansera", nullptr, nullptr);
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

    // --------------------------------------------------------
    // Geometria do cubo: 6 faces x 2 triangulos x 3 vertices
    // Cada vertice: x, y, z, r, g, b
    // Cada face tem uma cor diferente
    // --------------------------------------------------------
    float vertices[] = {
        // Face frontal (z=+0.5) - Vermelho
        -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,   1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 0.0f,

        // Face traseira (z=-0.5) - Verde
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 1.0f, 0.0f,

        // Face esquerda (x=-0.5) - Azul
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,

        // Face direita (x=+0.5) - Amarelo
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,

        // Face inferior (y=-0.5) - Magenta
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,

        // Face superior (y=+0.5) - Ciano
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Atributo 0: posicao (xyz)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Atributo 1: cor (rgb)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Localizacoes dos uniforms
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");

    // Controles impressos no console
    std::cout << "=== Controles ===" << std::endl;
    std::cout << "X / Y / Z  : rotacionar cubo 1 nos eixos X, Y, Z" << std::endl;
    std::cout << "A / D      : transladar no eixo X" << std::endl;
    std::cout << "W / S      : transladar no eixo Z" << std::endl;
    std::cout << "I / J      : transladar no eixo Y" << std::endl;
    std::cout << "[ / ]      : diminuir / aumentar escala" << std::endl;
    std::cout << "ESC        : sair" << std::endl;
    std::cout << "Cubo 2 (direita) e Cubo 3 (esquerda) sao estaticos." << std::endl;

    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastTime;
        lastTime = currentTime;

        processInput(window, dt);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Camera fixa olhando para a origem levemente de cima
        Mat4 view = lookAt(0.0f, 2.5f, 6.0f,
                           0.0f, 0.0f, 0.0f,
                           0.0f, 1.0f, 0.0f);
        Mat4 proj = perspective(
            45.0f * 3.14159265f / 180.0f,
            800.0f / 600.0f,
            0.1f, 100.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);

        glBindVertexArray(VAO);

        // --- Cubo 1: controlado pelo teclado ---
        // Ordem: Translate * RotX * RotY * RotZ * Scale
        Mat4 model1 = mul(translate(posX, posY, posZ),
                     mul(rotateX(rotX),
                     mul(rotateY(rotY),
                     mul(rotateZ(rotZ),
                         scaleM(scl)))));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model1.m);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // --- Cubo 2: estatico, deslocado para a direita ---
        Mat4 model2 = mul(translate(2.8f, 0.0f, 0.0f),
                     mul(rotateY(0.4f),
                         scaleM(0.8f)));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model2.m);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // --- Cubo 3: estatico, deslocado para a esquerda ---
        Mat4 model3 = mul(translate(-2.8f, 0.0f, 0.0f),
                     mul(rotateX(0.5f),
                     mul(rotateY(-0.3f),
                         scaleM(1.2f))));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model3.m);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
