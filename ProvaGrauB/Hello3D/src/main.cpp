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
#include <cstdio>
#include <algorithm>
#include <cctype>

#include "Camera.h"

// ============================================================
// Constantes
// ============================================================
static const float PI       = 3.14159265f;
static const float MOVE_SPD = 3.f;
static const float ROT_SPD  = 1.5f;
static const float SCL_SPD  = 1.f;

// ============================================================
// Material
// ============================================================
struct Material {
    float Ka[3] = {0.15f, 0.15f, 0.15f};
    float Kd[3] = {0.8f,  0.8f,  0.8f };
    float Ks[3] = {1.0f,  1.0f,  1.0f };
    float Ns    = 64.f;
    std::string textureName;
};

// ============================================================
// Curva de Bezier cubica
//
// B(t) = (1-t)^3 * P0 + 3(1-t)^2*t * P1 + 3(1-t)*t^2 * P2 + t^3 * P3
// t in [0,1]: interpola de P0 ate P3 seguindo os pontos de controle P1 e P2
// ============================================================
inline Vec3 bezierCubic(const Vec3& p0, const Vec3& p1,
                         const Vec3& p2, const Vec3& p3, float t)
{
    float u = 1.f - t;
    return p0 * (u*u*u)
         + p1 * (3*u*u*t)
         + p2 * (3*u*t*t)
         + p3 * (t*t*t);
}

// Trajetoria: suporta N pontos de controle encadeados (N = 4, 7, 10, ...).
// A animacao usa pingpong: percorre a curva de P0->Pn e volta Pn->P0,
// criando um ciclo suave sem saltos abruptos.
struct BezierTrajectory {
    std::vector<Vec3> cp;
    float t     = 0.f;  // parametro global [0, 2*numSegs]
    float speed = 0.3f; // segmentos por segundo

    // Numero de segmentos cubicos (cada segmento usa 4 pontos com endpoint compartilhado)
    int numSegs() const {
        return (int)cp.size() >= 4 ? ((int)cp.size() - 1) / 3 : 0;
    }

    void update(float dt) {
        if (numSegs() == 0) return;
        float period = (float)(numSegs() * 2);
        t += speed * dt;
        if (t >= period) t -= period;
    }

    Vec3 getPos() const {
        int ns = numSegs();
        if (ns == 0) return Vec3(0, 0, 0);

        float tt = t;
        // Pingpong: na segunda metade do periodo, percorre em reverso
        if (tt > (float)ns) tt = (float)(ns * 2) - tt;

        int   seg = (int)tt;
        if (seg >= ns) seg = ns - 1;
        float lt  = tt - (float)seg;
        int   b   = seg * 3;

        return bezierCubic(cp[b], cp[b+1], cp[b+2], cp[b+3], lt);
    }
};

// ============================================================
// Objeto de cena
// ============================================================
struct SceneObject {
    std::string name;
    GLuint vao    = 0;
    int    nVerts = 0;
    GLuint texID  = 0;
    Material mat;

    Vec3  pos;         // posicao atual (manual ou sobrescrita por Bezier)
    Vec3  rot;         // angulos de Euler acumulados (radianos)
    float scl = 1.f;   // escala uniforme

    Vec3  initPos;     // posicao inicial (do arquivo de cena)
    Vec3  initRot;     // rotacao inicial (radianos)
    float initScl = 1.f;
    Vec3  fixedScale = Vec3(1, 1, 1); // escala nao-uniforme fixa (forma do objeto)

    BezierTrajectory traj;
    bool hasTraj    = false;
    bool showTexture = true; // toggle textura (tecla M)
};

// ============================================================
// Configuracao de luz
// ============================================================
struct LightCfg {
    std::string name    = "Light";
    Vec3  pos           = Vec3(0, 5, 5);
    Vec3  color         = Vec3(1, 1, 1);
    bool  enabled       = true;
};

// ============================================================
// Configuracao de camera (lida do arquivo de cena)
// ============================================================
struct CamCfg {
    Vec3  pos   = Vec3(0, 2, 8);
    float yaw   = -90.f;
    float pitch = -10.f;
    float fov   = 45.f;
    float nearZ = 0.1f;
    float farZ  = 100.f;
};

// ============================================================
// Estado global
// ============================================================
static Camera              g_camera;
static bool                g_firstMouse = true;
static float               g_lastX = 400.f, g_lastY = 300.f;
static std::vector<SceneObject> g_objects;
static LightCfg            g_lights[3];
static int                 g_selectedIdx = 0;
static bool                g_animRunning = true;
static int                 g_winW = 800, g_winH = 600;
static CamCfg              g_camCfg;

