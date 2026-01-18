# Sistema de Bordas Múltiplas por Tile

Este documento descreve as novas funcionalidades implementadas no sistema de bordas de terreno do editor de mapas.

## Visão Geral

Foram adicionadas três novas capacidades ao sistema de bordas:

1. **Bordas múltiplas por tile** - Suporte a múltiplos IDs de borda separados por vírgula
2. **Atributo `not-to`** - Exclusão de bordas para brushes específicos
3. **Ordenação por camadas** - Controle de sobreposição via `layer_order`

---

## 1. Bordas Múltiplas por Tile

### Descrição

Agora é possível especificar múltiplos IDs de borda em um único elemento `<border>`, separados por vírgula. Isso permite que um mesmo terreno aplique várias bordas simultaneamente quando faz fronteira com outro terreno.

### Sintaxe XML

```xml
<!-- Antes: apenas um ID por border -->
<border align="outer" id="261"/>
<border align="outer" id="159"/>

<!-- Agora: múltiplos IDs em uma única declaração -->
<border align="outer" id="261,159"/>
```

### Exemplo de Uso

```xml
<brush name="brown sea floor" type="ground" server_lookid="15010" z-order="2600">
    <item id="15010" chance="2500"/>
    <item id="15011" chance="2500"/>

    <!-- Aplica duas bordas: stone ice (261) e a borda padrão (159) -->
    <border align="outer" id="261,159"/>
    <border align="outer" to="none" id="261,159"/>
</brush>
```

### Arquivos Modificados

| Arquivo | Alteração |
|---------|-----------|
| `source/ground_brush.cpp` | Parsing de IDs separados por vírgula, criação de múltiplos `BorderBlock` |
| `source/ground_brush.h` | Nova função `getBrushesTo()` que retorna vetor de bordas |

### Detalhes da Implementação

**Em `ground_brush.cpp` (linhas 134-156):**

```cpp
// Parse comma-separated IDs
std::string idStr = attribute.as_string();
std::stringstream ss(idStr);
std::string token;

while(std::getline(ss, token, ',')) {
    // Trim whitespace
    size_t start = token.find_first_not_of(" \t");
    size_t end = token.find_last_not_of(" \t");
    if(start == std::string::npos) continue;
    token = token.substr(start, end - start + 1);

    int32_t id = std::stoi(token);
    if(id == 0) {
        autoBorders.push_back(nullptr);
    } else {
        auto it = g_brushes.borders.find(id);
        if(it == g_brushes.borders.end() || !it->second) {
            warnings.push_back("\nCould not find border id " + std::to_string(id));
            continue;
        }
        autoBorders.push_back(it->second);
    }
}
```

---

## 2. Atributo `not-to` (Exclusão de Bordas)

### Descrição

O novo atributo `not-to` permite excluir a aplicação de uma borda quando o terreno faz fronteira com brushes específicos. Isso é útil para criar bordas especiais entre certos tipos de terreno.

### Sintaxe XML

```xml
<border align="outer" id="238" not-to="grotto"/>
<border align="outer" id="238" not-to="grotto,cave,dungeon"/>
```

### Exemplo de Uso

```xml
<brush name="new ice mountain" type="ground" server_lookid="44700" z-order="9901">
    <item id="44700" chance="1"/>

    <!-- Usa borda 238 para todos EXCETO "grotto" -->
    <border align="outer" id="238" not-to="grotto"/>
    <border align="outer" to="none" id="238" not-to="grotto"/>

    <!-- Usa borda especial 262 APENAS para "grotto" -->
    <border align="outer" to="grotto" id="262"/>
</brush>
```

### Arquivos Modificados

| Arquivo | Alteração |
|---------|-----------|
| `source/ground_brush.cpp` | Parsing do atributo `not-to`, função `isExcludedBrush()` |
| `source/ground_brush.h` | Novo campo `std::vector<uint32_t> not_to` em `BorderBlock` |

### Detalhes da Implementação

**Estrutura `BorderBlock` em `ground_brush.h`:**

```cpp
struct BorderBlock {
    bool outer;
    bool super;
    uint32_t to;
    std::vector<uint32_t> not_to; // Brushes to exclude from this border
    int32_t layer_order;
    AutoBorder* autoborder;
    std::vector<SpecificCaseBlock*> specific_cases;
};
```

**Parsing em `ground_brush.cpp` (linhas 193-212):**

```cpp
// Parse not-to attribute for exclusions (comma-separated list of brush names)
std::vector<uint32_t> notToValues;
if((attribute = childNode.attribute("not-to"))) {
    std::string notToStr = attribute.as_string();
    std::stringstream ss(notToStr);
    std::string brushName;
    while(std::getline(ss, brushName, ',')) {
        // Trim whitespace
        size_t start = brushName.find_first_not_of(" \t");
        size_t end = brushName.find_last_not_of(" \t");
        if(start != std::string::npos && end != std::string::npos) {
            brushName = brushName.substr(start, end - start + 1);
        }
        if(!brushName.empty()) {
            Brush* notToBrush = g_brushes.getBrush(brushName);
            if(!notToBrush) {
                warnings.push_back("Not-to brush " + wxstr(brushName) + " doesn't exist.");
            } else {
                notToValues.push_back(notToBrush->getID());
            }
        }
    }
}
```

**Função de verificação em `ground_brush.cpp` (linhas 627-635):**

