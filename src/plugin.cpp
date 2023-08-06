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

#define RES_DIR(a) (std::string("D:/a/textures/bedrock-samples/resource_pack/") + a)
// #define RES_DIR(a)                                                                                                     \
//     (std::string(                                                                                                      \
//          "C:\\Users\\OEOTYAN\\AppData\\Local\\Packages\\Microsoft.MinecraftUWP_8wekyb3d8bbwe\\LocalState\\games\\com." \
//          "mojang\\resource_packs\\XeKrSquarePattern-BE\\") +                                                           \
//      a)

nlohmann::json json;
nlohmann::json jblocks;
nlohmann::json jtextures;
nlohmann::json jnewcolormap;

std::unordered_set<class Block*> bset;

enum class BlockRenderLayer : __int32 {
    double_sided = 0x0,
    ray_traced_water = 0x1,
    blend = 0x2,
    opaque = 0x3,
    optional_alphatest = 0x4,
    alphatest = 0x5,
    seasons_opaque = 0x6,
    seasons_optional_alphatest = 0x7,
    alphatest_single_side = 0x8,
    endportal = 0x9,
    barrier = 0xA,
    light = 0xB,
    structure_void = 0xC,
    count = 0xD,
};

std::string_view getRenderLayerStr(enum class BlockRenderLayer layer) {
    return magic_enum::enum_name(layer);
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
    std::string upTex = "unknown";
    mce::Color color;

    bj["extra_data"]["use_grass_color"] = std::unordered_set<std::string>{"grass"}.contains(textureName);

    bj["extra_data"]["use_leaves_color"] =
        std::unordered_set<std::string>{"leaves", "leaves2", "mangrove_leaves", "vine"}.contains(textureName);

    bj["extra_data"]["use_water_color"] =
        std::unordered_set<std::string>{"water", "flowing_water"}.contains(textureName);

    bool doNotUseCarried = bj["extra_data"]["use_grass_color"] || bj["extra_data"]["use_leaves_color"];

    if (!jblocks.contains(textureName)) {
        logger.warn(textureName);
    } else {
        // logger.info(textureName);
        nlohmann::json textureMap;
        if (jblocks[textureName].contains("carried_textures") && !doNotUseCarried) {
            textureMap = jblocks[textureName]["carried_textures"];
        } else if (jblocks[textureName].contains("textures")) {
            textureMap = jblocks[textureName]["textures"];
        } else {
            textureMap = "";
        }
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
                    color += mce::Color(data[k4], data[k4 + 1], data[k4 + 2], data[k4 + 3]).sRGBToLinear() *
                             (data[k4 + 3] / 255.0f);
                    totalAlpha += data[k4 + 3] / 255.0f;
                }
                color /= totalAlpha;
                color = color.LinearTosRGB();
                color.a = totalAlpha / (width * height);
            } else {
                if (block->getTypeName() != "minecraft:air") {
                    logger.error(RES_DIR(upTex));
                }
                upTex = "unknown";
            }
        }
    }
    bj["extra_data"]["textures"] = upTex;
    bj["extra_data"]["color"] = {color.r, color.g, color.b, color.a};

    bj.erase("version");

    // for (auto& a : jnewcolormap) {
    //     if (a == bjc){
    //         return;
    //     }
    // }

    json.push_back(bj);
}

void PluginInit() {
    // Your code here
    std::ifstream(RES_DIR("blocks.json")) >> jblocks;
    std::ifstream(RES_DIR("textures/terrain_texture.json")) >> jtextures;
    // std::ifstream("D:/Unzip/SAC/bedrock-server-1.20.1.02/block_color.json") >> jnewcolormap;

    // for (auto& a : jnewcolormap) {
    //     a.erase("version");
    // }

    Event::ServerStartedEvent::subscribe([&](const Event::ServerStartedEvent& e) -> bool {
        std::ofstream("block_color.json") << json.dump(4);
        return true;
    });
}
