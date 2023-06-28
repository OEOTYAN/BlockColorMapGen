/**
 * @file plugin.cpp
 * @brief The main file of the plugin
 */

#include <llapi/LoggerAPI.h>
#include "mc/Block.hpp"
#include "mc/Level.hpp"
#include "HookAPI.h"
#include "ScheduleAPI.h"
#include "EventAPI.h"
#include "stb_image.h"

#include "version.h"

#include "Nlohmann/json.hpp"
#include "magic_enum/magic_enum.hpp"

// We recommend using the global logger.
extern Logger logger;

#define RES_DIR(a) (std::string("D:/code/packs/bedrock-samples/resource_pack/") + a)
// #define RES_DIR(a)                                                                                                     \
//     (std::string(                                                                                                      \
//          "C:\\Users\\OEOTYAN\\AppData\\Local\\Packages\\Microsoft.MinecraftUWP_8wekyb3d8bbwe\\LocalState\\games\\com." \
//          "mojang\\resource_packs\\XeKrSquarePattern-BE\\") +                                                           \
//      a)

nlohmann::json json;
nlohmann::json jblocks;
nlohmann::json jtextures;

std::unordered_set<class Block*> bset;

enum class BlockRenderLayer : __int32 {
    RENDERLAYER_DOUBLE_SIDED = 0x0,
    RENDERLAYER_RAY_TRACED_WATER = 0x1,
    RENDERLAYER_BLEND = 0x2,
    RENDERLAYER_OPAQUE = 0x3,
    RENDERLAYER_OPTIONAL_ALPHATEST = 0x4,
    RENDERLAYER_ALPHATEST = 0x5,
    RENDERLAYER_SEASONS_OPAQUE = 0x6,
    RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST = 0x7,
    RENDERLAYER_ALPHATEST_SINGLE_SIDE = 0x8,
    RENDERLAYER_ENDPORTAL = 0x9,
    RENDERLAYER_BARRIER = 0xA,
    RENDERLAYER_LIGHT = 0xB,
    RENDERLAYER_STRUCTURE_VOID = 0xC,
    _RENDERLAYER_COUNT = 0xD,
};

std::string getRenderLayerStr(enum class BlockRenderLayer layer) {
    switch (layer) {
        case BlockRenderLayer::RENDERLAYER_DOUBLE_SIDED:
            return "double_sided";
        case BlockRenderLayer::RENDERLAYER_RAY_TRACED_WATER:
            return "ray_traced_water";
        case BlockRenderLayer::RENDERLAYER_BLEND:
            return "blend";
        case BlockRenderLayer::RENDERLAYER_OPAQUE:
            return "opaque";
        case BlockRenderLayer::RENDERLAYER_OPTIONAL_ALPHATEST:
            return "optional_alphatest";
        case BlockRenderLayer::RENDERLAYER_ALPHATEST:
            return "alphatest";
        case BlockRenderLayer::RENDERLAYER_SEASONS_OPAQUE:
            return "seasons_opaque";
        case BlockRenderLayer::RENDERLAYER_SEASONS_OPTIONAL_ALPHATEST:
            return "seasons_optional_alphatest";
        case BlockRenderLayer::RENDERLAYER_ALPHATEST_SINGLE_SIDE:
            return "alphatest_single_side";
        case BlockRenderLayer::RENDERLAYER_ENDPORTAL:
            return "endportal";
        case BlockRenderLayer::RENDERLAYER_BARRIER:
            return "barrier";
        case BlockRenderLayer::RENDERLAYER_LIGHT:
            return "light";
        case BlockRenderLayer::RENDERLAYER_STRUCTURE_VOID:
            return "structure_void";
        default:
            return "invalid";
    }
}

THook(void, "?setRuntimeId@Block@@IEBAXAEBI@Z", Block* block, unsigned int const& id) {
    original(block, id);

    if (bset.contains(block)) {
        return;
    }
    bset.insert(block);

    auto blockstr = ((CompoundTag&)(block->getSerializationId())).toSNBT(0, SnbtFormat::Minimize);

    auto bj = nlohmann::json::parse(ReplaceStr(ReplaceStr(blockstr, ":1b", ":true"), ":0b", ":false"));

    bj["extra_data"]["render_layer"] = getRenderLayerStr(block->getRenderLayer());

    auto textureName = block->getTypeName().substr(10);

    ReplaceStr(textureName, "_block_slab", "_slab");

    if (!jblocks.contains(textureName)) {
        std::string newName;
        auto vec = SplitStrWithPattern(textureName, "_");
        for (auto& v : vec) {
            if (v != vec.front()) {
                v[0] = std::toupper(v[0]);
                newName += v;
            } else {
                newName = v;
            }
        }
        if (jblocks.contains(newName)) {
            textureName = newName;
        }
    }
    if (!jblocks.contains(textureName)) {
        logger.warn(textureName);
    } else {
        // logger.info(textureName);
        nlohmann::json textureMap;
        if (jblocks[textureName].contains("carried_textures")) {
            textureMap = jblocks[textureName]["carried_textures"];
        } else if (jblocks[textureName].contains("textures")) {
            textureMap = jblocks[textureName]["textures"];
        } else {
            textureMap = "";
        }
        std::string upTex;
        if (textureMap.is_string()) {
            upTex = textureMap;
        } else {
            upTex = textureMap["up"];
        }
        if (jtextures["texture_data"].contains(upTex)) {
            auto texturePathMap = jtextures["texture_data"][upTex]["textures"];
            if (texturePathMap.is_string()) {
                upTex = texturePathMap;
            } else {
                auto texturePath = texturePathMap[block->getVariant() % texturePathMap.size()];
                if (texturePath.is_string()) {
                    upTex = texturePath;
                } else {
                    upTex = texturePath["path"];
                }
            }
        } else {
            upTex = "textures/blocks/" + upTex;
        }
        mce::Color color;
        if (upTex != "") {
            int width, height, channel;
            unsigned char* data = stbi_load(RES_DIR(upTex + ".png").c_str(), &width, &height, &channel, 4);
            if (data == nullptr) {
                data = stbi_load(RES_DIR(upTex + ".tga").c_str(), &width, &height, &channel, 4);
            }
            if (data != nullptr) {
                float totalAlpha = 0;
                for (int i = 0; i < width * height; i++) {
                    auto k4 = 4 * i;
                    color += mce::Color(data[k4], data[k4 + 1], data[k4 + 2], data[k4 + 3]) * (data[k4 + 3] / 255.0f);
                    totalAlpha += data[k4 + 3] / 255.0f;
                }
                color /= totalAlpha;
                color.a = totalAlpha / (width * height);
            } else {
                logger.error(RES_DIR(upTex));
            }
        }
        bj["extra_data"]["textures"] = upTex;
        bj["extra_data"]["color"] = {color.r, color.g, color.b, color.a};
    }

    json.push_back(bj);
}

void PluginInit() {
    // Your code here
    std::ifstream(RES_DIR("blocks.json")) >> jblocks;
    std::ifstream(RES_DIR("textures/terrain_texture.json")) >> jtextures;

    Event::ServerStartedEvent::subscribe([&](const Event::ServerStartedEvent& e) -> bool {
        std::ofstream("block_color.json") << json.dump(4);
        return true;
    });
}
