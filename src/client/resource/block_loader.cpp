#include "block_loader.hpp"

#include "client/application.hpp"
#include "util/file.hpp"

#define OVERRIDE(field, value)\
    for (auto& block: blocks) block->##field = value

#define PUSH_BACK(field, value)\
    for (auto& block: blocks) block->##field.push_back(value)

namespace cybrion
{
    BlockLoader* BlockLoader::s_blockLoader = nullptr;

    BlockLoader::BlockLoader()
    {
        s_blockLoader = this;
    }

    BlockLoader& BlockLoader::Get()
    {
        return *s_blockLoader;
    }

    void BlockLoader::load()
    {
        loadTextures();
        loadModels();
        loadConfigFiles();
    }

    u32 BlockLoader::getTextureId(const string& name)
    {
        return m_textureIdMap[name];
    }

    ref<BlockMesh> BlockLoader::getMesh(const string& name) const
    {
        return m_meshMap.find(name)->second;
    }

    void BlockLoader::bindTextureArray()
    {
        m_textureArray.bind(0);
    }

    void BlockLoader::loadConfigFiles()
    {
        string folderPath = Application::Get().getResourcePath("configs/blocks/");
    
        for (auto& entry : std::filesystem::directory_iterator(folderPath))
        {
            string path = entry.path().string();
            
            if (loadConfigFile(path))
                CYBRION_GAME_TRACE("Loaded file {}", path);
            else
                CYBRION_GAME_WARN("Cannot load file {}", path);
        }
    }

