#define COORD_ASSET_MAP_IMPL
#include "texture_manager.h"

#include <cassert>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <fstream>
#include <string>
#include "graph_types.h"

// libs
#include "backends/imgui_impl_opengl3_loader.h"  //instead of <GL/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Sample::Tex {

namespace {

const IconVariant kUnknownIcon[] = {
   { 16, AssetID::IC_INFORMATION_16_PX },
   { 32, AssetID::IC_INFORMATION_32_PX },
};

const IconVariant kUserIcon[] = {
   { 24, AssetID::VIRTUAL_24_PX_1 },
   { 56, AssetID::VIRTUAL_56_PX_1 },
};

const IconVariant kPartyIcon[] = {
   { 24, AssetID::VIRTUAL_24_PX_2 },
   { 56, AssetID::VIRTUAL_56_PX_2 },
};

const IconVariant kSCSessionIcon[] = {
   { 16, AssetID::IC_SCSESSION_16_PX },
   { 56, AssetID::VIRTUAL_56_PX },
};

const IconVariant kHeatedDSServerIcon[] = {
   { 16, AssetID::IC_HEATEDDS_SERVER_16_PX },
   { 32, AssetID::IC_HEATEDDS_SERVER_32_PX },
};

const IconVariant kStandaloneServerIcon[] = {
   { 16, AssetID::IC_STANDALONE_SERVER_16_PX },
   { 32, AssetID::IC_STANDALONE_SERVER_32_PX },
};

const IconVariant kHydraSampleIcon[] = {
   { 24, AssetID::HYDRASAMPLE_24PX },
   { 104, AssetID::HYDRASAMPLE_104PX },
   { 184, AssetID::HYDRASAMPLE_184PX },
   { 264, AssetID::HYDRASAMPLE_264PX },
   { 344, AssetID::HYDRASAMPLE_344PX },
};

const IconVariant kDSSessionIcon[] = {
   { 24, AssetID::VIRTUAL_24_PX },
   { 56, AssetID::VIRTUAL_56_PX },
};

const IconVariant kMMSessionIcon[] = {
   { 24, AssetID::VIRTUAL_24_PX_3 },
   { 56, AssetID::VIRTUAL_56_PX_3 },
};

const IconVariant kMMFlowSampleIcon[] = {
   { 16, AssetID::IC_PVE_16_PX_1 },
   { 32, AssetID::IC_PVE_32_PX_1 },
};

struct IconLODSet {
   const IconVariant* variants = nullptr;
   size_t count = 0;
};

template<size_t N>
IconLODSet MakeIconLODSet(const IconVariant (&variants)[N])
{
   return IconLODSet{ variants, N };
}

IconLODSet LODSetForKind(NodeKind kind)
{
   switch (kind) {
      case NodeKind::User:             return MakeIconLODSet(kUserIcon);
      case NodeKind::Party:            return MakeIconLODSet(kPartyIcon);
      case NodeKind::SCSession:        return MakeIconLODSet(kSCSessionIcon);
      case NodeKind::HeatedDSServer:   return MakeIconLODSet(kHeatedDSServerIcon);
      case NodeKind::StandaloneServer: return MakeIconLODSet(kStandaloneServerIcon);
      case NodeKind::HydraSample:      return MakeIconLODSet(kHydraSampleIcon);
      case NodeKind::DSSession:        return MakeIconLODSet(kDSSessionIcon);
      case NodeKind::MMSession:        return MakeIconLODSet(kMMSessionIcon);
      case NodeKind::MMFlowSample:     return MakeIconLODSet(kMMFlowSampleIcon);
      case NodeKind::Unknown:
      default:                         return MakeIconLODSet(kUnknownIcon);
   }
}

IconLodInfo ChooseLOD(IconLODSet set, float desiredPx)
{
   if (!set.variants || set.count == 0)
      return IconLodInfo{ AssetID::IC_INFORMATION_32_PX, 32 };

   if (!std::isfinite(desiredPx) || desiredPx <= 0.0f)
      desiredPx = 32.0f;

   for (size_t i = 0; i < set.count; ++i) {
      if ((float)set.variants[i].sizePx >= desiredPx)
         return IconLodInfo{ set.variants[i].asset, set.variants[i].sizePx };
   }

   const IconVariant& largest = set.variants[set.count - 1];
   return IconLodInfo{ largest.asset, largest.sizePx };
}

} // namespace

TextureManager::TextureManager()
{
   ValidateAssetPipeline();
}

void TextureManager::Prefetch(const char *assetListFile)
{
   if (!assetListFile) {
      for (int i = 0; i < ASSET_COUNT; ++i) {
         std::string path = AssetPaths[i];
         PrefetchOne(path);
      }
   } else {
       OutputDebugStringA("TextureManager: Prefetching from file...\n");
       std::ifstream in(assetListFile);
       if (!in.is_open()) {
          char _buf[256];
          std::snprintf(_buf, sizeof(_buf), "TextureManager: failed to open '%s'\n", assetListFile);
          OutputDebugStringA(_buf);
          return;
       }

       std::string path;
       while (std::getline(in, path)) {
          if (path.empty()) continue;
          if (!path.empty() && path.back() == '\r') // CRLF
             path.pop_back();

          PrefetchOne(path);
       }
   }

   char _buf[256];
   std::snprintf(_buf, sizeof(_buf), "TextureManager: Prefetched %zu textures\n", LoadedCount());
   OutputDebugStringA(_buf);
}

void TextureManager::PrefetchOne(const std::string& path)
{
   ImTextureID tex = GetOrLoad(path);
   if (tex == ImTextureID_Invalid) {
      std::fprintf(stderr, "TextureManager: failed to prefetch '%s'\n", path.c_str());
   }
}

