#ifndef TERRAIN_COMPONENT_H
#define TERRAIN_COMPONENT_H

#define MAX_TERRAINGRID 16
#define MAX_TERRAINNODES 5460
// to get best terrainnodes number, use this formula:
// ( 4⁰ + 4¹ + 4² + ... + 4^(levels-1) ) * rootGridSize²

#include "buffer/InterleavedBuffer.h"
#include "buffer/IndexBuffer.h"
#include "util/Material.h"
#include "Supernova.h"

namespace Supernova{

    struct TerrainNodeIndex{
        unsigned int indexCount;
        unsigned int indexOffset;
    };

    struct TerrainNode{
        Attribute indexAttribute;

        //-----u_vs_terrainNodeParams
        Vector2 position = Vector2(0, 0);
        float size = 0;
        float currentRange = 0;
        float resolution = 0; //int
        uint8_t _pad_20[12];
        //-----

        size_t childs[4];
        bool hasChilds = false;

        float maxHeight = 0;
        float minHeight = 0;
        
        float visible = false;
    };

    struct TerrainComponent{
        bool loaded = false;

        Material material;

        InterleavedBuffer buffer;
        IndexBuffer indices;

        ObjectRender render;
        ObjectRender depthRender;

        std::shared_ptr<ShaderRender> shader;
        std::shared_ptr<ShaderRender> depthShader;

        std::string shaderProperties;
        std::string depthShaderProperties;

        int slotVSParams = -1;
        int slotFSParams = -1;
        int slotFSLighting = -1;
        int slotFSFog = -1;
        int slotVSShadows = -1;
        int slotFSShadows = -1;
        int slotVSTerrain = -1;
        int slotVSTerrainNode = -1;

        bool castShadows = true;

        Texture heightMap;

        TerrainNodeIndex fullResNode = {0, 0};
        TerrainNodeIndex halfResNode = {0, 0};

        bool autoSetRanges = true;
        bool heightMapLoaded = false;

        Vector2 offset;
        std::vector<float> ranges;

        TerrainNode nodes[MAX_TERRAINNODES];
        unsigned int numNodes = 0;

        size_t grid[MAX_TERRAINGRID]; //root nodes

        //-----u_vs_terrainParams
        float terrainSize = 200;
        float maxHeight = 10;
        float resolution = 32; //int
        uint8_t _pad_12[4];
        Vector3 eyePos;
        uint8_t _pad_28[4];
        //-----

        int rootGridSize = 2;
        int levels = 6;

        int textureBaseTiles = 1;
        int textureDetailTiles = 20;

        bool needUpdateTerrain = true;
        bool needUpdateTexture = false;
        bool needReload = false;
    };
    
}

#endif //TERRAIN_COMPONENT_H