// Locations dos uniforms de luz (pre-buscados para evitar overhead)
static int g_lPosLoc[3], g_lColLoc[3], g_lEnLoc[3];

// Multiplicador global de intensidade das luzes (modificavel em tempo real)
static float g_lightIntensity = 1.0f;

// ============================================================
// Parser do arquivo de cena (.ini)
// ============================================================
static void trimStr(std::string& s) {
    size_t l = s.find_first_not_of(" \t\r\n");
    size_t r = s.find_last_not_of(" \t\r\n");
    s = (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
}

struct ObjDesc {
    std::string name, file;
    Vec3  pos, rot;
    float scl = 1.f;
    Vec3  fixedScale = Vec3(1, 1, 1); // escala nao-uniforme definida em scene.ini
    BezierTrajectory traj;
    bool  hasTraj = false;
};

bool parseScene(const char* path,
                CamCfg& cam,
                std::vector<LightCfg>& lv,
                std::vector<ObjDesc>&  ov)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Arquivo de cena nao encontrado: " << path << "\n";
        return false;
    }

    std::string section;
    LightCfg curL;
    ObjDesc  curO;
    bool inLight = false, inObj = false;

    auto flushL = [&]() {
        if (inLight && lv.size() < 3) lv.push_back(curL);
        inLight = false;
        curL = LightCfg();
    };
    auto flushO = [&]() {
        if (inObj) {
            curO.hasTraj = !curO.traj.cp.empty();
            ov.push_back(curO);
        }
        inObj = false;
        curO = ObjDesc();
    };

    std::string line;
    while (std::getline(f, line)) {
        // Remove comentarios
        auto ci = line.find('#');
        if (ci != std::string::npos) line = line.substr(0, ci);
        trimStr(line);
        if (line.empty()) continue;

        if (line[0] == '[') {
            // Nova secao: salva o que estava sendo construido
            flushL(); flushO();
            section = line.substr(1, line.size() - 2);
            for (char& c : section) c = (char)tolower((unsigned char)c);
            if (section == "light")  { inLight = true; curL = LightCfg(); }
            if (section == "object") { inObj   = true; curO = ObjDesc();  }
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq); trimStr(key);
        std::string val = line.substr(eq + 1); trimStr(val);
        for (char& c : key) c = (char)tolower((unsigned char)c);
        std::istringstream ss(val);

        if (section == "camera") {
            if      (key == "position") ss >> cam.pos.x >> cam.pos.y >> cam.pos.z;
            else if (key == "yaw")      ss >> cam.yaw;
            else if (key == "pitch")    ss >> cam.pitch;
            else if (key == "fov")      ss >> cam.fov;
            else if (key == "near")     ss >> cam.nearZ;
            else if (key == "far")      ss >> cam.farZ;
        }
        else if (section == "light") {
            if      (key == "name")     curL.name = val;
            else if (key == "position") ss >> curL.pos.x >> curL.pos.y >> curL.pos.z;
            else if (key == "color")    ss >> curL.color.x >> curL.color.y >> curL.color.z;
            else if (key == "enabled")  { int e = 1; ss >> e; curL.enabled = (e != 0); }
        }
        else if (section == "object") {
            if      (key == "name")     curO.name = val;
            else if (key == "file")     curO.file = val;
            else if (key == "position") ss >> curO.pos.x >> curO.pos.y >> curO.pos.z;
            else if (key == "rotation") ss >> curO.rot.x >> curO.rot.y >> curO.rot.z;
            else if (key == "scale")    ss >> curO.scl;
            else if (key == "scale_xyz")    ss >> curO.fixedScale.x >> curO.fixedScale.y >> curO.fixedScale.z;
            else if (key == "bezier_speed") ss >> curO.traj.speed;
            else if (key == "bezier_points") {
                // Substitui '/' por espaco para facilitar a leitura dos floats
                std::string bstr = val;
                for (char& c : bstr) if (c == '/') c = ' ';
                std::istringstream bss(bstr);
                float x, y, z;
                while (bss >> x >> y >> z)
                    curO.traj.cp.push_back(Vec3(x, y, z));
            }
        }
    }
    flushL(); flushO();

    // Garante exatamente 3 luzes (preenche com defaults desabilitados)
    LightCfg defL; defL.enabled = false;
    while (lv.size() < 3) lv.push_back(defL);

    return true;
}