ImTextureID TextureManager::Access(AssetID id)
{
   if (id >= ASSET_COUNT) return ImTextureID_Invalid;
   return GetOrLoad(AssetPaths[id]);
}

// Self-test function to validate enum <-> array mapping and basic bounds behavior.
void TextureManager::ValidateAssetPipeline()
{
   // Compile-time size check: AssetPaths must have exactly ASSET_COUNT entries.
   #define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
   static_assert((ARRAY_SIZE(AssetPaths)) == (size_t)ASSET_COUNT,
      "AssetPaths size mismatch: generated array must match enum entries (ASSET_COUNT)");

   // Spot-check: each enum index has a non-empty path string.
   for (size_t i = 0; i < ARRAY_SIZE(AssetPaths); ++i) {
      const char* path = AssetPaths[i];
      assert(path != nullptr && "AssetPaths entry is null");
      assert(path[0] != '\0' && "AssetPaths entry is empty");
   }

   // Bounds test: Access must return the invalid texture id for out-of-range ids.
   // We expect the implementation to check id >= ASSET_COUNT and return ImTextureID_Invalid.
   // Call Access with an out-of-range id and ensure it returns the invalid sentinel.
   ImTextureID invalid_id = ImTextureID_Invalid;
   ImTextureID got = TextureManager::Access(static_cast<AssetID>(ASSET_COUNT));
   assert(got == invalid_id && "TextureManager::Access did not return ImTextureID_Invalid for out-of-range id");

   // Report success
   OutputDebugStringA("Validated pipeline: Asset ID->Path\n");
}


AssetID TextureManager::IconAssetForKind(NodeKind kind, float desiredPx) const
{
   return IconLodInfoForKind(kind, desiredPx).asset;
}

IconLodInfo TextureManager::IconLodInfoForKind(NodeKind kind, float desiredPx) const
{
   return ChooseLOD(LODSetForKind(kind), desiredPx);
}

ImTextureID TextureManager::IconForKind(NodeKind kind, float desiredPx)
{
   return Access(IconAssetForKind(kind, desiredPx));
}

ImTextureID TextureManager::IconForKind(NodeKind kind)
{
   return IconForKind(kind, 32.0f);
}

TextureManager::~TextureManager()
{
   Shutdown();
}

std::string TextureManager::NormalizeKey(const std::string& path)
{
   // Make cache keys stable across / vs \ and case differences (Windows)
   std::string k = path;
   std::replace(k.begin(), k.end(), '/', '\\');
   std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) { return (char)std::tolower(c); });
   return k;
}

bool TextureManager::LoadImageRGBA(const std::string& relativePath, unsigned char** outPixels, int* outW, int* outH)
{
   if (!outPixels || !outW || !outH)
      return false;

   int w = 0, h = 0, comp = 0;

   // Force RGBA. This guarantees 4 bytes per pixel and makes drawing predictable.
   unsigned char* pixels = stbi_load(relativePath.c_str(), &w, &h, &comp, 4);
   if (!pixels || w <= 0 || h <= 0)
      return false;

   *outPixels = pixels;
   *outW = w;
   *outH = h;
   return true;
}

GLTextureHandle TextureManager::UploadRGBA(const unsigned char* pixels, int w, int h)
{
   if (!pixels || w <= 0 || h <= 0)
      return 0;

   GLuint tex = 0;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);

   // Reasonable default sampling for UI icons
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   // Clamp to edge helps for icon atlases / transparent borders
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   // Upload
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexImage2D(GL_TEXTURE_2D,
      0,
      GL_RGBA,
      w,
      h,
      0,
      GL_RGBA,
      GL_UNSIGNED_BYTE,
      pixels);

   glBindTexture(GL_TEXTURE_2D, 0);
   return (GLTextureHandle)tex;
}

ImTextureID TextureManager::GetOrLoad(const std::string& relativePath)
{
   //The path is relative to executable : ".\\Assets\\Icons\\ic_user_16 px.png"
   const std::string key = NormalizeKey(relativePath);

   auto it = m_cache.find(key);
   if (it != m_cache.end())
      return (ImTextureID)(intptr_t)(it->second.glTex);

   unsigned char* pixels = nullptr;
   int w = 0, h = 0;

   if (!LoadImageRGBA(relativePath, &pixels, &w, &h)) {
      const char* reason = stbi_failure_reason();
      std::fprintf(stderr, "TextureManager: failed to load '%s': %s\n", relativePath.c_str(), reason ? reason : "(unknown)");
      return ImTextureID_Invalid;
   }

   GLTextureHandle tex = UploadRGBA(pixels, w, h);
   stbi_image_free(pixels);

   if (tex == 0)
      return ImTextureID_Invalid;

   Entry e;
   e.glTex = tex;
   e.width = w;
   e.height = h;
   m_cache.emplace(key, e);

   return (ImTextureID)(intptr_t)tex;
}

void TextureManager::Unload(const std::string& relativePath)
{
   const std::string key = NormalizeKey(relativePath);
   auto it = m_cache.find(key);
   if (it == m_cache.end())
      return;

   GLuint tex = (GLuint)it->second.glTex;
   if (tex != 0)
      glDeleteTextures(1, &tex);

   m_cache.erase(it);
}

void TextureManager::Shutdown()
{
   for (auto& kv : m_cache) {
      GLuint tex = (GLuint)kv.second.glTex;
      if (tex != 0)
         glDeleteTextures(1, &tex);
   }
   m_cache.clear();
}

} // namespace Sample::Tex
