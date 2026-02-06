# Area Decoration System

Sistema para decorar automaticamente regiões do mapa com itens baseados em regras de floor (chão).

## Arquivos do Sistema

### Core
- `source/area_decoration.h` - Header com todas as estruturas e classes
- `source/area_decoration.cpp` - Implementação da lógica de decoração

### UI
- `source/area_decoration_dialog.h` - Header dos dialogs
- `source/area_decoration_dialog.cpp` - Implementação dos dialogs wxWidgets
- `source/cluster_preview_window.h` - Header do dialog de preview de cluster
- `source/cluster_preview_window.cpp` - Implementação do ClusterPreviewWindow

### Integração
- `source/main_menubar.cpp` - Menu item "Area Decoration" no menu Window
- `source/main_menubar.h` - Declaração do handler
- `source/gui_ids.h` - ID `AREA_DECORATION` para o menu
- `data/menubar.xml` - Definição do menu item
- `source/CMakeLists.txt` - Arquivos adicionados ao build

---

## Estruturas de Dados (namespace AreaDecoration)

### ItemEntry
```cpp
struct ItemEntry {
    uint16_t itemId = 0;    // Server ID do item
    int weight = 100;       // Peso para seleção aleatória
};
```

### RuleMode
Determina como uma FloorRule opera:
```cpp
enum class RuleMode {
    SingleFloor,    // Match single floor ID
    FloorRange,     // Match floor ID range
    Cluster         // Cluster-based placement (no floor matching)
};
```

Cada modo define comportamentos distintos (ver seção **Modos de Regra** abaixo).

### FloorRule
Define quais itens podem spawnar em determinados floors, ou um cluster de itens dispostos em grid:
```cpp
struct FloorRule {
    std::string name;

    // Floor matching: single ID or range
    uint16_t floorId = 0;        // Para single floor
    uint16_t fromFloorId = 0;    // Para range (inicio)
    uint16_t toFloorId = 0;      // Para range (fim)

    // Items for this rule
    std::vector<ItemEntry> items;  // Itens possiveis

    // Border item to place on top of decoration items (0 = none)
    uint16_t borderItemId = 0;

    // Friend floor - bias placement toward another ground tile (0 = disabled)
    uint16_t friendFloorId = 0;
    uint16_t friendFromFloorId = 0;
    uint16_t friendToFloorId = 0;
    int friendChance = 0;    // 0-100 (%)
    int friendStrength = 0;  // 0-100 (stronger = tighter bias)

    // Placement settings
    int maxPlacements = -1;   // -1 = ilimitado
    float density = 1.0f;     // 0.0 - 1.0
    int priority = 0;         // Maior prioridade e processado primeiro
    bool enabled = true;

    // Rule mode (determines which fields are active)
    RuleMode ruleMode = RuleMode::SingleFloor;

    // --- Cluster mode fields (only used when ruleMode == Cluster) ---
    std::vector<CompositeTile> clusterTiles;  // Items on a grid with offsets
    bool hasCenterPoint = false;
    Position centerOffset;  // Relative offset within the cluster grid

    // For centered mode: how many cluster instances to place
    int instanceCount = 1;
    int instanceMinDistance = 5;  // Min distance between instances

    bool matchesFloor(uint16_t groundId) const;
    bool isRangeRule() const { return ruleMode == RuleMode::FloorRange; }
    bool isClusterRule() const { return ruleMode == RuleMode::Cluster; }

    // Cluster helper methods
    void getClusterBounds(Position& outMin, Position& outMax) const;
    std::vector<uint16_t> getClusterItemIds() const;
    size_t getClusterTotalItemCount() const;
    uint16_t getClusterRepresentativeItemId() const;
};
```

### SpacingConfig
```cpp
struct SpacingConfig {
    int minDistance = 1;           // Distância mínima entre itens
    int minSameItemDistance = 2;   // Distância mínima entre itens iguais
    bool checkDiagonals = true;    // Considerar diagonais
};
```