// ============================================================
// Compilacao de shaders
// ============================================================
static std::string loadShaderSource(const char* path) {
    std::ifstream file(path);
    if (!file.is_open())
        std::cerr << "Shader nao encontrado: " << path << "\n";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileAndLinkShaders() {
    std::string vs_src = loadShaderSource("shaders/shader.vert");
    std::string fs_src = loadShaderSource("shaders/shader.frag");
    const char* vs_c   = vs_src.c_str();
    const char* fs_c   = fs_src.c_str();
    int ok; char log[512];

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_c, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 512, nullptr, log); std::cerr << "VS: " << log << "\n"; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_c, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 512, nullptr, log); std::cerr << "FS: " << log << "\n"; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 512, nullptr, log); std::cerr << "Link: " << log << "\n"; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ============================================================
// Carregamento de MTL
// ============================================================
static Material loadMTL(const std::string& path) {
    Material mat;
    std::ifstream f(path.c_str());
    if (!f.is_open()) return mat;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string kw; ss >> kw;
        if      (kw == "Ka")     ss >> mat.Ka[0] >> mat.Ka[1] >> mat.Ka[2];
        else if (kw == "Kd")     ss >> mat.Kd[0] >> mat.Kd[1] >> mat.Kd[2];
        else if (kw == "Ks")     ss >> mat.Ks[0] >> mat.Ks[1] >> mat.Ks[2];
        else if (kw == "Ns")     ss >> mat.Ns;
        else if (kw == "map_Kd") ss >> mat.textureName;
    }
    return mat;
}

// ============================================================
// Carregamento de OBJ
// Parser simples: v, vt, vn, f (triangularizado)
// ============================================================
GLuint loadOBJ(const std::string& filePath, int& nVerts, Material& mat) {
    struct Pos { float x, y, z; };
    struct UV  { float s, t;    };
    struct Nrm { float x, y, z; };

    std::vector<Pos> positions;
    std::vector<UV>  texCoords;
    std::vector<Nrm> normals;
    std::vector<float> buf;
    std::string mtlFile;

    size_t sep = filePath.find_last_of("/\\");
    std::string baseDir = (sep != std::string::npos) ? filePath.substr(0, sep + 1) : "";

    std::ifstream f(filePath.c_str());
    if (!f.is_open()) {
        std::cerr << "OBJ nao encontrado: " << filePath << "\n";
        nVerts = 0; return 0;
    }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string w; ss >> w;
        if      (w == "mtllib") { ss >> mtlFile; }
        else if (w == "v")  { Pos p; ss >> p.x >> p.y >> p.z; positions.push_back(p); }
        else if (w == "vt") { UV  t; ss >> t.s >> t.t;        texCoords.push_back(t); }
        else if (w == "vn") { Nrm n; ss >> n.x >> n.y >> n.z; normals.push_back(n);   }
        else if (w == "f") {
            while (ss >> w) {
                int vi = 0, ti = 0, ni = 0;
                std::istringstream is(w); std::string idx;
                if (std::getline(is, idx, '/')) vi = !idx.empty() ? std::stoi(idx) - 1 : 0;
                if (std::getline(is, idx, '/')) ti = !idx.empty() ? std::stoi(idx) - 1 : 0;
                if (std::getline(is, idx))      ni = !idx.empty() ? std::stoi(idx) - 1 : 0;
                buf.push_back(positions[vi].x);
                buf.push_back(positions[vi].y);
                buf.push_back(positions[vi].z);
                if (ti >= 0 && ti < (int)texCoords.size()) {
                    buf.push_back(texCoords[ti].s);
                    buf.push_back(texCoords[ti].t);
                } else { buf.push_back(0.f); buf.push_back(0.f); }
                if (ni >= 0 && ni < (int)normals.size()) {
                    buf.push_back(normals[ni].x);
                    buf.push_back(normals[ni].y);
                    buf.push_back(normals[ni].z);
                } else { buf.push_back(0.f); buf.push_back(1.f); buf.push_back(0.f); }
            }
        }
    }

    if (!mtlFile.empty()) {
        mat = loadMTL(baseDir + mtlFile);
        // Prepend baseDir ao nome da textura para carregamento correto
        if (!mat.textureName.empty())
            mat.textureName = baseDir + mat.textureName;
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    // layout 0: posicao (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    // layout 1: texcoord (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // layout 2: normal (vec3)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    nVerts = (int)(buf.size() / 8);
    return VAO;
}

// ============================================================
// Carregamento de textura
// ============================================================
GLuint loadTexture(const std::string& path) {
    if (path.empty()) return 0;
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
        std::cout << "Textura: " << path << " (" << w << "x" << h << ")\n";
    } else {
        std::cerr << "Falha ao carregar textura: " << path << "\n";
    }
    stbi_image_free(data);
    return id;
}

