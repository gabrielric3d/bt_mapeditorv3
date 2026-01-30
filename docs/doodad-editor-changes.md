# Doodad Editor - Alteracoes no Codigo Fonte

Este documento descreve todas as alteracoes feitas no codigo fonte para implementar o Doodad Editor GUI.

## Arquivos Criados

### 1. `source/doodad_editor_window.h`

Arquivo de cabecalho com as definicoes das classes do editor.

**Estruturas de dados:**

```cpp
// Constantes do grid
const int DOODAD_GRID_SIZE = 10;      // Grid 10x10
const int DOODAD_GRID_CENTER = 5;     // Centro (0,0) na posicao 5
const int DOODAD_CELL_SIZE = 38;      // Tamanho de cada celula em pixels

// Paginacao
const int DOODADS_PER_PAGE = 50;

// Representa um item em uma tile do composite
struct DoodadTileItem {
    int x;           // Offset X relativo ao centro (-5 a +4)
    int y;           // Offset Y relativo ao centro (-5 a +4)
    int z;           // Offset Z (geralmente 0)
    uint16_t itemId;
};

// Representa uma configuracao composite completa
struct DoodadComposite {
    int chance;
    std::vector<DoodadTileItem> tiles;
};

// Representa um item simples (nao-composite)
struct DoodadSingleItem {
    uint16_t itemId;
    int chance;
};

// Info sobre um doodad brush para a lista
struct DoodadBrushInfo {
    wxString name;
    int compositeCount;
    int singleCount;
};
```

**Classes:**

1. **DoodadEditorDialog** - Dialog principal do editor
   - Painel esquerdo com lista paginada de doodads
   - Painel direito com editor (propriedades, single items, composites)
   - Geracao de XML para clipboard

2. **DoodadGridPanel** - Painel do grid 10x10 para edicao de composites
   - Renderizacao de sprites
   - Selecao de celulas
   - Conversao entre coordenadas de grid e relativas

3. **DoodadPreviewPanel** - Painel de preview do composite
   - Visualizacao centralizada do layout

---

### 2. `source/doodad_editor_window.cpp`

Implementacao completa do editor (~1200 linhas).

**IDs de controles:**

```cpp
enum {
    ID_SINGLE_ITEM_LIST = wxID_HIGHEST + 1,
    ID_COMPOSITES_LIST,
    ID_ADD_SINGLE_ITEM,
    ID_REMOVE_SINGLE_ITEM,
    ID_BROWSE_SINGLE_ITEM,
    ID_NEW_COMPOSITE,
    ID_REMOVE_COMPOSITE,
    ID_CLEAR_GRID,
    ID_COMPOSITE_CHANCE,
    ID_GRID_ITEM_ID,
    ID_BROWSE_GRID_ITEM,
    ID_LOAD_TIMER,
    ID_FILTER_TEXT,
    ID_DOODAD_LIST,
    ID_PREV_PAGE,
    ID_NEXT_PAGE,
    ID_CREATE_NEW
};
```

**Funcionalidades principais:**

- `CreateGUIControls()` - Cria toda a interface grafica
- `LoadExistingDoodads()` - Carrega todos os doodad brushes existentes
- `UpdateDoodadList()` - Atualiza a lista com paginacao
- `OnFilterChanged()` - Filtra doodads por nome
- `LoadDoodadBrush()` - Carrega um doodad brush para edicao
- `GenerateXML()` - Gera o XML para o clipboard
- `ApplyItemToGridPosition()` - Aplica item na posicao do grid
- `UpdateCompositeFromGrid()` / `UpdateGridFromComposite()` - Sincroniza dados

**Drop Target para arrastar items:**

```cpp
class DoodadGridDropTarget : public wxTextDropTarget {
    bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;
};
```

---

## Arquivos Modificados

### 3. `source/doodad_brush.h`

Adicionados metodos de acesso para o editor:

```cpp
// Access methods for editor
int getCompositeCount(int variation) const;
const CompositeTileList& getCompositeAt(int variation, int index) const;
int getCompositeChanceAt(int variation, int index) const;
int getSingleCount(int variation) const;
uint16_t getSingleItemId(int variation, int index) const;
int getSingleItemChance(int variation, int index) const;
```

---

### 4. `source/doodad_brush.cpp`

Implementacao dos metodos de acesso:

```cpp
int DoodadBrush::getCompositeCount(int variation) const
{
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return 0;
    }
    return alternatives[variation]->composite_items.size();
}

const CompositeTileList& DoodadBrush::getCompositeAt(int variation, int index) const
{
    static CompositeTileList empty;
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return empty;
    }
    if (index < 0 || index >= (int)alternatives[variation]->composite_items.size()) {
        return empty;
    }
    return alternatives[variation]->composite_items[index].items;
}

int DoodadBrush::getCompositeChanceAt(int variation, int index) const
{
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return 0;
    }
    if (index < 0 || index >= (int)alternatives[variation]->composite_items.size()) {
        return 0;
    }
    return alternatives[variation]->composite_items[index].chance;
}

int DoodadBrush::getSingleCount(int variation) const
{
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return 0;
    }
    return alternatives[variation]->single_items.size();
}

uint16_t DoodadBrush::getSingleItemId(int variation, int index) const
{
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return 0;
    }
    if (index < 0 || index >= (int)alternatives[variation]->single_items.size()) {
        return 0;
    }
    return alternatives[variation]->single_items[index].item->getID();
}

int DoodadBrush::getSingleItemChance(int variation, int index) const
{
    if (variation < 0 || variation >= (int)alternatives.size()) {
        return 0;
    }
    if (index < 0 || index >= (int)alternatives[variation]->single_items.size()) {
        return 0;
    }
    return alternatives[variation]->single_items[index].chance;
}
```

