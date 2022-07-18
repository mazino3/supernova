uniform sampler2D u_heightData;

uniform u_vs_terrainParams {
    float size;
    float maxHeight;
    float resolution;
    vec3 eyePos;
} terrain;

uniform u_vs_terrainNodeParams {
    vec2 pos;
    float size;
    float range;
    float resolution; //int
} terrainNode;

float morphFactor;

vec2 morphVertex(vec2 gridPos, vec2 worldPos, float morph) {
    vec2 gridDim = vec2(terrainNode.resolution, terrainNode.resolution);

    vec2 fracPart = fract(gridPos * gridDim.xy * 0.5) * 2.0 / gridDim.xy;
    return worldPos - fracPart * terrainNode.size * morph;
}

float getHeight(vec3 position) {
    return texture(u_heightData, (position.xz + (terrain.size/2.0)) / terrain.size).r * terrain.maxHeight;
}

// must be called BEFORE getTerrainNormal because morphValue
vec3 getTerrainPosition(vec3 pos){
    pos = terrainNode.size * pos;
    pos = pos + vec3(terrainNode.pos[0], 0.0, terrainNode.pos[1]);

    pos = vec3(pos.x, getHeight(pos), pos.z);

    float morphStart = 0.0;
    float morphEnd = 0.4;

    float dist = distance(terrain.eyePos, vec3(pbrParams.modelMatrix * vec4(pos, 1.0)));

    float nextlevel_thresh = ((terrainNode.range - dist) / terrainNode.size * terrainNode.resolution / float(terrain.resolution));
    morphFactor = 1.0 - smoothstep(morphStart, morphEnd, nextlevel_thresh);

    pos.xz = morphVertex(a_position.xz, pos.xz, morphFactor);

    pos = vec3(pos.x, getHeight(pos), pos.z);

    return pos;
}

vec3 getTerrainNormal(vec3 normal, vec3 position){
    #ifdef HAS_NORMALS
        float delta = (morphFactor + 1.0) * terrainNode.size / float(terrainNode.resolution);

        vec3 dA = delta * normalize(cross(normal.yzx, normal));
        vec3 dB = delta * normalize(cross(dA, normal));
        vec3 p = position;
        vec3 pA = position + dA;
        vec3 pB = position + dB;

        float h = getHeight(position);
        float hA = getHeight(pA);
        float hB = getHeight(pB);

        p += normal * h;
        pA += normal * hA;
        pB += normal * hB;

        normal = normalize(cross(pB - p, pA - p));
    #endif

    return normal;
}

//v_TerrainTextureCoords = (localPos.xz + (u_terrainSize/2.0)) / u_terrainSize;
//v_TerrainTextureDetailTiled = v_TerrainTextureCoords * float(u_terrainTextureDetailTiles);
//terrainTextureBaseTiled = v_TerrainTextureCoords * float(u_terrainTextureBaseTiles);