// Textura branca 1x1 usada quando a textura esta desabilitada (modo material puro)
GLuint createWhiteTexture() {
    GLuint id; glGenTextures(1, &id);
    unsigned char white[3] = {255, 255, 255};
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return id;
}

// ============================================================
// Texturas procedurais — geradas em CPU, enviadas para GPU
// ============================================================
static unsigned int lcg(unsigned int& s) { s = s*1664525u + 1013904223u; return s; }
static float frand(unsigned int& s) { return (float)(lcg(s) & 0xFFFF) / 65535.f; }

static GLuint uploadTexture(const std::vector<unsigned char>& d, int w, int h) {
    GLuint id; glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return id;
}

// Fileiras de tijolos laranja-vermelhos com argamassa cinza clara
GLuint createBrickTexture() {
    const int W=256, H=128, BW=58, BH=22, M=4;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = 12345u;
    for (int y=0; y<H; y++) {
        int row = y/(BH+M), ry = y%(BH+M);
        int off = (row%2==0) ? 0 : BW/2;
        for (int x=0; x<W; x++) {
            int cx = (x+off)%(BW+M);
            int i = (y*W+x)*3;
            if (ry>=BH || cx>=BW) {
                d[i]=185; d[i+1]=180; d[i+2]=175; // argamassa
            } else {
                float v = frand(s)*0.18f;
                d[i]  = (unsigned char)std::min(255.f, 175.f+v*60.f);
                d[i+1]= (unsigned char)std::max(0.f,   65.f+v*30.f);
                d[i+2]= (unsigned char)std::max(0.f,   52.f+v*20.f);
            }
        }
    }
    return uploadTexture(d,W,H);
}

// Grade de janelas de vidro: moldura escura + vidro azul-cinza
GLuint createGlassTexture() {
    const int W=128, H=256, WW=18, WH=28, F=3;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = 99999u;
    for (int y=0; y<H; y++) {
        for (int x=0; x<W; x++) {
            int wx=x%(WW+F), wy=y%(WH+F);
            int i=(y*W+x)*3;
            if (wx>=WW || wy>=WH) {
                d[i]=50; d[i+1]=55; d[i+2]=60; // moldura escura
            } else {
                float v = frand(s)*0.12f;
                d[i]  = (unsigned char)(88 +v*40);
                d[i+1]= (unsigned char)(122+v*40);
                d[i+2]= (unsigned char)(162+v*35);
            }
        }
    }
    return uploadTexture(d,W,H);
}

// Concreto bege com linhas horizontais sutis (junta de concretagem)
GLuint createConcreteTexture() {
    const int W=256, H=256;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = 54321u;
    for (int y=0; y<H; y++) {
        bool joint = (y%42==0 || y%42==1);
        for (int x=0; x<W; x++) {
            float v = frand(s)*0.10f;
            int i=(y*W+x)*3;
            if (joint) {
                d[i]=148; d[i+1]=138; d[i+2]=112;
            } else {
                d[i]  = (unsigned char)(188+v*42);
                d[i+1]= (unsigned char)(176+v*36);
                d[i+2]= (unsigned char)(140+v*30);
            }
        }
    }
    return uploadTexture(d,W,H);
}

// Asfalto: cinza escuro com granulado sutil
GLuint createAsphaltTexture() {
    const int W=256, H=256;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = 11111u;
    for (int y=0; y<H; y++) {
        for (int x=0; x<W; x++) {
            float v = frand(s);
            int i=(y*W+x)*3;
            unsigned char c = (unsigned char)(28+v*30);
            d[i]=c; d[i+1]=c; d[i+2]=(unsigned char)(c+4);
        }
    }
    return uploadTexture(d,W,H);
}

// Pedra/pavimento: marrom com variacao de cor
GLuint createStoneTexture() {
    const int W=256, H=256;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = 77777u;
    for (int y=0; y<H; y++) {
        for (int x=0; x<W; x++) {
            float v = frand(s);
            int i=(y*W+x)*3;
            d[i]  = (unsigned char)(82 +v*55);
            d[i+1]= (unsigned char)(62 +v*42);
            d[i+2]= (unsigned char)(43 +v*32);
        }
    }
    return uploadTexture(d,W,H);
}

