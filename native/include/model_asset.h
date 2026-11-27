#pragma once

#include <android/asset_manager.h>
#include <GLES2/gl2.h>
#include <cstddef>
#include <vector>

namespace mfa {
struct ModelVertex { float x,y,z,r,g,b,a,u,v; };
struct ModelPrimitive {
  GLuint buffer{};
  GLuint texture{};
  GLsizei vertexCount{};
  bool textured{};
};
struct ModelAsset {
  std::vector<ModelPrimitive> primitives;
  float minX{},minY{},minZ{},maxX{},maxY{},maxZ{};
  float originX{},originY{},originZ{};
  bool valid{};
};
bool loadGlbModel(AAssetManager* manager,const char* assetPath,ModelAsset& out);
void releaseGlbModel(ModelAsset& model);
}