    void BlockLoader::loadTextures()
    {
        constexpr i32 BLOCK_TEXTURE_SIZE = 256;
        string folderPath = Application::Get().getResourcePath("textures/blocks/");

        // count number of block textures
        u32 layerCount = 0;
        for (auto& entry : std::filesystem::directory_iterator(folderPath)) {
            layerCount += 1;
        }

        // init texture array
        m_textureArray.init(BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, layerCount, 4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

        // loading textures
        stbi_set_flip_vertically_on_load(true);
        for (auto& entry : std::filesystem::directory_iterator(folderPath))
        {
            string path = entry.path().string();
            string name = entry.path().stem().string();

            if (entry.path().extension() != ".png")
                continue;

            CYBRION_CLIENT_TRACE("Loaded block texture: {}", name);

            i32 width, height, nchannels;
            u8* data = stbi_load(path.c_str(), &width, &height, &nchannels, STBI_rgb_alpha);

            if ((width & (width - 1)) || (height & (height - 1)))
            {
                CYBRION_CLIENT_ERROR("Incorrect texture size");
                continue;
            }
            
            u32 id = name == "no_texture" ? 0 : m_textureIdMap.size() + (m_textureIdMap.count("no_texture") == 0);
            m_textureIdMap[name] = id;

            u8* resizedData = (u8*)malloc(BLOCK_TEXTURE_SIZE * BLOCK_TEXTURE_SIZE * nchannels);

            stbir_resize_uint8_generic(data, width, height, 0,
                resizedData, BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, 0,
                nchannels, STBIR_FLAG_ALPHA_USES_COLORSPACE, -1, STBIR_EDGE_ZERO,
                STBIR_FILTER_BOX, STBIR_COLORSPACE_LINEAR, NULL);

            m_textureArray.setSubImage(id, resizedData);

            stbi_image_free(data);
            free(resizedData);
        }
    }

    void BlockLoader::loadModels()
    {
        string folderPath = Application::Get().getResourcePath("models/blocks/");

        for (auto& entry : std::filesystem::directory_iterator(folderPath))
        {
            string path = entry.path().string();
            string name = entry.path().stem().string();

            if (entry.path().extension() != ".obj")
                continue;

            CYBRION_CLIENT_TRACE("Loaded block model: {}", name);

            auto model = loadObjFile(path);
            m_meshMap[name] = model;
        }
    }

    ref<BlockMesh> BlockLoader::loadObjFile(const string& path)
    {
        std::ifstream file(path);
        auto model = std::make_shared<BlockMesh>();

        vector<vec3> vpos;
        vector<vec3> vnormal;
        vector<vec2> vtex;

        string sline;
        u32 texId = 0;
        while (std::getline(file, sline))
        {
            std::stringstream line(sline);

            string type;
            line >> type;

            if (type == "#")
                continue;

            if (type == "v")
            {
                vec3 pos;
                line >> pos.x >> pos.y >> pos.z;
                vpos.push_back(pos);
            }
            else if (type == "vn")
            {
                vec3 normal;
                line >> normal.x >> normal.y >> normal.z;
                vnormal.push_back(normal);
            }
            else if (type == "vt")
            {
                vec3 tex;
                line >> tex.x >> tex.y >> tex.z;
                vtex.push_back(tex);
            }
            else if (type == "usemtl")
            {
                line >> texId;
            }
            else if (type == "f")
            {
                string tri;
                while (line >> tri)
                {
                    std::stringstream ss(tri);

                    i32 id[3];
                    for (i32 i = 0; i < 3; ++i)
                    {
                        string s;
                        std::getline(ss, s, '/');
                        id[i] = std::stoi(s);
                    }

                    model->vertices.push_back({
                        vpos[id[0] - 1],
                        vtex[id[1] - 1],
                        vnormal[id[2] - 1],
                        texId
                    });
                }
            }
        }

        file.close();
        return model;
    }

    AABB getAABBFromYAMLNode(const YAML::Node& node)
    {
        return {
            {
                node[0].as<f32>(),
                node[1].as<f32>(),
                node[2].as<f32>()
            },
            {
                node[3].as<f32>(),
                node[4].as<f32>(),
                node[5].as<f32>()
            },
        };
    }

    bool BlockLoader::loadConfigFile(const string& path)
    {
        YAML::Node config = YAML::LoadFile(path);
        BlockType type = StringToEnum<BlockType>(std::filesystem::path(path).stem().string());

        for (auto it : config)
        {
            // handle key
            string key = it.first.as<string>();
            bool is_state = key.starts_with("[") && key.ends_with("]");

            if (is_state)
            {
                // erase "[" and "]"
                key.erase(key.begin());
                key.erase(key.end()-1);

                umap<string, string> stateMap;

                bool validKey = true;
                if (!key.empty())
                {
                    for (auto it0 : std::views::split(key, ','))
                    {
                        auto view = std::views::split(string(it0.begin(), it0.end()), '=');
                        auto it1 = view.begin();
                        string key { (*it1).begin(), (*it1).end() }; it1++;

                        if (it1 == view.end())
                        {
                            CYBRION_CLIENT_ERROR("An error occurred while loading {}", std::filesystem::path(path).filename().string());
                            validKey = false;
                            break;
                        }

                        string value { (*it1).begin(), (*it1).end() };
                        
                        stateMap[key] = value;
                    }
                }

                if (!validKey) continue;

                vector<Block*> blocks;

                Blocks::Get().queryBlocks(type, stateMap, blocks);

                // handle value
                YAML::Node value = it.second;
                for (auto it0 : value)
                {
                    string key = it0.first.as<string>();

                    if (it0.second.IsSequence())
                    {
                        if (key == "model_tex")
                        {
                            vector<u32> vtex;
                            for (auto it1 : it0.second)
                            {
                                u32 texId = getTextureId(it1.as<string>());
                                vtex.push_back(texId);
                            }
                            OVERRIDE(m_modelTextures, vtex);
                        }

                        if (key == "model")
                        {
                            OVERRIDE(m_shape, BlockShape::CUSTOM);
                            for (auto it1: it0.second)
                                PUSH_BACK(m_meshes, getMesh(it1.as<string>()));
                        }

                        if (key == "collision")
                        {
                            vector<AABB> collisionBounds;

                            if (it0.second[0].IsSequence())
                                for (auto it1 : it0.second)
                                    collisionBounds.push_back(getAABBFromYAMLNode(it1));
                            else
                                collisionBounds.push_back(getAABBFromYAMLNode(it0.second));
                            OVERRIDE(m_collisionBounds, collisionBounds);
                        }

                        if (key == "bound")
                        {
                            OVERRIDE(m_bound, getAABBFromYAMLNode(it0.second));
                        }
                    }
                    else
                    {
                        string value = it0.second.as<string>();

                        if (key == "display_name") OVERRIDE(m_displayName, value);
                        if (key == "display") OVERRIDE(m_display, StringToEnum<BlockDisplay>(value));

                        if (key == "interactive") OVERRIDE(m_isInteractive, value == "True");
                        
                        if (key == "model")
                        {
                            OVERRIDE(m_shape, BlockShape::CUSTOM);
                            PUSH_BACK(m_meshes, getMesh(value));
                        }

                        if (key == "collision")
                        {
                            if (it0.second.IsNull())
                            {
                                vector<AABB> emptyCollisionBounds;
                                OVERRIDE(m_collisionBounds, emptyCollisionBounds);
                            }
                        }

                        if (key == "model_tex")
                            PUSH_BACK(m_modelTextures, getTextureId(value));

                        if (key == "rotate_x") OVERRIDE(m_rotationX, StringToEnum<BlockRotation>(value));
                        if (key == "rotate_y") OVERRIDE(m_rotationY, StringToEnum<BlockRotation>(value));
                        if (key == "rotate_z") OVERRIDE(m_rotationZ, StringToEnum<BlockRotation>(value));

                        if (key == "sound") OVERRIDE(m_sound, value);

                        if (key == "all")
                        {
                            u32 id = getTextureId(value);
                            OVERRIDE(m_topTexture, id);
                            OVERRIDE(m_bottomTexture, id);
                            OVERRIDE(m_northTexture, id);
                            OVERRIDE(m_southTexture, id);
                            OVERRIDE(m_eastTexture, id);
                            OVERRIDE(m_westTexture, id);
                        }

                        if (key == "side")
                        {
                            u32 id = getTextureId(value);
                            OVERRIDE(m_northTexture, id);
                            OVERRIDE(m_southTexture, id);
                            OVERRIDE(m_eastTexture, id);
                            OVERRIDE(m_westTexture, id);
                        }

                        if (key == "top")
                        {
                            u32 id = getTextureId(value);
                            OVERRIDE(m_topTexture, id);
                        }

                        if (key == "bottom")
                        {
                            u32 id = getTextureId(value);
                            OVERRIDE(m_bottomTexture, id);
                        }
                    }
                }
            }
        }

        return true;
    }
}
