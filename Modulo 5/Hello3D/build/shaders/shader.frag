#version 330 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D texture1;

uniform vec3 lightPos;
uniform vec3 viewPos;

uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform float Ns;

void main() {
    vec3 texColor = vec3(texture(texture1, TexCoord));

    // Ambiente
    vec3 ambient = Ka * texColor;

    // Difusa
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = Kd * diff * texColor;

    // Especular
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), Ns);
    vec3 specular   = Ks * spec;

    vec3 result = ambient + diffuse + specular;
    FragColor   = vec4(result, 1.0);
}