// Tinta metalica lisa para veiculos (cor base configuravel)
GLuint createPaintTexture(unsigned char r, unsigned char g, unsigned char b) {
    const int W=64, H=64;
    std::vector<unsigned char> d(W*H*3);
    unsigned int s = (unsigned int)(r*12345u + g*54321u + b);
    for (int y=0; y<H; y++) {
        for (int x=0; x<W; x++) {
            float v = frand(s)*0.07f;
            int i=(y*W+x)*3;
            d[i]  = (unsigned char)std::min(255,(int)(r+v*60));
            d[i+1]= (unsigned char)std::min(255,(int)(g+v*60));
            d[i+2]= (unsigned char)std::min(255,(int)(b+v*60));
        }
    }
    return uploadTexture(d,W,H);
}

// Retorna textura procedural adequada ao nome do objeto
GLuint assignProceduralTexture(const std::string& name) {
    if (name == "Plataforma") return createStoneTexture();
    if (name == "Predio_A")   return createGlassTexture();
    if (name == "Predio_B")   return createConcreteTexture();
    if (name == "Predio_C")   return createBrickTexture();
    if (name == "Rua")        return createAsphaltTexture();
    if (name == "Veiculo_A")  return createPaintTexture(220, 170, 18);
    if (name == "Veiculo_B")  return createPaintTexture(185, 25, 22);
    return 0;
}

// ============================================================
// Uniforms de luz
// ============================================================
void setupLightUniforms(GLuint shader) {
    for (int i = 0; i < 3; i++) {
        char buf[64];
        std::sprintf(buf, "lightPos[%d]",     i); g_lPosLoc[i] = glGetUniformLocation(shader, buf);
        std::sprintf(buf, "lightColor[%d]",   i); g_lColLoc[i] = glGetUniformLocation(shader, buf);
        std::sprintf(buf, "lightEnabled[%d]", i); g_lEnLoc[i]  = glGetUniformLocation(shader, buf);
    }
}

void sendLightUniforms() {
    for (int i = 0; i < 3; i++) {
        glUniform3f(g_lPosLoc[i], g_lights[i].pos.x, g_lights[i].pos.y, g_lights[i].pos.z);
        // Aplica multiplicador de intensidade sobre a cor da luz
        glUniform3f(g_lColLoc[i],
            g_lights[i].color.x * g_lightIntensity,
            g_lights[i].color.y * g_lightIntensity,
            g_lights[i].color.z * g_lightIntensity);
        glUniform1i(g_lEnLoc[i],  g_lights[i].enabled ? 1 : 0);
    }
}

// ============================================================
// Callbacks
// ============================================================
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    g_winW = w; g_winH = h;
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    float fx = (float)xpos, fy = (float)ypos;
    if (g_firstMouse) { g_lastX = fx; g_lastY = fy; g_firstMouse = false; }
    g_camera.rotate(fx - g_lastX, -(fy - g_lastY));
    g_lastX = fx; g_lastY = fy;
}