```cpp
// Helper function to check if a brush ID is in the not_to exclusion list
static bool isExcludedBrush(const GroundBrush::BorderBlock* bb, uint32_t brushId) {
    for(uint32_t excludedId : bb->not_to) {
        if(excludedId == brushId) {
            return true;
        }
    }
    return false;
}
```

---

## 3. Ordenação por Camadas (layer_order)

### Descrição

Quando múltiplas bordas são aplicadas ao mesmo tile com o mesmo `z-order`, o `layer_order` determina a ordem de renderização. Bordas com `layer_order` maior são desenhadas por cima.

### Comportamento

- O `layer_order` é atribuído automaticamente com base na ordem dos IDs na lista separada por vírgula
- O primeiro ID recebe `layer_order = 0`, o segundo `layer_order = 1`, etc.
- A ordenação final considera primeiro o `z` (z-order do brush) e depois o `layer_order`

### Exemplo

```xml
<!-- ID 261 terá layer_order=0, ID 159 terá layer_order=1 -->
<!-- Resultado: borda 159 será renderizada ACIMA da borda 261 -->
<border align="outer" id="261,159"/>
```

### Arquivos Modificados

| Arquivo | Alteração |
|---------|-----------|
| `source/ground_brush.h` | Novo campo `layer_order` em `BorderBlock` e `BorderCluster`, operador `<` modificado |
| `source/ground_brush.cpp` | Atribuição de `layer_order` durante parsing e aplicação de bordas |

### Detalhes da Implementação

**Estrutura `BorderCluster` em `ground_brush.h`:**

```cpp
struct BorderCluster {
    uint32_t alignment;
    int32_t z;
    int32_t layer_order; // Order within same z-level
    const AutoBorder* border;

    bool operator<(const BorderCluster& other) const {
        if(z != other.z) return z < other.z;
        return layer_order < other.layer_order;
    }
};
```

**Atribuição de layer_order em `ground_brush.cpp` (linhas 256-268):**

```cpp
// Create BorderBlock for each border ID
std::vector<BorderBlock*> createdBlocks;
int32_t layerOrder = 0;
for(AutoBorder* autoBorder : autoBorders) {
    BorderBlock* borderBlock = newd BorderBlock;
    borderBlock->super = isSuper;
    borderBlock->autoborder = autoBorder;
    borderBlock->to = toValue;
    borderBlock->not_to = notToValues;
    borderBlock->outer = isOuter;
    borderBlock->layer_order = layerOrder++;

    createdBlocks.push_back(borderBlock);
    borders.push_back(borderBlock);
}
```

---

## Resumo de Arquivos Modificados

### `source/ground_brush.h`

```diff
+ #include <vector>

  struct BorderBlock {
      bool outer;
      bool super;
      uint32_t to;
+     std::vector<uint32_t> not_to; // Brushes to exclude from this border
+     int32_t layer_order; // Order within same z-level (0 = bottom, higher = on top)
      AutoBorder* autoborder;
      std::vector<SpecificCaseBlock*> specific_cases;
  };

  struct BorderCluster {
      uint32_t alignment;
      int32_t z;
+     int32_t layer_order; // Order within same z-level
      const AutoBorder* border;

      bool operator<(const BorderCluster& other) const {
-         return other.z > z;
+         if(z != other.z) return z < other.z;
+         return layer_order < other.layer_order;
      }
  };

+ static std::vector<const BorderBlock*> getBrushesTo(GroundBrush* first, GroundBrush* second);
```

### `source/ground_brush.cpp`

**Includes adicionados:**
```cpp
#include <sstream>
#include "carpet_brush.h"
#include "settings.h"
```

**Funções modificadas/adicionadas:**
- `GroundBrush::load()` - Parsing de múltiplos IDs e atributo `not-to`
- `isExcludedBrush()` - Nova função helper para verificar exclusões
- `GroundBrush::getBrushTo()` - Atualizada para verificar exclusões
- `GroundBrush::getBrushesTo()` - Nova função que retorna todas as bordas aplicáveis
- `GroundBrush::doBorders()` - Atualizada para usar `getBrushesTo()` e `layer_order`

---

## Exemplos Completos de Uso

### Exemplo 1: Terreno com Múltiplas Bordas

```xml
<brush name="ice ground 3" type="ground" server_lookid="44405" z-order="2000">
    <item id="44405" chance="2"/>
    <item id="44406" chance="1"/>

    <!-- Aplica borda de gelo de pedra -->
    <border align="outer" id="261"/>
    <border align="outer" to="none" id="261"/>
</brush>
```

### Exemplo 2: Bordas Condicionais com Exclusão

```xml
<brush name="new ice mountain" type="ground" server_lookid="44700" z-order="9901">
    <item id="44700" chance="1"/>

    <!-- Borda padrão para todos exceto grotto -->
    <border align="outer" id="238" not-to="grotto"/>
    <border align="outer" to="none" id="238" not-to="grotto"/>

    <!-- Borda especial apenas para grotto -->
    <border align="outer" to="grotto" id="262"/>
</brush>
```

### Exemplo 3: Múltiplas Bordas com Ordenação

```xml
<brush name="brown sea floor" type="ground" server_lookid="15010" z-order="2600">
    <item id="15010" chance="2500"/>

    <!-- Borda 261 (layer 0) renderizada primeiro, depois 159 (layer 1) por cima -->
    <border align="outer" id="261,159"/>
    <border align="outer" to="none" id="261,159"/>
</brush>
```

---

## Compatibilidade

Estas mudanças são **retrocompatíveis**. Arquivos XML existentes que usam a sintaxe antiga (um ID por elemento `<border>`) continuarão funcionando normalmente.
