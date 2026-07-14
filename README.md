# PG-Ray-Tracing

Repositório base para o projeto da disciplina de Processamento Gráfico.

---

## Estrutura do repositório

```text
PG-Ray-Tracing/
├── main.cpp / main.py          ← ponto de entrada (seu código vai aqui)
├── entrega1.py                 ← exemplo funcional da entrega 1 (referência)
│
├── src/
│   ├── Ponto.h / Ponto.py      ← classe Ponto (template — expanda)
│   └── Vetor.h / Vetor.py      ← classe Vetor (template — expanda)
│
└── utils/                      ← infraestrutura pronta (não precisa mexer)
    ├── input/                  ← cenas de exemplo (.json) e malhas (.obj)
    │   ├── sampleScene.json    ← Cornell box com esferas
    │   ├── mirrorScene.json    ← cena com transformações
    │   └── monkeyScene.json    ← cena com malha complexa
    ├── Scene/
    │   ├── sceneSchema.py/.hpp ← tipos de dados da cena (SceneData, etc.)
    │   └── sceneParser.py/.cpp ← carrega um .json → SceneData
    └── MeshReader/
        ├── Colormap.py/.cpp    ← lê arquivos .mtl
        └── ObjReader.py/.cpp   ← lê arquivos .obj
```

---

## Como começar

### Python

```bash
# Rode a partir da raiz do repositório
python main.py
```

### C++

```bash
# Compile e rode a partir da raiz do repositório
g++ -std=c++17 main.cpp -o raytracer && ./raytracer
```

> **Requisito:** compilador com suporte a C++17 (`g++ 8+` ou `clang++ 7+`).

---

## Carregando uma cena

O parser de cena lê um arquivo `.json` e devolve um objeto `SceneData` com os dados de câmera, luzes e objetos. Você deve utilizá-los como parâmetros de entrada para construção dos seus objetos e para a geração da imagem referente àquela cena. 
> **Dica:** você não precisa utilizar dados que ainda não são necessárias para a entrega atual. Por exemplo, não é necessário utilizar o objReader antes da entrega de malhas ou o índice de reflexão antes da entrega de raytracing recursivo.

**Python:**

```python
from utils.Scene.sceneParser import SceneJsonLoader

scene = SceneJsonLoader.load_file("utils/input/sampleScene.json")

print(scene.camera.lookfrom)       # Ponto — posição da câmera
print(scene.camera.lookat)         # Ponto — para onde aponta
print(scene.camera.up_vector)      # Vetor — direção "para cima"
print(scene.camera.screen_distance)# float — distância câmera-tela
print(scene.camera.image_width)    # int   — largura em pixels
print(scene.camera.image_height)   # int   — altura em pixels

for obj in scene.objects:
    print(obj.obj_type)             # "sphere" | "plane" | "mesh"
    print(obj.relative_pos)         # Ponto — posição do objeto
    print(obj.material.color)       # ColorData — kd (difuso)
    print(obj.material.ks)          # ColorData — especular
    print(obj.material.ka)          # ColorData — ambiente
    print(obj.material.ns)          # float     — brilho
    print(obj.material.ni)          # float     — índice de refração

    # Propriedades específicas por tipo:
    if obj.obj_type == "sphere":
        radius = obj.get_num("radius")
        center = obj.get_ponto("center")   # ou obj.relative_pos

    if obj.obj_type == "plane":
        normal = obj.get_vetor("normal")   # Vetor normal ao plano

    if obj.obj_type == "mesh":
        path = obj.get_property("path")    # caminho do arquivo .obj

for light in scene.light_list:
    print(light.pos)    # Ponto — posição da luz
    print(light.color)  # ColorData — cor/intensidade

print(scene.global_light.color)     # ColorData — luz ambiente
```

**C++:**

```cpp
#include "utils/Scene/sceneParser.cpp"

SceneData scene = SceneJsonLoader::loadFile("utils/input/sampleScene.json");

scene.camera.lookfrom       // Ponto
scene.camera.lookat         // Ponto
scene.camera.upVector       // Vetor
scene.camera.screen_distance// double
scene.camera.image_width    // int
scene.camera.image_height   // int

for (auto& obj : scene.objects) {
    obj.objType             // string: "sphere", "plane", "mesh"
    obj.relativePos         // Ponto
    obj.material.color      // ColorData — kd
    obj.material.ks         // ColorData
    obj.material.ka         // ColorData
    obj.material.ns         // double — brilho
    obj.material.ni         // double — IOR

    obj.getNum("radius")    // double
    obj.getVetor("normal")  // Vetor
    obj.getPonto("center")  // Ponto
    obj.getProperty("path") // string
}
```