void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;

    // TAB: selecionar proximo objeto
    if (key == GLFW_KEY_TAB && !g_objects.empty()) {
        g_selectedIdx = (g_selectedIdx + 1) % (int)g_objects.size();
        std::cout << "Selecionado: [" << g_selectedIdx << "] "
                  << g_objects[g_selectedIdx].name << "\n";
    }

    // 1/2/3: toggle Key/Fill/Back light
    if (key == GLFW_KEY_1) {
        g_lights[0].enabled = !g_lights[0].enabled;
        std::cout << g_lights[0].name << ": " << (g_lights[0].enabled ? "ON" : "OFF") << "\n";
    }
    if (key == GLFW_KEY_2) {
        g_lights[1].enabled = !g_lights[1].enabled;
        std::cout << g_lights[1].name << ": " << (g_lights[1].enabled ? "ON" : "OFF") << "\n";
    }
    if (key == GLFW_KEY_3) {
        g_lights[2].enabled = !g_lights[2].enabled;
        std::cout << g_lights[2].name << ": " << (g_lights[2].enabled ? "ON" : "OFF") << "\n";
    }

    // P: pausar / retomar animacao Bezier
    if (key == GLFW_KEY_P) {
        g_animRunning = !g_animRunning;
        std::cout << "Animacao Bezier: " << (g_animRunning ? "RODANDO" : "PAUSADA") << "\n";
    }

    // M: toggle textura do objeto selecionado (mostra material puro)
    if (key == GLFW_KEY_M && !g_objects.empty()) {
        SceneObject& sel = g_objects[g_selectedIdx];
        sel.showTexture = !sel.showTexture;
        std::cout << sel.name
                  << " textura: "
                  << (sel.showTexture ? "ON (textura)" : "OFF (material puro - Ka/Kd/Ks do .mtl)") << "\n";
        // Imprime os coeficientes lidos do .mtl para comprovacao
        std::cout << "  Ka = (" << sel.mat.Ka[0] << ", " << sel.mat.Ka[1] << ", " << sel.mat.Ka[2] << ")\n"
                  << "  Kd = (" << sel.mat.Kd[0] << ", " << sel.mat.Kd[1] << ", " << sel.mat.Kd[2] << ")\n"
                  << "  Ks = (" << sel.mat.Ks[0] << ", " << sel.mat.Ks[1] << ", " << sel.mat.Ks[2] << ")\n"
                  << "  Ns = "  << sel.mat.Ns << "\n";
    }

    // R: resetar transforms do objeto selecionado
    if (key == GLFW_KEY_R && !g_objects.empty()) {
        SceneObject& obj = g_objects[g_selectedIdx];
        obj.pos  = obj.initPos;
        obj.rot  = obj.initRot;
        obj.scl  = obj.initScl;
        obj.traj.t = 0.f;
        std::cout << "Reset: " << obj.name << "\n";
    }

    // + / -: modificar intensidade global das luzes em tempo real
    if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
        g_lightIntensity = std::min(3.0f, g_lightIntensity + 0.1f);
        std::cout << "Intensidade das luzes: " << g_lightIntensity << "\n";
    }
    if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
        g_lightIntensity = std::max(0.0f, g_lightIntensity - 0.1f);
        std::cout << "Intensidade das luzes: " << g_lightIntensity << "\n";
    }

    // Q / E: modificar Ns (brilho especular) do objeto selecionado
    if (!g_objects.empty()) {
        if (key == GLFW_KEY_Q) {
            g_objects[g_selectedIdx].mat.Ns = std::max(1.f, g_objects[g_selectedIdx].mat.Ns - 8.f);
            std::cout << g_objects[g_selectedIdx].name
                      << " Ns (shininess): " << g_objects[g_selectedIdx].mat.Ns << "\n";
        }
        if (key == GLFW_KEY_E) {
            g_objects[g_selectedIdx].mat.Ns = std::min(256.f, g_objects[g_selectedIdx].mat.Ns + 8.f);
            std::cout << g_objects[g_selectedIdx].name
                      << " Ns (shininess): " << g_objects[g_selectedIdx].mat.Ns << "\n";
        }
    }
}

// ============================================================
// Input continuo (camera + objeto selecionado)
// ============================================================
void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // --- Camera (WASD + Space/Ctrl) ---
    float fwd = 0, side = 0, vert = 0;
    if (glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS) fwd  += 1;
    if (glfwGetKey(window, GLFW_KEY_S)            == GLFW_PRESS) fwd  -= 1;
    if (glfwGetKey(window, GLFW_KEY_A)            == GLFW_PRESS) side -= 1;
    if (glfwGetKey(window, GLFW_KEY_D)            == GLFW_PRESS) side += 1;
    if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) vert += 1;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vert -= 1;
    g_camera.move(fwd, side, vert, dt);

    if (g_objects.empty()) return;
    SceneObject& obj = g_objects[g_selectedIdx];

    // --- Rotacao do objeto (X/Y/Z) ---
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) obj.rot.x += ROT_SPD * dt;
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) obj.rot.y += ROT_SPD * dt;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) obj.rot.z += ROT_SPD * dt;

    // --- Translacao (setas + I/J) - so disponivel se objeto nao tiver trajetoria ativa ---
    if (!obj.hasTraj || !g_animRunning) {
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) obj.pos.x -= MOVE_SPD * dt;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) obj.pos.x += MOVE_SPD * dt;
        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) obj.pos.z -= MOVE_SPD * dt;
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) obj.pos.z += MOVE_SPD * dt;
        if (glfwGetKey(window, GLFW_KEY_I)     == GLFW_PRESS) obj.pos.y += MOVE_SPD * dt;
        if (glfwGetKey(window, GLFW_KEY_J)     == GLFW_PRESS) obj.pos.y -= MOVE_SPD * dt;
    }

    // --- Escala uniforme (O diminui / L aumenta) ---
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        obj.scl = std::max(0.05f, obj.scl - SCL_SPD * dt);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        obj.scl = std::min(10.f,  obj.scl + SCL_SPD * dt);
}