### DistributionMode
```cpp
enum class DistributionMode {
    PureRandom,   // Distribuição aleatória pura
    Clustered,    // Agrupa itens em clusters
    GridBased     // Distribui em grid com jitter
};

struct DistributionConfig {
    DistributionMode mode = DistributionMode::PureRandom;
    float clusterStrength = 0.5f;  // 0.0-1.0
    int clusterCount = 3;
    int gridSpacingX = 3;
    int gridSpacingY = 3;
    int gridJitter = 1;            // Variação aleatória no grid
};
```

### DecorationPreset
Configuração completa salva/carregável:
```cpp
struct DecorationPreset {
    std::string name;
    std::vector<FloorRule> floorRules;
    SpacingConfig spacing;
    DistributionConfig distribution;
    int maxItemsTotal = -1;
    bool skipBlockedTiles = true;
    uint64_t defaultSeed = 0;  // 0 = seed aleatório

    // Serialização XML
    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);
    std::string toXmlString() const;
    bool fromXmlString(const std::string& xml);
};
```

### AreaDefinition
Define a região a ser decorada:
```cpp
struct AreaDefinition {
    enum class Type {
        Rectangle,   // Retângulo definido por coordenadas
        FloodFill,   // Preenchimento a partir de ponto
        Selection    // Seleção atual do editor
    };

    Type type = Type::Rectangle;

    // Para Rectangle
    Position rectMin;
    Position rectMax;

    // Para FloodFill
    Position floodOrigin;
    uint16_t floodTargetGround = 0;
    int floodMaxRadius = 100;

    std::vector<Position> getAllPositions(Map& map) const;
};
```

### PreviewState
Estado do preview (não destrutivo):
```cpp
struct PreviewItem {
    Position position;
    uint16_t itemId = 0;
    const FloorRule* sourceRule = nullptr;
};

class PreviewState {
public:
    std::vector<PreviewItem> items;
    int totalItemsPlaced = 0;
    std::unordered_map<uint16_t, int> itemCountById;
    uint64_t seed = 0;
    bool isValid = false;

    bool hasItemAt(const Position& pos) const;
    std::vector<const PreviewItem*> getItemsAt(const Position& pos) const;
};
```

---

## Modos de Regra (Rule Modes)

Cada `FloorRule` opera em um de tres modos, controlado pelo campo `ruleMode`:

### 1. Single Floor ID (`RuleMode::SingleFloor`)

Modo padrao. Faz match de um unico ground tile ID. Itens da regra sao colocados em tiles cujo ground possui o `floorId` especificado.

- **Campo usado:** `floorId`
- **Comportamento:** Para cada tile na area, se `tile.groundId == rule.floorId`, a regra pode colocar um item naquele tile.

### 2. Floor Range (`RuleMode::FloorRange`)

Faz match de um intervalo de floor IDs. Itens sao colocados em tiles cujo ground ID esta dentro do range `[fromFloorId, toFloorId]`.

- **Campos usados:** `fromFloorId`, `toFloorId`
- **Comportamento:** Para cada tile, se `fromFloorId <= tile.groundId <= toFloorId`, a regra pode colocar um item naquele tile.

### 3. Cluster (`RuleMode::Cluster`)

Modo especial sem floor matching. Define um conjunto de itens organizados em um grid (cluster). Cada `CompositeTile` no vetor `clusterTiles` possui um offset (x, y, z) e uma lista de item IDs que ocupam aquela posicao relativa.

**Sub-modos de placement:**

#### Centered (hasCenterPoint = true)
O usuario define um ponto central dentro do grid do cluster (via `centerOffset`). O engine coloca instancias inteiras do cluster como unidades dentro da area. O numero de instancias e controlado por `instanceCount` e a distancia minima entre instancias por `instanceMinDistance`.

