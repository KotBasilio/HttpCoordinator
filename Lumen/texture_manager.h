#pragma once
#include <imgui.h>

#include <string>
#include <unordered_map>
#include "AssetsEnum.h"

enum class NodeKind;

namespace Sample::Tex {

// OpenGL texture id type (GLuint). We avoid including GL headers here.
using GLTextureHandle = unsigned int;

// Small, shared texture cache for ImGui/OpenGL.
// Loads images from disk using stb_image and uploads to OpenGL as GL_TEXTURE_2D.
class TextureManager {
public:
   TextureManager();
   void Prefetch(const char* assetListFile = nullptr);
   ~TextureManager();

   TextureManager(const TextureManager&) = delete;
   TextureManager& operator=(const TextureManager&) = delete;

   // Get prefetched texture. Returns ImTextureID_Invalid on failure.
   ImTextureID Access(AssetID id);
   ImTextureID IconForKind(NodeKind kind);

   // Remove a single texture (optional convenience).
   void Unload(const std::string& relativePath);

   // Small introspection helpers
   size_t LoadedCount() const { return m_cache.size(); }

private:
   void ValidateAssetPipeline();
   void Shutdown();

   struct Entry {
      GLTextureHandle glTex = 0;
      int width = 0;
      int height = 0;
   };

   std::unordered_map<std::string, Entry> m_cache;

   // Get or load a texture. Returns ImTextureID_Invalid on failure.
   ImTextureID GetOrLoad(const std::string& relativePath);
   void PrefetchOne(const std::string& path);

   static bool LoadImageRGBA(const std::string& relativePath, unsigned char** outPixels, int* outW, int* outH);
   static GLTextureHandle UploadRGBA(const unsigned char* pixels, int w, int h);

   static std::string NormalizeKey(const std::string& path);
};

} // namespace Sample::Tex