// ============================================================
// Main
// ============================================================
int main() {
    // === 1. Carregar configuracao da cena ===
    std::vector<LightCfg> lightVec;
    std::vector<ObjDesc>  objDescs;

    if (!parseScene("assets/scene.ini", g_camCfg, lightVec, objDescs)) {
        // Cena padrao se o arquivo nao for encontrado
        LightCfg l0; l0.name = "Key Light";  l0.pos = Vec3(-3,5,5);  l0.color = Vec3(1.0f,0.9f,0.75f);
        LightCfg l1; l1.name = "Fill Light"; l1.pos = Vec3(4,3,4);   l1.color = Vec3(0.4f,0.42f,0.55f);
        LightCfg l2; l2.name = "Back Light"; l2.pos = Vec3(0,4,-5);  l2.color = Vec3(0.7f,0.72f,0.9f);
        lightVec.push_back(l0); lightVec.push_back(l1); lightVec.push_back(l2);
        ObjDesc o; o.name = "Cubo"; o.file = "assets/cube.obj"; o.scl = 1.f;
        objDescs.push_back(o);
    }

    for (int i = 0; i < 3; i++) g_lights[i] = lightVec[i];

    // === 2. Inicializar GLFW ===
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_winW, g_winH,
        "Diorama Urbano - Grau B v2 - Hiago Pansera", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar janela GLFW\n";
        glfwTerminate(); return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD\n"; return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // === 3. Compilar shaders ===
    GLuint shader = compileAndLinkShaders();

    // === 4. Buscar locations dos uniforms ===
    int modelLoc   = glGetUniformLocation(shader, "model");
    int viewLoc    = glGetUniformLocation(shader, "view");
    int projLoc    = glGetUniformLocation(shader, "projection");
    int tex1Loc    = glGetUniformLocation(shader, "texture1");
    int vpLoc      = glGetUniformLocation(shader, "viewPos");
    int kaLoc      = glGetUniformLocation(shader, "Ka");
    int kdLoc      = glGetUniformLocation(shader, "Kd");
    int ksLoc      = glGetUniformLocation(shader, "Ks");
    int nsLoc      = glGetUniformLocation(shader, "Ns");
    int useFlatLoc = glGetUniformLocation(shader, "useFlatColor");
    int flatColLoc = glGetUniformLocation(shader, "flatColor");
    setupLightUniforms(shader);

    // === 5. Textura branca de fallback ===
    GLuint whiteTex = createWhiteTexture();

    // === 6. Carregar objetos da cena ===
    for (const auto& desc : objDescs) {
        SceneObject obj;
        obj.name       = desc.name;
        obj.traj       = desc.traj;
        obj.hasTraj    = desc.hasTraj;
        obj.fixedScale = desc.fixedScale;
        // Rotacao do arquivo em graus -> radianos
        obj.initRot = Vec3(desc.rot.x * PI / 180.f,
                           desc.rot.y * PI / 180.f,
                           desc.rot.z * PI / 180.f);
        obj.initPos = desc.pos;
        obj.initScl = desc.scl;
        obj.pos     = obj.initPos;
        obj.rot     = obj.initRot;
        obj.scl     = obj.initScl;
        obj.showTexture = true;

        obj.vao = loadOBJ(desc.file, obj.nVerts, obj.mat);
        if (!obj.mat.textureName.empty())
            obj.texID = loadTexture(obj.mat.textureName);
        // Se nao tiver textura no MTL, gera textura procedural pelo nome do objeto
        if (obj.texID == 0) {
            GLuint proc = assignProceduralTexture(obj.name);
            obj.texID = (proc != 0) ? proc : whiteTex;
        }

        g_objects.push_back(obj);
        std::cout << "Carregado: [" << g_objects.size() - 1 << "] "
                  << obj.name << " (" << obj.nVerts << " vertices, traj="
                  << (obj.hasTraj ? "sim" : "nao") << ")\n";
    }

    // === 7. Configurar camera com dados do arquivo ===
    g_camera = Camera(g_camCfg.pos, g_camCfg.yaw, g_camCfg.pitch, 5.f, 0.1f);

    // === Imprimir controles ===
    std::cout << "\n========================================\n"
              << "  Diorama Urbano - Grau B v2\n"
              << "  Hiago Pansera - Computacao Grafica\n"
              << "========================================\n"
              << " CAMERA\n"
              << "   W/A/S/D + Mouse   : mover / rotacionar\n"
              << "   Espaco / Ctrl     : subir / descer\n"
              << " OBJETO (TAB para selecionar)\n"
              << "   X / Y / Z         : rotacionar eixo\n"
              << "   Setas / I / J     : transladar (XZ / Y)\n"
              << "   [ / ]             : escala uniforme\n"
              << "   M                 : toggle textura\n"
              << "   R                 : resetar transforms\n"
              << " ILUMINACAO\n"
              << "   1                 : Key Light on/off\n"
              << "   2                 : Fill Light on/off\n"
              << "   3                 : Back Light on/off\n"
              << " ANIMACAO\n"
              << "   P                 : pausar / retomar Bezier\n"
              << " ESC                 : sair\n"
              << "========================================\n\n";

    float lastTime = (float)glfwGetTime();

    // Cores visuais dos indicadores de luz
    static const float INDICATOR_COLORS[3][3] = {
        {1.f, 0.9f, 0.3f},  // Key  - amarelo
        {0.3f, 0.6f, 1.f},  // Fill - azul
        {0.9f, 0.9f, 1.f},  // Back - branco
    };

    // === 8. Loop de renderizacao ===
    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt  = now - lastTime;
        lastTime  = now;

        processInput(window, dt);

        // Atualizar trajetorias Bezier
        if (g_animRunning) {
            for (auto& obj : g_objects)
                if (obj.hasTraj)
                    obj.traj.update(dt);
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        // Matrizes de view e projecao
        Mat4 view = g_camera.getViewMatrix();
        float aspect = (g_winH > 0) ? (float)g_winW / (float)g_winH : 1.f;
        Mat4 proj = perspective(g_camCfg.fov * PI / 180.f, aspect, g_camCfg.nearZ, g_camCfg.farZ);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, proj.m);
        glUniform3f(vpLoc, g_camera.position.x, g_camera.position.y, g_camera.position.z);
        glUniform1i(useFlatLoc, 0);

        sendLightUniforms();

        // --- Renderizar objetos da cena ---
        for (int i = 0; i < (int)g_objects.size(); i++) {
            SceneObject& obj = g_objects[i];
            if (!obj.vao) continue;

            // Posicao: Bezier se tiver trajetoria ativa, manual caso contrario
            Vec3 worldPos = (obj.hasTraj && g_animRunning)
                          ? obj.traj.getPos()
                          : obj.pos;

            // Matriz de modelo: T * Rx * Ry * Rz * S_uniforme * S_fixa
            // S_fixa define a forma do objeto (nao-uniforme, lida do scene.ini)
            // S_uniforme e a escala interativa do usuario (teclas [ / ])
            Mat4 model = mul(translate(worldPos.x, worldPos.y, worldPos.z),
                         mul(rotateX(obj.rot.x),
                         mul(rotateY(obj.rot.y),
                         mul(rotateZ(obj.rot.z),
                         mul(scaleM(obj.scl),
                             scaleXYZ(obj.fixedScale.x,
                                      obj.fixedScale.y,
                                      obj.fixedScale.z))))));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);

            // Objeto selecionado recebe boost de ambiente para destaque visual
            float boost = (i == g_selectedIdx) ? 0.2f : 0.f;
            float ka[3] = {
                std::min(1.f, obj.mat.Ka[0] + boost),
                std::min(1.f, obj.mat.Ka[1] + boost),
                std::min(1.f, obj.mat.Ka[2] + boost)
            };
            glUniform3fv(kaLoc, 1, ka);
            glUniform3fv(kdLoc, 1, obj.mat.Kd);
            glUniform3fv(ksLoc, 1, obj.mat.Ks);
            glUniform1f(nsLoc,     obj.mat.Ns);

            // Textura: usa a textura real ou branca (modo material puro)
            GLuint texBind = (obj.showTexture) ? obj.texID : whiteTex;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texBind);
            glUniform1i(tex1Loc, 0);
            glUniform1i(useFlatLoc, 0);

            glBindVertexArray(obj.vao);
            glDrawArrays(GL_TRIANGLES, 0, obj.nVerts);
        }

        // --- Renderizar indicadores visuais das luzes ---
        if (!g_objects.empty() && g_objects[0].vao) {
            glUniform1i(useFlatLoc, 1);
            glBindVertexArray(g_objects[0].vao);
            for (int i = 0; i < 3; i++) {
                if (!g_lights[i].enabled) continue;
                glUniform3fv(flatColLoc, 1, INDICATOR_COLORS[i]);
                Mat4 lm = mul(translate(g_lights[i].pos.x,
                                        g_lights[i].pos.y,
                                        g_lights[i].pos.z),
                              scaleM(0.15f));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, lm.m);
                glDrawArrays(GL_TRIANGLES, 0, g_objects[0].nVerts);
            }
            glUniform1i(useFlatLoc, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // === 9. Cleanup ===
    for (auto& obj : g_objects) {
        if (obj.vao) glDeleteVertexArrays(1, &obj.vao);
        if (obj.texID && obj.texID != whiteTex)
            glDeleteTextures(1, &obj.texID);
    }
    glDeleteTextures(1, &whiteTex);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