- **Campos usados:** `clusterTiles`, `hasCenterPoint`, `centerOffset`, `instanceCount`, `instanceMinDistance`
- **Comportamento:** O engine seleciona posicoes aleatorias na area, verificando distancia minima entre instancias, e coloca o cluster completo centrado em cada posicao.

#### Random (hasCenterPoint = false)
Sem ponto central definido. Itens do cluster sao espalhados individualmente em posicoes aleatorias dentro da area. Cada item de cada `CompositeTile` e tratado como um item individual para placement.

- **Campos usados:** `clusterTiles`
- **Comportamento:** O engine extrai todos os item IDs do cluster e os distribui aleatoriamente pela area, sem manter a disposicao relativa do grid.

---

## ClusterPreviewWindow

Dialog visual para inspecionar e configurar clusters definidos em uma `FloorRule`.

**Arquivo:** `source/cluster_preview_window.h` / `source/cluster_preview_window.cpp`

### Funcionalidades
- Exibe um grid visual representando os tiles definidos no cluster
- Cada celula mostra os sprites dos itens atribuidos aquela posicao
- Clique em uma celula para definir o ponto central do cluster (destacado com borda dourada)
- Hover sobre celulas mostra tooltip com informacoes do tile (offset, item IDs)
- CheckBox para habilitar/desabilitar selecao de centro

### Layout
```
+------------------------------------------+
|  [x] Enable Center Point Selection       |
|                                          |
|  +----+----+----+----+                   |
|  |    | i1 |    |    |                   |
|  +----+----+----+----+                   |
|  | i2 | i3 |****| i4 |   **** = center  |
|  +----+----+----+----+                   |
|  |    | i5 |    |    |                   |
|  +----+----+----+----+                   |
|                                          |
|  Grid: 4x3 | Items: 5                   |
|  Center: (2, 1, 0)                       |
+------------------------------------------+
```

### Construtor
```cpp
ClusterPreviewWindow(wxWindow* parent, AreaDecoration::FloorRule& rule,
                     std::function<void()> onChangeCallback = nullptr);
```

O dialog recebe uma referencia direta a `FloorRule` e modifica `hasCenterPoint` e `centerOffset` quando o usuario clica para definir o centro. O callback opcional notifica o dialog pai sobre alteracoes.

---

## Classes Principais

### DecorationEngine
Motor principal de processamento:
```cpp
class DecorationEngine {
public:
    DecorationEngine(Editor* editor);

    void setArea(const AreaDefinition& area);
    void setPreset(const DecorationPreset& preset);

    bool generatePreview(uint64_t seed = 0);  // Gera preview
    bool rerollPreview();                      // Re-gera com nova seed
    const PreviewState& getPreviewState() const;

    bool applyPreview();   // Aplica ao mapa (cria Action para undo)
    void clearPreview();

private:
    // Métodos de geração por modo
    void generatePureRandom(...);
    void generateClustered(...);
    void generateGridBased(...);

    bool checkSpacing(const Position& pos, uint16_t itemId) const;
    const ItemEntry* selectItemFromRule(const FloorRule* rule);
};
```

### PresetManager
Gerencia presets salvos (Singleton):
```cpp
class PresetManager {
public:
    static PresetManager& getInstance();

    bool loadPresets();   // Carrega de data/presets/decoration/
    bool savePresets();

    std::vector<std::string> getPresetNames() const;
    const DecorationPreset* getPreset(const std::string& name) const;
    bool addPreset(const DecorationPreset& preset);
    bool removePreset(const std::string& name);

    std::string getPresetsDirectory() const;  // data/presets/decoration/
};
```

### PreviewManager
Gerencia preview ativo para renderização (Singleton):
```cpp
class PreviewManager {
public:
    static PreviewManager& getInstance();

    void setActivePreview(PreviewState* preview);
    PreviewState* getActivePreview() const;
    void clearActivePreview();
    bool hasActivePreview() const;

    std::vector<const PreviewItem*> getPreviewItemsAt(const Position& pos) const;

    float getPreviewOpacity() const;
    void setPreviewOpacity(float opacity);
};
```