---

### 5. `source/palette_brushlist.h`

Adicionada declaracao do handler do botao:

```cpp
void OnClickCreateDoodad(wxCommandEvent& event);
```

---

### 6. `source/palette_brushlist.cpp`

Adicionado include:

```cpp
#include "doodad_editor_window.h"
```

Adicionado evento no event table:

```cpp
EVT_BUTTON(PALETTE_DOODAD_BRUSH_BUTTON, BrushPalettePanel::OnClickCreateDoodad)
```

Adicionado botao "Create Doodad" no construtor (similar ao "Create Border"):

```cpp
if (palette_type == TILESET_DOODAD) {
    wxButton* createDoodadButton = newd wxButton(this, PALETTE_DOODAD_BRUSH_BUTTON, "Create Doodad");
    subSizer->Add(createDoodadButton, 0, wxEXPAND);
}
```

Implementacao do handler:

```cpp
void BrushPalettePanel::OnClickCreateDoodad(wxCommandEvent& WXUNUSED(event))
{
    DoodadEditorDialog* dialog = newd DoodadEditorDialog(g_gui.root, "Doodad Brush Editor");
    dialog->Show();
}
```

---

### 7. `source/gui_ids.h`

Adicionado ID para o botao:

```cpp
PALETTE_DOODAD_BRUSH_BUTTON,
```

---

## Arquivos de Build

### 8. `source/CMakeLists.txt`

Adicionados os novos arquivos:

```cmake
# Na secao de headers:
${CMAKE_CURRENT_LIST_DIR}/doodad_editor_window.h

# Na secao de sources:
${CMAKE_CURRENT_LIST_DIR}/doodad_editor_window.cpp
```

---

### 9. `vcproj/Project/RME.vcxproj`

Adicionadas entradas para Visual Studio:

```xml
<ClInclude Include="..\..\source\doodad_editor_window.h" />
<ClCompile Include="..\..\source\doodad_editor_window.cpp" />
```

---

### 10. `vcproj/Project/RME.vcxproj.filters`

Adicionados filtros para organizacao no Visual Studio:

```xml
<ClInclude Include="..\..\source\doodad_editor_window.h">
  <Filter>Header Files</Filter>
</ClInclude>

<ClCompile Include="..\..\source\doodad_editor_window.cpp">
  <Filter>Source Files</Filter>
</ClCompile>
```

---

## Funcionalidades do Editor

### Painel Esquerdo - Lista de Doodads

- **Filtro de busca**: Pesquisa doodads por nome (case-insensitive)
- **Lista paginada**: Exibe 50 doodads por pagina
- **Informacoes**: Mostra "C:X S:Y" (X composites, Y single items)
- **Navegacao**: Botoes < e > para mudar de pagina
- **Criar Novo**: Botao para iniciar um novo doodad

### Painel Direito - Editor

#### Secao de Propriedades
- **Brush Name**: Nome do doodad brush
- **Server Look ID**: ID do item para visualizacao
- **Draggable**: Se pode ser arrastado
- **On Blocking**: Se pode ser colocado em tiles bloqueadas
- **On Duplicate**: Se permite duplicatas
- **Redo Borders**: Se refaz bordas ao colocar
- **One Size**: Se usa tamanho unico
- **Thickness**: Espessura (floor/ceiling)

#### Aba Single Items
- Lista de items simples com ID e chance
- Controles para adicionar/remover items
- Botao Browse para buscar items

#### Aba Composites
- Lista de composites com chance e contagem de tiles
- Grid 10x10 para posicionar items
  - Centro (0,0) marcado em verde
  - Celula selecionada marcada em amarelo
  - Coordenadas relativas mostradas nas bordas
- Preview do composite atual
- Controles para novo/remover composite
- Item ID input com Browse

### Saida XML

O editor gera XML no formato:

```xml
<brush name="nome_do_doodad" type="doodad" server_lookid="1234"
       draggable="true" on_blocking="false" thickness="25/100">
    <item id="1234" chance="10" />
    <item id="1235" chance="5" />
    <composite chance="10">
        <tile x="0" y="0"> <item id="1234" /> </tile>
        <tile x="1" y="0"> <item id="1235" /> </tile>
        <tile x="0" y="1"> <item id="1236" /> </tile>
    </composite>
</brush>
```

O XML e copiado para o clipboard para ser colado no arquivo `doodads.xml`.

---

## Dependencias

O editor utiliza as seguintes bibliotecas wxWidgets:
- `wx/dialog.h`
- `wx/sizer.h`, `wx/gbsizer.h`
- `wx/statbox.h`, `wx/stattext.h`, `wx/statline.h`
- `wx/textctrl.h`, `wx/button.h`
- `wx/listbox.h`, `wx/listctrl.h`
- `wx/spinctrl.h`, `wx/checkbox.h`
- `wx/notebook.h`
- `wx/dcbuffer.h`
- `wx/dnd.h` (drag and drop)
- `wx/clipbrd.h` (clipboard)
- `wx/timer.h` (carregamento assincrono)

---

## Notas de Implementacao

1. **Carregamento Assincrono**: O carregamento dos doodads e feito via `wxTimer` com delay de 50ms para evitar congelamento da janela.

2. **Renderizacao de Sprites**: Utiliza `g_gui.gfx.getSprite(type.clientID)` para obter sprites dos items.

3. **Paginacao**: Lista exibe 50 items por pagina para melhor performance com grandes quantidades de doodads.

4. **Coordenadas do Grid**: O grid usa coordenadas 0-9, convertidas para coordenadas relativas -5 a +4 para o XML.

5. **Preview em Tempo Real**: O painel de preview atualiza automaticamente ao modificar o composite.