---

## Lendo uma malha .obj

**Python:**

```python
from utils.MeshReader.ObjReader import ObjReader

mesh = ObjReader("utils/input/cubo.obj")

mesh.get_face_points()   # list[list[Ponto]] — 3 pontos por face
mesh.get_normals()       # list[Vetor]
mesh.get_vertices()      # list[Ponto]
mesh.get_faces()         # list[FaceData]  — índices + material por face
```

**C++:**

```cpp
#include "utils/MeshReader/ObjReader.cpp"

objReader mesh("utils/input/cubo.obj");

mesh.getFacePoints()  // vector<vector<Ponto>>
mesh.getNormals()     // vector<Vetor>
mesh.getVertices()    // vector<Ponto>
```

---

## Gerando a imagem

O formato de saída padrão é **PPM** (texto simples, abre em qualquer visualizador de imagens). Escreva na saída padrão e redirecione para um arquivo:

```bash
python main.py > imagem.ppm
```

Estrutura do arquivo PPM:

```text
P3
<largura> <altura>
255
<r> <g> <b>   ← um pixel por linha, valores 0–255
<r> <g> <b>
...
```

Para converter para PNG:

```bash
python utils/convert_ppm.py imagem.ppm imagem.png
```

---

## O que está pronto vs. o que você implementa

| Componente | Situação | Onde está |
|---|---|---|
| Parser de cena (.json) | **Pronto** | `utils/Scene/sceneParser` |
| Leitor de malhas (.obj) | **Pronto** | `utils/MeshReader/ObjReader` |
| Leitor de materiais (.mtl) | **Pronto** | `utils/MeshReader/Colormap` |
| Cenas de exemplo | **Prontas** | `utils/input/` |
| Classe `Ponto` (base) | Template | `src/Ponto` |
| Classe `Vetor` (base) | Template | `src/Vetor` |
| Câmera | **Você implementa** | `main` ou `src/` |
| Interseção raio-esfera | **Você implementa** | `main` ou `src/` |
| Interseção raio-plano | **Você implementa** | `main` ou `src/` |
| Interseção raio-triângulo | **Você implementa** | `main` ou `src/` |
| Transformações afins | **Você implementa** | `main` ou `src/` |
| Iluminação de Phong | **Você implementa** | `main` ou `src/` |
| Reflexão e refração | **Você implementa** | `main` ou `src/` |

---

## Entregas

O projeto consiste de 4 entregas mais uma entrega individual extra:

1. [Raycasting com esferas e planos](#1-raycasting-com-esferas-e-planos)
2. [Raycasting com malhas de triângulos](#2-raycasting-com-malhas-de-triângulos)
3. [Raytracing não recursivo](#3-raytracing-não-recursivo)
4. [Raytracing recursivo](#4-raytracing-recursivo)
5. [Features individuais](#5-features-individuais)

---

### 1. Raycasting com Esferas e Planos

#### Tipos para pontos e vetores

O grupo pode usar bibliotecas externas ou expandir os templates `src/Ponto` e `src/Vetor` com as operações necessárias (produto escalar, produto vetorial, normalização, subtração, multiplicação por escalar, etc.).

#### Câmera

Implemente uma câmera móvel composta por:

- Ponto de localização no mundo $\small C(c_1, c_2, c_3),\; c_i \in \mathbb{R}$
- Ponto para onde a câmera aponta (centro da tela) $\small M(x, y, z),\; x,y,z \in \mathbb{R}$
- Vetor "para cima" $\small V_{up}(v_1, v_2, v_3),\; V_{up} \neq \vec{0}$
- Três vetores ortonormais $\small W, V, U \in \mathbb{R}^3$ — por convenção, **W** tem a mesma direção que $(M - C)$ mas sentido oposto
- Distância câmera-tela $\small d \in \mathbb{R}_+^*$
- Resolução $\small h_{res}, v_{res} \in \mathbb{Z}_+^*$ (largura e altura em pixels)

> **OBS:** A resolução padrão de pixel é 1×1, ou seja, a tela tem tamanho 1 no mundo e cada pixel mede $\frac{1}{h_{res}}$. O ângulo de visão é consequência direta da resolução e da distância $d$.

Com esses atributos, cada pixel $(i, j)$ mapeia a um ponto na tela por combinação linear dos vetores da base a partir de $C$.

No repositório: `scene.camera` já contém `lookfrom` ($C$), `lookat` ($M$), `up_vector` ($V_{up}$), `screen_distance` ($d$), `image_width` ($h_{res}$), `image_height` ($v_{res}$).

#### Interseções

- **Esfera** — definida por centro $C_\varepsilon(x,y,z)$, raio $r \in \mathbb{R}_+^*$ e cor difusa $O_d \in [0,1]^3$
- **Plano** — definido por ponto $P_p(x,y,z)$, normal $\hat{N}(v_1,v_2,v_3)$ e cor difusa $O_d \in [0,1]^3$

Será necessário renderizar a cena: para cada pixel, disparar um raio e retornar a cor $k_d$ do objeto mais próximo (ou preto se não houver interseção).

> A cena `sampleScene.json` contém um [Cornell box](https://en.wikipedia.org/wiki/Cornell_box) com 2 esferas e 5 planos.

---

### 2. Raycasting com Malhas de Triângulos

#### Malha de triângulos

Uma malha é definida por:

- $n_\triangle \in \mathbb{N}$ — número de triângulos
- $n_\circ \in \mathbb{N},\; n_\circ \geq 3$ — número de vértices
- Lista de vértices (pontos), tamanho $n_\circ$
- Lista de triplas de índices de vértices (uma por triângulo), tamanho $n_\triangle$
- Lista de normais de triângulos (vetores), tamanho $n_\triangle$
- Lista de normais de vértices — cada uma é a média das normais dos triângulos que compartilham aquele vértice, tamanho $n_\circ$
- Cor difusa $O_d \in [0,1]^3$

A malha **deve ser carregada de um arquivo `.obj`** — use `ObjReader` do repositório ou implemente o seu próprio leitor.

#### Transformações afins

- Implemente matrizes de floats/doubles (ex: arrays 4×4)
- Suporte aplicação de matrizes a pontos e vetores
- Implemente `translation`, `scaling` e `rotation`
- Não é necessária animação; renderizar antes e depois de uma transformação é suficiente

A lista de transformações de cada objeto está em `obj.transforms` — **a ordem importa**.

---

### 3. Raytracing não Recursivo

Nesta etapa o programa continua fazendo ray-casting, mas ao invés de retornar apenas a cor difusa $k_d$ do objeto atingido, calcula a cor de cada pixel pelo **modelo de iluminação de Phong**, que simula como a luz interage com a superfície. A equação completa é:

$$I = k_a \cdot I_a + \sum_{n=1}^{m} \left[ k_d \cdot (\hat{L}_n \cdot \hat{N}) \cdot I_{L_n} + k_s \cdot (\hat{R}_n \cdot \hat{V})^\eta \cdot I_{L_n} \right] + k_r \cdot I_r + k_t \cdot I_t$$

**Por enquanto, ignore os termos $k_r \cdot I_r$ e $k_t \cdot I_t$** — reflexões e refrações ficam para a entrega 4.

#### Propriedades de material

Todos os objetos possuem os seguintes coeficientes, todos disponíveis via `obj.material`:

- Coeficiente difuso $\small k_d \in [0, 1]^3$ → `.color`
- Coeficiente especular $\small k_s \in [0, 1]^3$ → `.ks`
- Coeficiente ambiental $\small k_a \in [0, 1]^3$ → `.ka`
- Coeficiente de reflexão $\small k_r \in [0, 1]^3$ → `.kr`
- Coeficiente de transmissão $\small k_t \in [0, 1]^3$ → `.kt`
- Rugosidade $\small \eta > 0$ → `.ns`

#### Fontes de luz

Implemente uma classe de luz com:

- **Luzes pontuais** — posição $l(x,y,z)$ e intensidade $I_{L_n} \in [0,255]^3$ → `scene.light_list`
- **Luz ambiente** — cor $I_a \in [0,255]^3$ → `scene.global_light.color`

#### Vetores do modelo de Phong

Para calcular a equação em cada ponto de interseção $P$, você precisará construir os seguintes vetores (todos normalizados):

- $\hat{N}$ — normal à superfície em $P$ (para esfera: $P - C_\varepsilon$; para plano: normal do plano; para malha: interpolada a partir das normais dos vértices)
- $\hat{L}_n$ — de $P$ até a posição da luz $n$: $\;\hat{L}_n = \text{normalize}(l_n - P)$
- $\hat{R}_n$ — reflexão de $-\hat{L}_n$ em relação a $\hat{N}$: $\;\hat{R}_n = 2(\hat{L}_n \cdot \hat{N})\hat{N} - \hat{L}_n$
- $\hat{V}$ — de $P$ até o observador; para raios primários é a câmera ($\hat{V} = \text{normalize}(C - P)$); muda para raios secundários
- $I_r \in [0,255]^3$ — cor retornada pelo raio refletido (entrega 4)
- $I_t \in [0,255]^3$ — cor retornada pelo raio refratado (entrega 4)

#### Sombras

Para cada luz $n$, antes de somar sua contribuição ao pixel, verifique se o ponto $P$ está em sombra em relação a ela:

1. Construa um **raio de sombra** com origem em $P$ (ligeiramente deslocado ao longo de $\hat{N}$ para evitar auto-interseção) e direção $\hat{L}_n$
2. Teste interseção desse raio com todos os objetos da cena
3. Se houver alguma interseção com $t \in (0,\, |l_n - P|)$ — ou seja, um objeto entre $P$ e a luz — a contribuição de $I_{L_n}$ é **ignorada** para aquele pixel

O resultado é que superfícies bloqueadas por outros objetos ficam na sombra, enquanto as iluminadas diretamente recebem a contribuição difusa e especular normalmente.

---

### 4. Raytracing Recursivo

Nesta etapa, adicione a iluminação recursiva (reflexões e refrações) ao modelo de Phong:

- **IOR dos objetos** — $\text{IOR} \in \mathbb{R},\; \text{IOR} \geq 1$ → `obj.material.ni`. Considere IOR do ar = 1. Pode ser definido na entrada ou na `main`.
- **Reflexão** — para todo objeto com propriedades reflexivas ($k_r > 0$), dispare um raio secundário refletido e some $k_r \cdot I_r$ ao resultado de Phong.
- **Refração** — para todo objeto transparente ($k_t > 0$), calcule a direção refratada via lei de Snell usando `material.ni` e some $k_t \cdot I_t$.
- **Limite de recursão** — defina uma profundidade máxima para evitar recursão infinita.

Como o ray-tracer já sabe fazer ray-casting, basta fazer uma chamada recursiva ao ray-casting a partir dos pontos de interseção com objetos reflexivos ou transparentes, somando a cor secundária ao resultado de Phong.

---

### 5. Features Individuais

⚠️ Cada aluno escolhe **uma** feature extra implementada individualmente. Integrantes do mesmo grupo não podem escolher a mesma feature.

| Dificuldade | Tempo estimado | Nota máxima |
| --- | --- | --- |
| Fácil | 1 dia | 4 pts |
| Média | 3–4 dias | 7 pts |
| Difícil | 7–10 dias | 10 pts |

#### Features fáceis

- **Anti-aliasing** (supersampling) — lança um raio para cada canto do pixel e usa a média como cor final
- **Cones e cilindros**
- **Paraboloide**
- **Textura em planos** — posição do ponto de interseção determina o pixel correspondente na imagem de textura
- **Textura em esferas** — usa coordenadas esféricas
- **Textura procedural** — baseada em uma regra, sem arquivo externo
- **Tone mapping**
- **Bump mapping** (via randomização de normal)

#### Features médias

- **Soft shadows** — fonte de luz extensa com pontos igualmente espaçados ou aleatórios ao longo da fonte
- **Renderizar um toro como malha de triângulos** — gere os triângulos a partir de um retângulo parametrizado
- **Textura sólida**
- **Iluminação de raios paralelos saindo de um retângulo** — como luz solar entrando por uma janela; raios com direção predefinida

#### Features difíceis

- **Superfície de Bézier**
- **Superfície de revolução** com curva geratriz de Bézier
- **Octree** (subdivisão de bounding-box)
- **Relief mapping**
- **Binary Space Partitioning (BSP)**