---

## Dialogs UI

### AreaDecorationDialog
Dialog principal com abas:

**Abas:**
1. **Area** - Seleção da região (Rectangle, Selection)
2. **Floor Rules** - Lista de regras com Add/Edit/Remove
3. **Settings** - Spacing, Distribution, Limits
4. **Seed** - Configuração de seed para reprodutibilidade

**Controles de Preset:**
- ComboBox para selecionar preset salvo
- TextCtrl para nome do preset
- Botões: Save, Delete, Export, Import

**Ações:**
- Preview - Gera preview visual
- Reroll - Nova seed
- Apply - Aplica ao mapa
- Revert - Limpa preview

### FloorRuleEditDialog
Dialog para editar uma regra (não-modal):

**Layout em 2 colunas:**

**Coluna Esquerda:**
- Rule Name (TextCtrl)
- Rule Mode Selection:
  - Single Floor ID (RadioButton + SpinCtrl)
  - Floor Range (RadioButton + 2 SpinCtrl)
  - Cluster (RadioButton) - habilita grid de cluster com preview visual
- Floor Selection com Preview visual (para modos SingleFloor/FloorRange)
- Settings (Density, Max Placements, Priority)
- Cluster Settings (quando modo Cluster):
  - Instance Count (SpinCtrl)
  - Instance Min Distance (SpinCtrl)
  - Botao "Preview Cluster..." para abrir ClusterPreviewWindow
- Doodad Browser com paginacao e busca

**Coluna Direita:**
- Items List (wxListCtrl com ícones)
  - Colunas: Icon, Item ID, Weight, Name
  - Suporta drag & drop
- Controles: ID, Browse, Weight, Add, Remove

**Funcionalidades:**
- Extrai itens de doodads (singles + composites)
- Preview do floor em tempo real
- Busca de doodads com paginação (50 por página)

---

## Integração com Doodads

O sistema extrai itens de DoodadBrush, incluindo composites:

```cpp
void FloorRuleEditDialog::AddItemsFromDoodad(DoodadBrush* doodad) {
    int maxVariation = doodad->getMaxVariation();
    for (int v = 0; v < maxVariation; ++v) {
        // Single items
        int singleCount = doodad->getSingleCount(v);
        for (int i = 0; i < singleCount; ++i) {
            uint16_t itemId = doodad->getSingleItemId(v, i);
            int weight = doodad->getSingleItemChance(v, i);
            // adiciona à lista
        }

        // Composite items
        int compositeCount = doodad->getCompositeCount(v);
        for (int c = 0; c < compositeCount; ++c) {
            const CompositeTileList& composite = doodad->getCompositeAt(v, c);
            for (const auto& tilePair : composite) {
                for (Item* item : tilePair.second) {
                    // adiciona cada item do composite
                }
            }
        }
    }
}
```

---

## Renderização de Sprites

**Importante:** Usar `clientID` do ItemType, não o server ID diretamente:

```cpp
wxBitmap GetItemBitmap(uint16_t itemId, int size) {
    // Correto: obter clientID do ItemType
    const ItemType& itemType = g_items.getItemType(itemId);
    Sprite* spr = nullptr;
    if (itemType.id != 0) {
        spr = g_gui.gfx.getSprite(itemType.clientID);
    }
    // desenhar sprite...
}
```

---

## Formato XML do Preset

