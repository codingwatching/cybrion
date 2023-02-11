#pragma once

#include "core/linear_palette.hpp"
#include "world/block/block_registry.hpp"

namespace cybrion
{
    static constexpr i32 LOG_2_CHUNK_SIZE = 5;
    static constexpr i32 CHUNK_SIZE = 1 << LOG_2_CHUNK_SIZE;
    static constexpr i32 CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

    struct ChunkData
    {
        LinearPalette<BlockRegistry::BlockStateCount(), CHUNK_VOLUME> blocks;

        ivec3 position;
        
        Entity eastChunk;
        Entity topChunk;
        Entity southChunk;
        Entity westChunk;
        Entity bottomChunk;
        Entity northChunk;

        Block& getBlock(const uvec3& pos) const;
        Block* tryGetBlock(const ivec3& pos);
        void setBlock(const uvec3& pos, const Block& block);

        vec3 getWorldPosition() const;

        static u32 PosToIndex(const uvec3& pos);
    };
}