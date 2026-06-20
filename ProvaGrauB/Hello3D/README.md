# Diorama Urbano — Grau B v2
**Hiago Pansera | Computação Gráfica | Unisinos 2025**

---

## Descrição

Visualizador 3D em OpenGL 3.3 Core Profile representando um **diorama estilo quarteirão urbano**:
uma plataforma elevada com três prédios de alturas distintas, uma rua central e dois veículos
animados percorrendo trajetórias de Bézier cúbica. Integra todos os componentes do pipeline
gráfico estudados ao longo do semestre.

```
         [Prédio A]      [Prédio B]
          azul-cinza       bege
              |               |
  Veículo A →→ [===  Rua  ===] →→
              ←← [===  Rua  ===] ← Veículo B
              |               |
         [Prédio C]     [Plataforma base]
          tijolo
```

---

## Setup — Compilação e Execução

### Dependências
- CMake 3.5+
- MinGW-w64 (MSYS2/ucrt64)
- GLFW3
- OpenGL 3.3+

### Compilar

```powershell
cd "GrauB Versao2/Hello3D"
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
./Hello3D.exe
```

---

## Controles

| Tecla | Ação |
|-------|------|
| W / A / S / D + Mouse | Mover e rotacionar câmera (first-person) |
| Espaço / Ctrl | Câmera sobe / desce |
| TAB | Selecionar próximo objeto |
| X / Y / Z | Rotacionar objeto selecionado |
| Setas / I / J | Transladar objeto selecionado |
| `[` / `]` | Escala uniforme interativa |
| 1 / 2 / 3 | Toggle Key / Fill / Back light |
| + / - | Aumentar / diminuir intensidade das luzes |
| Q / E | Diminuir / aumentar Ns (brilho especular) do objeto |
| P | Pausar / retomar animação Bézier |
| M | Alternar textura ↔ material puro (imprime Ka/Kd/Ks no console) |
| R | Resetar transforms do objeto selecionado |
| ESC | Sair |

---

## Cena — Objetos e Materiais

| Objeto | Arquivo | Cor (Kd) | scale_xyz | Animação |
|--------|---------|----------|-----------|----------|
| Plataforma | platform.obj | marrom (0.35,0.25,0.15) | 16×0.6×16 | — |
| Prédio A | building_a.obj | azul-cinza (0.40,0.52,0.65) | 3×9×3 | — |
| Prédio B | building_b.obj | bege (0.85,0.78,0.60) | 2.5×6×2.5 | — |
| Prédio C | building_c.obj | tijolo (0.72,0.32,0.22) | 2×4×2 | — |
| Rua | road.obj | asfalto (0.18,0.18,0.18) | 16×0.05×3.5 | — |
| Veículo A | vehicle_a.obj | amarelo (1.0,0.85,0.10) | 1.2×0.45×0.7 | Bézier cúbica |
| Veículo B | vehicle_b.obj | vermelho (0.80,0.15,0.12) | 1.0×0.40×0.65 | Bézier cúbica |

**`scale_xyz`**: escala não-uniforme fixa que define a forma do objeto (lida de `scene.ini`).
O usuário controla escala uniforme interativa por cima com `[` / `]`.

---

## Arquivo de Configuração (`assets/scene.ini`)

```ini
[camera]   position / yaw / pitch / fov / near / far

[light]    name / position / color / enabled   (até 3 luzes)

[object]   name / file / position / rotation / scale
           scale_xyz = sx sy sz   <- escala nao-uniforme da forma
           bezier_speed / bezier_points  <- trajetoria Bezier
```

---

## Funcionalidades Implementadas

| Requisito | Implementação |
|-----------|--------------|
| Múltiplos OBJs (triangularizados, normais, UVs) | 7 arquivos .obj distintos |
| Ka/Kd/Ks/Ns do .mtl usados no shader | `loadMTL()` → uniforms Ka/Kd/Ks/Ns |
| Iluminação Phong com atenuação quadrática | `calcPointLight()` no fragment shader |
| 3 fontes de luz pontuais (sistema 3 pontos) | Key/Fill/Back Light |
| Toggle individual de cada luz | Teclas 1 / 2 / 3 |
| Modificar parâmetros de luz em tempo real | +/- (intensidade), Q/E (Ns especular) |
| Câmera first-person (teclado + mouse) | Classe `Camera` com move()/rotate() |
| Seleção de objetos (TAB) | `g_selectedIdx` cicla o vetor de objetos |
| Translação / Rotação / Escala uniforme | Setas, X/Y/Z, [/] |
| Arquivo de cena com câmera + frustum + luzes + objetos | `assets/scene.ini` |
| Escala não-uniforme baked via `scale_xyz` | `scaleXYZ()` na matriz de modelo |
| Animação Bézier cúbica com pause | `bezierCubic()` + `BezierTrajectory` |

---

## Assets

| Asset | Procedência |
|-------|-------------|
| `*.obj` / `*.mtl` | Criados manualmente para este projeto |
| `Texture.png` | Textura de placeholder (substituir por texturas reais) |

Repositórios de modelos gratuitos:
- [Poly Haven](https://polyhaven.com/) — CC0
- [Sketchfab Free](https://sketchfab.com/features/free-3d-models) — CC BY
- [AmbientCG](https://ambientcg.com/) — CC0 (texturas PBR)

---

## Referências

- LearnOpenGL — https://learnopengl.com/
- Anton's OpenGL 4 Tutorials — https://antongerdelan.net/opengl/
- OpenGL Reference — https://docs.gl/
- GLAD — https://glad.dav1d.de/
- GLFW — https://www.glfw.org/
- stb_image — https://github.com/nothings/stb
- Notas de aula — Computação Gráfica, Unisinos 2025