```xml
<?xml version="1.0"?>
<decoration_preset name="Forest Floor">
    <spacing
        min_distance="1"
        min_same_item_distance="2"
        check_diagonals="true"/>

    <distribution
        mode="0"
        cluster_strength="0.5"
        cluster_count="3"
        grid_spacing_x="3"
        grid_spacing_y="3"
        grid_jitter="1"/>

    <settings
        max_items_total="-1"
        skip_blocked="true"
        default_seed="0"/>

    <floor_rules>
        <!-- Single Floor mode (default) -->
        <rule
            name="Grass Decorations"
            floor_id="4526"
            from_floor_id="0"
            to_floor_id="0"
            density="0.3"
            max_placements="-1"
            priority="0"
            enabled="true">
            <item id="2767" weight="100"/>
            <item id="2768" weight="50"/>
        </rule>

        <!-- Cluster mode example -->
        <rule
            name="Forest Cluster"
            rule_mode="cluster"
            has_center="true"
            center_x="2"
            center_y="2"
            center_z="0"
            density="0.5"
            max_placements="-1"
            priority="0"
            enabled="true"
            instance_count="3"
            instance_min_distance="5">
            <cluster_tile x="0" y="0" z="0">
                <item id="2767"/>
                <item id="2768"/>
            </cluster_tile>
            <cluster_tile x="1" y="0" z="0">
                <item id="2769"/>
            </cluster_tile>
            <cluster_tile x="2" y="1" z="0">
                <item id="2770"/>
            </cluster_tile>
        </rule>
    </floor_rules>
</decoration_preset>
```

### Atributos do Cluster Mode na XML

| Atributo | Tipo | Descricao |
|---|---|---|
| `rule_mode` | string | `"cluster"` para ativar modo cluster |
| `has_center` | bool | `"true"` para modo centered, `"false"` para random |
| `center_x`, `center_y`, `center_z` | int | Offset do ponto central no grid (quando `has_center="true"`) |
| `instance_count` | int | Numero de instancias do cluster a colocar (centered mode) |
| `instance_min_distance` | int | Distancia minima entre instancias |

Cada `<cluster_tile>` define uma posicao no grid com seus itens:
- `x`, `y`, `z`: Offset relativo da posicao no grid do cluster
- Filhos `<item id="..."/>`: Itens que ocupam essa posicao

---

## Fluxo de Uso

### Fluxo Basico (Single Floor / Floor Range)

1. Usuario abre Window > Area Decoration
2. Define area (Rectangle ou Selection)
3. Adiciona Floor Rules:
   - Clica "Add Rule"
   - Seleciona modo: Single Floor ID ou Floor Range
   - Define floor ID(s)
   - Adiciona itens manualmente ou de doodads
   - Define density e outras opcoes
4. Configura spacing/distribution nas abas
5. Clica "Preview" para ver resultado
6. Clica "Reroll" se quiser nova distribuicao
7. Clica "Apply" para aplicar (suporta undo)
8. Opcionalmente salva como preset

### Fluxo Cluster Mode

1. Usuario abre Window > Area Decoration
2. Define area (Rectangle ou Selection)
3. Adiciona Floor Rule com modo Cluster:
   - Clica "Add Rule"
   - Seleciona modo "Cluster"
   - Adiciona itens ao cluster via doodad browser (composites sao convertidos para cluster tiles)
   - Clica "Preview Cluster..." para abrir o ClusterPreviewWindow
   - No ClusterPreviewWindow, visualiza o grid e opcionalmente define o ponto central clicando em uma celula
4. Configura instancias (se centered):
   - Define Instance Count (quantas copias do cluster colocar)
   - Define Instance Min Distance (distancia minima entre copias)
5. Clica "Preview" para ver resultado
6. Clica "Apply" para aplicar (suporta undo)

---

## TODO / Melhorias Futuras

- [ ] Renderizar preview no mapa (atualmente so estatisticas)
- [ ] FloodFill para selecao de area
- [ ] Edicao inline de weight na lista de itens
- [ ] Copiar/colar regras entre presets
- [ ] Hotkey para aplicacao rapida
- [ ] Suporte a multiplos Z-levels
- [x] Cluster mode para placement de grupos de itens em grid
- [x] ClusterPreviewWindow para visualizacao e configuracao de clusters
- [ ] Drag & drop para reordenar cluster tiles no grid
- [ ] Cluster templates (salvar/carregar clusters reutilizaveis)
