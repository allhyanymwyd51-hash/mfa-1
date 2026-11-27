#include "model_asset.h"
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb_image.h"

namespace mfa {
namespace {
struct V3 { float x,y,z; };
V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 normalize(V3 a){float n=std::sqrt(std::max(1e-8f,dot(a,a)));return {a.x/n,a.y/n,a.z/n};}


void* loadImage(const cgltf_image* image,int* width,int* height){
  if(!image||!image->buffer_view)return nullptr;
  const uint8_t* bytes=cgltf_buffer_view_data(image->buffer_view);if(!bytes)return nullptr;
  return stbi_load_from_memory(bytes,static_cast<int>(image->buffer_view->size),width,height,nullptr,4);
}
GLuint textureForImage(const cgltf_image* image,std::unordered_map<const cgltf_image*,GLuint>& cache){
  if(!image)return 0;
  auto found=cache.find(image);if(found!=cache.end())return found->second;
  int width=0,height=0;auto* pixels=static_cast<unsigned char*>(loadImage(image,&width,&height));if(!pixels||width<=0||height<=0){if(pixels)stbi_image_free(pixels);cache.emplace(image,0);return 0;}
  GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
  const bool powerOfTwo=(width&(width-1))==0&&(height&(height-1))==0;
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,powerOfTwo?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  const GLint wrap=powerOfTwo?GL_REPEAT:GL_CLAMP_TO_EDGE;glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);if(powerOfTwo)glGenerateMipmap(GL_TEXTURE_2D);stbi_image_free(pixels);
  cache.emplace(image,texture);return texture;
}
const cgltf_accessor* attr(const cgltf_primitive* p,cgltf_attribute_type type){for(cgltf_size i=0;i<p->attributes_count;i++)if(p->attributes[i].type==type&&p->attributes[i].index==0)return p->attributes[i].data;return nullptr;}
uint32_t indexAt(const cgltf_accessor* indices,cgltf_size i){return indices?static_cast<uint32_t>(cgltf_accessor_read_index(indices,i)):static_cast<uint32_t>(i);}
V3 worldPoint(const float* m,V3 p){return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};}
V3 worldNormal(const float* m,V3 n){return normalize({m[0]*n.x+m[4]*n.y+m[8]*n.z,m[1]*n.x+m[5]*n.y+m[9]*n.z,m[2]*n.x+m[6]*n.y+m[10]*n.z});}
}

void releaseGlbModel(ModelAsset& model){for(auto&p:model.primitives){if(p.buffer)glDeleteBuffers(1,&p.buffer);if(p.texture)glDeleteTextures(1,&p.texture);}model={};}

