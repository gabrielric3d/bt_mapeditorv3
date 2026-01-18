# Multiple Borders Per Tile System

This document describes the new features implemented in the map editor's terrain border system.

## Overview

Three new capabilities were added to the border system:

1. **Multiple borders per tile** - Support for multiple border IDs separated by comma
2. **`not-to` attribute** - Border exclusion for specific brushes
3. **Layer ordering** - Overlap control via `layer_order`

---

## 1. Multiple Borders Per Tile

### Description

It is now possible to specify multiple border IDs in a single `<border>` element, separated by comma. This allows a terrain to apply multiple borders simultaneously when bordering another terrain.

### XML Syntax

```xml
<!-- Before: only one ID per border -->
<border align="outer" id="261"/>
<border align="outer" id="159"/>

<!-- Now: multiple IDs in a single declaration -->
<border align="outer" id="261,159"/>
```

### Usage Example

```xml
<brush name="brown sea floor" type="ground" server_lookid="15010" z-order="2600">
    <item id="15010" chance="2500"/>
    <item id="15011" chance="2500"/>

    <!-- Applies two borders: stone ice (261) and the default border (159) -->
    <border align="outer" id="261,159"/>
    <border align="outer" to="none" id="261,159"/>
</brush>
```

### Modified Files

| File | Change |
|------|--------|
| `source/ground_brush.cpp` | Parsing of comma-separated IDs, creation of multiple `BorderBlock` |
| `source/ground_brush.h` | New function `getBrushesTo()` that returns a vector of borders |

### Implementation Details

**In `ground_brush.cpp` (lines 134-156):**

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

## 2. `not-to` Attribute (Border Exclusion)

### Description

The new `not-to` attribute allows excluding a border when the terrain borders specific brushes. This is useful for creating special borders between certain terrain types.

### XML Syntax

```xml
<border align="outer" id="238" not-to="grotto"/>
<border align="outer" id="238" not-to="grotto,cave,dungeon"/>
```

### Usage Example

```xml
<brush name="new ice mountain" type="ground" server_lookid="44700" z-order="9901">
    <item id="44700" chance="1"/>

    <!-- Uses border 238 for all EXCEPT "grotto" -->
    <border align="outer" id="238" not-to="grotto"/>
    <border align="outer" to="none" id="238" not-to="grotto"/>

    <!-- Uses special border 262 ONLY for "grotto" -->
    <border align="outer" to="grotto" id="262"/>
</brush>
```

### Modified Files

| File | Change |
|------|--------|
| `source/ground_brush.cpp` | Parsing of `not-to` attribute, `isExcludedBrush()` function |
| `source/ground_brush.h` | New field `std::vector<uint32_t> not_to` in `BorderBlock` |

### Implementation Details

**`BorderBlock` structure in `ground_brush.h`:**

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

**Parsing in `ground_brush.cpp` (lines 193-212):**

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

**Verification function in `ground_brush.cpp` (lines 627-635):**

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

## 3. Layer Ordering (layer_order)

### Description

When multiple borders are applied to the same tile with the same `z-order`, the `layer_order` determines the rendering order. Borders with higher `layer_order` are drawn on top.

### Behavior

- The `layer_order` is automatically assigned based on the order of IDs in the comma-separated list
- The first ID receives `layer_order = 0`, the second `layer_order = 1`, etc.
- The final ordering considers first the `z` (brush z-order) and then the `layer_order`

### Example

```xml
<!-- ID 261 will have layer_order=0, ID 159 will have layer_order=1 -->
<!-- Result: border 159 will be rendered ABOVE border 261 -->
<border align="outer" id="261,159"/>
```

### Modified Files

| File | Change |
|------|--------|
| `source/ground_brush.h` | New `layer_order` field in `BorderBlock` and `BorderCluster`, modified `<` operator |
| `source/ground_brush.cpp` | Assignment of `layer_order` during parsing and border application |

### Implementation Details

**`BorderCluster` structure in `ground_brush.h`:**

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

**layer_order assignment in `ground_brush.cpp` (lines 256-268):**

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

## Summary of Modified Files

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

**Added includes:**
```cpp
#include <sstream>
#include "carpet_brush.h"
#include "settings.h"
```

**Modified/added functions:**
- `GroundBrush::load()` - Parsing of multiple IDs and `not-to` attribute
- `isExcludedBrush()` - New helper function to check exclusions
- `GroundBrush::getBrushTo()` - Updated to check exclusions
- `GroundBrush::getBrushesTo()` - New function that returns all applicable borders
- `GroundBrush::doBorders()` - Updated to use `getBrushesTo()` and `layer_order`

---

## Complete Usage Examples

### Example 1: Terrain with Multiple Borders

```xml
<brush name="ice ground 3" type="ground" server_lookid="44405" z-order="2000">
    <item id="44405" chance="2"/>
    <item id="44406" chance="1"/>

    <!-- Applies stone ice border -->
    <border align="outer" id="261"/>
    <border align="outer" to="none" id="261"/>
</brush>
```

### Example 2: Conditional Borders with Exclusion

```xml
<brush name="new ice mountain" type="ground" server_lookid="44700" z-order="9901">
    <item id="44700" chance="1"/>

    <!-- Default border for all except grotto -->
    <border align="outer" id="238" not-to="grotto"/>
    <border align="outer" to="none" id="238" not-to="grotto"/>

    <!-- Special border only for grotto -->
    <border align="outer" to="grotto" id="262"/>
</brush>
```

### Example 3: Multiple Borders with Ordering

```xml
<brush name="brown sea floor" type="ground" server_lookid="15010" z-order="2600">
    <item id="15010" chance="2500"/>

    <!-- Border 261 (layer 0) rendered first, then 159 (layer 1) on top -->
    <border align="outer" id="261,159"/>
    <border align="outer" to="none" id="261,159"/>
</brush>
```

---

## Compatibility

These changes are **backward compatible**. Existing XML files that use the old syntax (one ID per `<border>` element) will continue to work normally.
