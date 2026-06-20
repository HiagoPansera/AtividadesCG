#version 330 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D texture1;
uniform vec3 viewPos;

// Material
uniform vec3  Ka;
uniform vec3  Kd;
uniform vec3  Ks;
uniform float Ns;

// Tres fontes de luz pontuais
// [0] Key Light   - luz principal, mais intensa, frente-lateral
// [1] Fill Light  - preenchimento, mais suave, lado oposto
// [2] Back Light  - fundo, separa objeto do background
uniform vec3 lightPos[3];
uniform vec3 lightColor[3];
uniform bool lightEnabled[3];

// Constantes de atenuacao para cena grande (diorama ~16 unidades)
// att(d=10) ~ 0.71 | att(d=15) ~ 0.57 | att(d=20) ~ 0.45
const float ATT_C = 1.0;
const float ATT_L = 0.02;
const float ATT_Q = 0.002;

// Cor plana para indicadores de luz
uniform bool useFlatColor;
uniform vec3 flatColor;

// Calcula contribuicao Phong de uma luz pontual com atenuacao na difusa
vec3 calcPointLight(vec3 lPos, vec3 lColor, vec3 norm, vec3 viewDir, vec3 texColor) {
    vec3  lDir = normalize(lPos - FragPos);
    float dist = length(lPos - FragPos);
    float att  = 1.0 / (ATT_C + ATT_L * dist + ATT_Q * dist * dist);

    // Difusa com atenuacao
    float diff    = max(dot(norm, lDir), 0.0);
    vec3  diffuse = Kd * diff * texColor * lColor * att;

    // Especular com atenuacao
    vec3  rDir    = reflect(-lDir, norm);
    float spec    = pow(max(dot(viewDir, rDir), 0.0), Ns);
    vec3  specular = Ks * spec * lColor * att;

    return diffuse + specular;
}

void main() {
    if (useFlatColor) {
        FragColor = vec4(flatColor, 1.0);
        return;
    }

    vec3 texColor = vec3(texture(texture1, TexCoord));
    vec3 norm     = normalize(Normal);
    vec3 viewDir  = normalize(viewPos - FragPos);

    // Ambiente global (nao depende das luzes)
    vec3 result = Ka * texColor;

    // Soma a contribuicao de cada luz ativa
    for (int i = 0; i < 3; i++) {
        if (lightEnabled[i])
            result += calcPointLight(lightPos[i], lightColor[i], norm, viewDir, texColor);
    }

    FragColor = vec4(result, 1.0);
}