bool loadGlbModel(AAssetManager* manager,const char* assetPath,ModelAsset& out){
  releaseGlbModel(out);if(!manager||!assetPath)return false;
  AAsset* asset=AAssetManager_open(manager,assetPath,AASSET_MODE_BUFFER);if(!asset)return false;
  off_t length=AAsset_getLength(asset);if(length<=0){AAsset_close(asset);return false;}
  std::vector<uint8_t> bytes(static_cast<size_t>(length));int bytesRead=AAsset_read(asset,bytes.data(),bytes.size());AAsset_close(asset);if(bytesRead!=length)return false;
  cgltf_options options{};options.memory.alloc_func=nullptr;options.memory.free_func=nullptr;
  cgltf_data* data=nullptr;cgltf_result result=cgltf_parse(&options,bytes.data(),bytes.size(),&data);if(result!=cgltf_result_success||!data)return false;
  result=cgltf_load_buffers(&options,data,assetPath);if(result!=cgltf_result_success){cgltf_free(data);return false;}
  out.minX=out.minY=out.minZ=std::numeric_limits<float>::max();out.maxX=out.maxY=out.maxZ=std::numeric_limits<float>::lowest();
  std::unordered_map<const cgltf_image*,GLuint> imageCache;
  const V3 sun=normalize({-.35f,.85f,.40f});
  for(cgltf_size ni=0;ni<data->nodes_count;ni++){
    const cgltf_node* node=&data->nodes[ni];if(!node->mesh)continue;float matrix[16];cgltf_node_transform_world(node,matrix);
    for(cgltf_size pi=0;pi<node->mesh->primitives_count;pi++){
      const cgltf_primitive* primitive=&node->mesh->primitives[pi];if(primitive->type!=cgltf_primitive_type_triangles)continue;
      const cgltf_accessor* positions=attr(primitive,cgltf_attribute_type_position);if(!positions||positions->count<3)continue;
      const cgltf_accessor* normals=attr(primitive,cgltf_attribute_type_normal);const cgltf_accessor* texcoords=attr(primitive,cgltf_attribute_type_texcoord);const cgltf_accessor* colors=attr(primitive,cgltf_attribute_type_color);
      const cgltf_size indexCount=primitive->indices?primitive->indices->count:positions->count;if(indexCount<3)continue;
      ModelPrimitive part;cgltf_material* material=primitive->material;std::array<float,4> base{1,1,1,1};
      if(material&&material->has_pbr_metallic_roughness)for(int c=0;c<4;c++)base[c]=material->pbr_metallic_roughness.base_color_factor[c];
      if(material&&material->has_pbr_metallic_roughness&&material->pbr_metallic_roughness.base_color_texture.texture){cgltf_image* image=material->pbr_metallic_roughness.base_color_texture.texture->image;part.texture=textureForImage(image,imageCache);part.textured=part.texture!=0;}
      std::vector<ModelVertex> vertices;vertices.reserve(static_cast<size_t>(indexCount));
      for(cgltf_size t=0;t+2<indexCount;t+=3){
        uint32_t ids[3]={indexAt(primitive->indices,t),indexAt(primitive->indices,t+1),indexAt(primitive->indices,t+2)};V3 p[3],n[3];float uv[3][2]{};float col[3][4];
        for(int k=0;k<3;k++){
          float raw[4]{};if(!cgltf_accessor_read_float(positions,ids[k],raw,3))continue;p[k]=worldPoint(matrix,{raw[0],raw[1],raw[2]});
          if(normals&&cgltf_accessor_read_float(normals,ids[k],raw,3))n[k]=worldNormal(matrix,{raw[0],raw[1],raw[2]});else n[k]={0,1,0};
          if(texcoords)cgltf_accessor_read_float(texcoords,ids[k],uv[k],2);
          for(int c=0;c<4;c++)col[k][c]=base[c];
          if(colors&&cgltf_accessor_read_float(colors,ids[k],raw,colors->type==cgltf_type_vec3?3:4)){for(int c=0;c<3;c++)col[k][c]*=raw[c];if(colors->type==cgltf_type_vec4)col[k][3]*=raw[3];}
        }
        if(!normals){V3 face=normalize(cross(sub(p[1],p[0]),sub(p[2],p[0])));n[0]=n[1]=n[2]=face;}
        for(int k=0;k<3;k++){
          float daylight=.58f+.42f*std::max(0.f,dot(n[k],sun));
          ModelVertex v{p[k].x,p[k].y,p[k].z,col[k][0]*daylight,col[k][1]*daylight,col[k][2]*daylight,col[k][3],uv[k][0],uv[k][1]};vertices.push_back(v);
          out.minX=std::min(out.minX,v.x);out.minY=std::min(out.minY,v.y);out.minZ=std::min(out.minZ,v.z);out.maxX=std::max(out.maxX,v.x);out.maxY=std::max(out.maxY,v.y);out.maxZ=std::max(out.maxZ,v.z);
        }
      }
      if(vertices.empty())continue;
      part.vertexCount=static_cast<GLsizei>(vertices.size());glGenBuffers(1,&part.buffer);glBindBuffer(GL_ARRAY_BUFFER,part.buffer);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(ModelVertex)),vertices.data(),GL_STATIC_DRAW);out.primitives.push_back(part);
    }
  }
  cgltf_free(data);
  if(out.primitives.empty()||out.maxX<=out.minX||out.maxY<=out.minY||out.maxZ<=out.minZ){releaseGlbModel(out);return false;}
  float cx=(out.minX+out.maxX)*.5f,cz=(out.minZ+out.maxZ)*.5f,ground=out.minY;
  out.originX=cx;out.originY=ground;out.originZ=cz;
  out.minX-=cx;out.maxX-=cx;out.minY=0;out.maxY-=ground;out.minZ-=cz;out.maxZ-=cz;out.valid=true;return true;
}
}
