#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <limits>
#include <initializer_list>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

namespace {
constexpr char kLog[] = "MFA_NATIVE";
constexpr float kPi = 3.1415926535f;
constexpr float kCity = 768.0f;
constexpr int kBlocks = 12;
constexpr int kDetailPoints = 6000;
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLog, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLog, __VA_ARGS__)

struct V3 { float x{}, y{}, z{}; };
V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 mul(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){float l=std::sqrt(std::max(0.000001f,dot(a,a)));return mul(a,1.f/l);}
struct M4 { float v[16]{}; };
M4 identity(){M4 m{};m.v[0]=m.v[5]=m.v[10]=m.v[15]=1;return m;}
M4 multiply(const M4&a,const M4&b){M4 r{};for(int c=0;c<4;c++)for(int row=0;row<4;row++)for(int k=0;k<4;k++)r.v[c*4+row]+=a.v[k*4+row]*b.v[c*4+k];return r;}
M4 perspective(float fov,float aspect,float nearZ,float farZ){M4 m{};float f=1.f/std::tan(fov*.5f);m.v[0]=f/aspect;m.v[5]=f;m.v[10]=(farZ+nearZ)/(nearZ-farZ);m.v[11]=-1;m.v[14]=(2*farZ*nearZ)/(nearZ-farZ);return m;}
M4 lookAt(V3 eye,V3 center,V3 up){V3 f=norm(sub(center,eye)),s=norm(cross(f,up)),u=cross(s,f);M4 m=identity();m.v[0]=s.x;m.v[4]=s.y;m.v[8]=s.z;m.v[1]=u.x;m.v[5]=u.y;m.v[9]=u.z;m.v[2]=-f.x;m.v[6]=-f.y;m.v[10]=-f.z;m.v[12]=-dot(s,eye);m.v[13]=-dot(u,eye);m.v[14]=dot(f,eye);return m;}

struct Vertex { float x{},y{},z{},r{},g{},b{},a{1.f},u{},v{}; };
struct Color { float r{},g{},b{},a{1.f}; };
struct Glyph { float u0{},v0{},u1{},v1{},advance{}; int width{},height{},xoff{},yoff{}; };
struct ArabicForm { uint32_t base,isolated,finalForm,initial,medial; bool joinsNext,joinsPrevious; };
void tri(std::vector<Vertex>&v,V3 a,V3 b,V3 c,Color q){v.push_back({a.x,a.y,a.z,q.r,q.g,q.b,q.a});v.push_back({b.x,b.y,b.z,q.r,q.g,q.b,q.a});v.push_back({c.x,c.y,c.z,q.r,q.g,q.b,q.a});}
void quad(std::vector<Vertex>&v,V3 a,V3 b,V3 c,V3 d,Color q){tri(v,a,b,c,q);tri(v,a,c,d,q);}
V3 rotateY(V3 p,float a){float c=std::cos(a),s=std::sin(a);return {p.x*c+p.z*s,p.y,-p.x*s+p.z*c};}
void box(std::vector<Vertex>&v,V3 center,V3 size,Color color,float yaw=0){
  float x=size.x*.5f,y=size.y*.5f,z=size.z*.5f;
  V3 p[8]={{-x,-y,-z},{x,-y,-z},{x,y,-z},{-x,y,-z},{-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}};
  for(auto &q:p)q=add(center,rotateY(q,yaw));
  const int faces[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
  const float shade[6]={.78f,.92f,.7f,1.f,1.f,.64f};
  for(int f=0;f<6;f++){Color c{color.r*shade[f],color.g*shade[f],color.b*shade[f]};quad(v,p[faces[f][0]],p[faces[f][1]],p[faces[f][2]],p[faces[f][3]],c);}
}
void cylinderBetween(std::vector<Vertex>&v,V3 bottom,V3 top,float rb,float rt,Color color,int sides=8){V3 axis=norm(sub(top,bottom)),ref=std::abs(axis.y)<.9f?V3{0,1,0}:V3{1,0,0},u=norm(cross(axis,ref)),w=cross(axis,u);for(int i=0;i<sides;i++){float a=2*kPi*i/sides,b=2*kPi*(i+1)/sides;V3 b0=add(bottom,mul(add(mul(u,std::cos(a)),mul(w,std::sin(a))),rb)),b1=add(bottom,mul(add(mul(u,std::cos(b)),mul(w,std::sin(b))),rb)),t0=add(top,mul(add(mul(u,std::cos(a)),mul(w,std::sin(a))),rt)),t1=add(top,mul(add(mul(u,std::cos(b)),mul(w,std::sin(b))),rt));float shade=.82f+.18f*std::sin(a);Color c{color.r*shade,color.g*shade,color.b*shade,color.a};quad(v,b0,b1,t1,t0,c);tri(v,bottom,b1,b0,color);tri(v,top,t0,t1,color);}}
void sphere(std::vector<Vertex>&v,V3 center,V3 radius,Color color,float yaw=0,int rings=8,int slices=12){for(int r=0;r<rings;r++){float p0=-kPi*.5f+kPi*r/rings,p1=-kPi*.5f+kPi*(r+1)/rings;for(int s=0;s<slices;s++){float t0=2*kPi*s/slices,t1=2*kPi*(s+1)/slices;auto point=[&](float p,float t){V3 q{radius.x*std::cos(p)*std::cos(t),radius.y*std::sin(p),radius.z*std::cos(p)*std::sin(t)};return add(center,rotateY(q,yaw));};V3 a=point(p0,t0),b=point(p0,t1),c=point(p1,t1),d=point(p1,t0);quad(v,a,b,c,d,{color.r,color.g,color.b,color.a});}}}
void torus(std::vector<Vertex>&v,V3 center,float major,float tube,Color color,float yaw=0){const int around=16,section=6;for(int i=0;i<around;i++)for(int j=0;j<section;j++){auto p=[&](float a,float b){V3 q{(major+tube*std::cos(b))*std::cos(a),tube*std::sin(b),(major+tube*std::cos(b))*std::sin(a)};return add(center,rotateY(q,yaw));};float a0=2*kPi*i/around,a1=2*kPi*(i+1)/around,b0=2*kPi*j/section,b1=2*kPi*(j+1)/section;quad(v,p(a0,b0),p(a1,b0),p(a1,b1),p(a0,b1),color);}}
void rect2d(std::vector<Vertex>&v,float x,float y,float w,float h,Color c){quad(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);}
void circle2d(std::vector<Vertex>&v,float x,float y,float r,Color c){for(int i=0;i<28;i++){float a=i*2*kPi/28,b=(i+1)*2*kPi/28;tri(v,{x,y,0},{x+std::cos(a)*r,y+std::sin(a)*r,0},{x+std::cos(b)*r,y+std::sin(b)*r,0},c);}}

struct Building {float x,z,w,d,h;Color color;bool shop,dealer;};
struct Trash {float x,z;bool alive{true};};
struct NPC {float x,z,phase,speed;Color shirt;int identity{-1};};
struct Car {float x,z,yaw,speed;Color color;bool traffic,owned;};

class NativeGame {
 public:
  android_app* app{};
  EGLDisplay display{EGL_NO_DISPLAY}; EGLSurface surface{EGL_NO_SURFACE}; EGLContext context{EGL_NO_CONTEXT};
  GLuint program{},cityBuffer{},detailBuffer{},frameBuffer{},fontTexture{};
  GLint posLoc{-1},colorLoc{-1},uvLoc{-1},mvpLoc{-1},pointLoc{-1},texturedLoc{-1},fontLoc{-1};
  int width{1},height{1};bool ready{},focused{true},room{};int insideBuilding{-1},drivenCar{-1},dealerBuilding{-1};
  std::vector<Vertex> cityVerts,frameVerts,pointVerts;size_t worldFrameCount{};
  std::vector<Building> buildings;std::vector<Trash> trash;std::vector<NPC> npcs;std::vector<Car> cars;
  std::vector<unsigned char> fontBytes,fontAtlas;stbtt_fontinfo fontInfo{};std::unordered_map<uint32_t,Glyph> glyphs;float fontScale{};bool fontReady{};
  V3 player{0,0,-4};float playerYaw{},speed{},cameraYaw{},cameraPitch{.23f},walkPhase{},health{100.f},hunger{100.f},money{},playerY{},verticalVelocity{};
  float joyX{},joyY{};bool joyActive{},runHeld{},jumpRequested{},cameraHeld{},pendingEnter{};int joyId{-1},cameraId{-1};float lastTouchX{},lastTouchY{};
  bool keyForward{},keyBack{},keyLeft{},keyRight{},keyRun{};bool keyJump{};
  int questStage{},trashQuest{},dialogSpeaker{},dialogAction{},dialogPage{};bool dialogueOpen{};std::vector<std::string>dialogPages;std::string objective;V3 missionTarget{};bool missionTargetVisible{};int missionProgress{},missionTotal{};bool packageCarried{},passengerOnboard{},ownsCar{};float toastTime{};std::string toastText;
  float packageX{-12.f},packageZ{10.f},deliveryX{12.f},deliveryZ{74.f},taxiPickupX{-12.f},taxiPickupZ{138.f},taxiDropX{12.f},taxiDropZ{202.f};
  uint32_t rng{0x4d464131u};float lastTime{};float doorAnim{};V3 exteriorReturn{};

  uint32_t randomU(){rng=rng*1664525u+1013904223u;return rng;}
  float random(){return (randomU()>>8)*(1.f/16777216.f);}
  Color rndColor(Color a,Color b){float t=random();return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}

  GLuint shader(GLenum type,const char*src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024]{};glGetShaderInfoLog(s,sizeof(log),nullptr,log);LOGE("shader: %s",log);glDeleteShader(s);return 0;}return s;}
  bool initGL(){
    const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_BLUE_SIZE,8,EGL_GREEN_SIZE,8,EGL_RED_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE};
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display==EGL_NO_DISPLAY||!eglInitialize(display,nullptr,nullptr))return false;
    EGLConfig cfg;EGLint count;eglChooseConfig(display,attrs,&cfg,1,&count);if(count<1)return false;
    EGLint format;eglGetConfigAttrib(display,cfg,EGL_NATIVE_VISUAL_ID,&format);ANativeWindow_setBuffersGeometry(app->window,0,0,format);
    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);surface=eglCreateWindowSurface(display,cfg,app->window,nullptr);
    if(context==EGL_NO_CONTEXT||surface==EGL_NO_SURFACE||!eglMakeCurrent(display,surface,surface,context))return false;
    eglSwapInterval(display,1);eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);
    const char*vs="attribute vec3 aPos; attribute vec4 aColor; attribute vec2 aUV; uniform mat4 uMvp; uniform float uPoint; varying vec4 vColor; varying vec2 vUV; void main(){gl_Position=uMvp*vec4(aPos,1.0);gl_PointSize=uPoint;vColor=aColor;vUV=aUV;}";
    const char*fs="precision mediump float; varying vec4 vColor; varying vec2 vUV; uniform sampler2D uFont; uniform float uTextured; void main(){if(uTextured>0.5){float a=texture2D(uFont,vUV).a;gl_FragColor=vec4(vColor.rgb,vColor.a*a);}else{gl_FragColor=vColor;}}";
    GLuint a=shader(GL_VERTEX_SHADER,vs),b=shader(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);glDeleteShader(a);glDeleteShader(b);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)return false;
    posLoc=glGetAttribLocation(program,"aPos");colorLoc=glGetAttribLocation(program,"aColor");uvLoc=glGetAttribLocation(program,"aUV");mvpLoc=glGetUniformLocation(program,"uMvp");pointLoc=glGetUniformLocation(program,"uPoint");texturedLoc=glGetUniformLocation(program,"uTextured");fontLoc=glGetUniformLocation(program,"uFont");
    glGenBuffers(1,&cityBuffer);glGenBuffers(1,&detailBuffer);glGenBuffers(1,&frameBuffer);
    if(cityVerts.empty())buildWorld();
    upload(cityBuffer,cityVerts,GL_STATIC_DRAW);
    upload(detailBuffer,pointVerts,GL_STATIC_DRAW);
    initFont();
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    ready=true;LOGI("Native MFA C++ renderer ready; points=%d",kDetailPoints);return true;
  }
  void upload(GLuint buffer,const std::vector<Vertex>&v,GLenum usage){glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(v.size()*sizeof(Vertex)),v.empty()?nullptr:v.data(),usage);}
  void initFont(){
    AAsset*asset=AAssetManager_open(app->activity->assetManager,"DejaVuSans.ttf",AASSET_MODE_BUFFER);if(!asset){LOGE("Arabic font asset missing");return;}
    const off_t length=AAsset_getLength(asset);fontBytes.resize(static_cast<size_t>(length));AAsset_read(asset,fontBytes.data(),length);AAsset_close(asset);
    if(!stbtt_InitFont(&fontInfo,fontBytes.data(),stbtt_GetFontOffsetForIndex(fontBytes.data(),0))){LOGE("Arabic font could not be initialized");return;}
    constexpr int atlasSize=1024,cell=64,columns=16;std::vector<uint32_t>codepoints;for(uint32_t c=32;c<=126;c++)codepoints.push_back(c);for(uint32_t c=0xFE70;c<=0xFEFF;c++)codepoints.push_back(c);for(uint32_t c=0x0660;c<=0x0669;c++)codepoints.push_back(c);codepoints.push_back(0x060C);codepoints.push_back(0x061B);codepoints.push_back(0x061F);codepoints.push_back(0x066A);
    fontAtlas.assign(atlasSize*atlasSize,0);fontAtlas[0]=255;fontScale=stbtt_ScaleForPixelHeight(&fontInfo,42.f);int slot=0;
    for(uint32_t cp:codepoints){int w=0,h=0,xoff=0,yoff=0;unsigned char*bitmap=stbtt_GetCodepointBitmap(&fontInfo,fontScale,fontScale,static_cast<int>(cp),&w,&h,&xoff,&yoff);int advance=0,lsb=0;stbtt_GetCodepointHMetrics(&fontInfo,static_cast<int>(cp),&advance,&lsb);Glyph g;g.advance=advance*fontScale;g.width=w;g.height=h;g.xoff=xoff;g.yoff=yoff;
      int ox=(slot%columns)*cell+4,oy=(slot/columns)*cell+4;if(bitmap&&w>0&&h>0&&ox+w<atlasSize&&oy+h<atlasSize){for(int y=0;y<h;y++)std::memcpy(fontAtlas.data()+(oy+y)*atlasSize+ox,bitmap+y*w,w);g.u0=static_cast<float>(ox)/atlasSize;g.u1=static_cast<float>(ox+w)/atlasSize;g.v0=static_cast<float>(oy)/atlasSize;g.v1=static_cast<float>(oy+h)/atlasSize;}
      if(bitmap)stbtt_FreeBitmap(bitmap,nullptr);
      glyphs[cp]=g;slot++;}
    glGenTextures(1,&fontTexture);glBindTexture(GL_TEXTURE_2D,fontTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,atlasSize,atlasSize,0,GL_ALPHA,GL_UNSIGNED_BYTE,fontAtlas.data());fontReady=true;LOGI("Arabic font atlas ready, glyphs=%zu",glyphs.size());
  }
  static const ArabicForm* arabicForm(uint32_t cp){
    static const ArabicForm forms[]={
      {0x0621,0xFE80,0,0,0,false,false},{0x0622,0xFE81,0xFE82,0,0,false,true},{0x0623,0xFE83,0xFE84,0,0,false,true},{0x0624,0xFE85,0xFE86,0,0,false,true},{0x0625,0xFE87,0xFE88,0,0,false,true},{0x0626,0xFE89,0xFE8A,0xFE8B,0xFE8C,true,true},{0x0627,0xFE8D,0xFE8E,0,0,false,true},
      {0x0628,0xFE8F,0xFE90,0xFE91,0xFE92,true,true},{0x0629,0xFE93,0xFE94,0,0,false,true},{0x062A,0xFE95,0xFE96,0xFE97,0xFE98,true,true},{0x062B,0xFE99,0xFE9A,0xFE9B,0xFE9C,true,true},{0x062C,0xFE9D,0xFE9E,0xFE9F,0xFEA0,true,true},{0x062D,0xFEA1,0xFEA2,0xFEA3,0xFEA4,true,true},{0x062E,0xFEA5,0xFEA6,0xFEA7,0xFEA8,true,true},
      {0x062F,0xFEA9,0xFEAA,0,0,false,true},{0x0630,0xFEAB,0xFEAC,0,0,false,true},{0x0631,0xFEAD,0xFEAE,0,0,false,true},{0x0632,0xFEAF,0xFEB0,0,0,false,true},{0x0633,0xFEB1,0xFEB2,0xFEB3,0xFEB4,true,true},{0x0634,0xFEB5,0xFEB6,0xFEB7,0xFEB8,true,true},{0x0635,0xFEB9,0xFEBA,0xFEBB,0xFEBC,true,true},{0x0636,0xFEBD,0xFEBE,0xFEBF,0xFEC0,true,true},
      {0x0637,0xFEC1,0xFEC2,0xFEC3,0xFEC4,true,true},{0x0638,0xFEC5,0xFEC6,0xFEC7,0xFEC8,true,true},{0x0639,0xFEC9,0xFECA,0xFECB,0xFECC,true,true},{0x063A,0xFECD,0xFECE,0xFECF,0xFED0,true,true},{0x0640,0x0640,0x0640,0x0640,0x0640,true,true},{0x0641,0xFED1,0xFED2,0xFED3,0xFED4,true,true},{0x0642,0xFED5,0xFED6,0xFED7,0xFED8,true,true},{0x0643,0xFED9,0xFEDA,0xFEDB,0xFEDC,true,true},
      {0x0644,0xFEDD,0xFEDE,0xFEDF,0xFEE0,true,true},{0x0645,0xFEE1,0xFEE2,0xFEE3,0xFEE4,true,true},{0x0646,0xFEE5,0xFEE6,0xFEE7,0xFEE8,true,true},{0x0647,0xFEE9,0xFEEA,0xFEEB,0xFEEC,true,true},{0x0648,0xFEED,0xFEEE,0,0,false,true},{0x0649,0xFEEF,0xFEF0,0,0,false,true},{0x064A,0xFEF1,0xFEF2,0xFEF3,0xFEF4,true,true}
    };for(const auto&f:forms)if(f.base==cp)return &f;return nullptr;
  }
  std::vector<uint32_t> utf8(const std::string&s){std::vector<uint32_t>out;for(size_t i=0;i<s.size();){uint8_t c=static_cast<uint8_t>(s[i]);if(c<0x80){out.push_back(c);i++;}else if((c&0xE0)==0xC0&&i+1<s.size()){out.push_back(((c&0x1F)<<6)|(static_cast<uint8_t>(s[i+1])&0x3F));i+=2;}else if((c&0xF0)==0xE0&&i+2<s.size()){out.push_back(((c&0x0F)<<12)|((static_cast<uint8_t>(s[i+1])&0x3F)<<6)|(static_cast<uint8_t>(s[i+2])&0x3F));i+=3;}else if((c&0xF8)==0xF0&&i+3<s.size()){out.push_back(((c&7)<<18)|((static_cast<uint8_t>(s[i+1])&0x3F)<<12)|((static_cast<uint8_t>(s[i+2])&0x3F)<<6)|(static_cast<uint8_t>(s[i+3])&0x3F));i+=4;}else{i++;}}return out;}
  std::vector<uint32_t> shapeArabic(const std::vector<uint32_t>&src){std::vector<uint32_t>o=src;for(size_t i=0;i<src.size();i++){const auto*cur=arabicForm(src[i]);if(!cur)continue;bool prev=false,next=false;if(i>0){auto*p=arabicForm(src[i-1]);prev=p&&p->joinsNext&&cur->joinsPrevious;}if(i+1<src.size()){auto*n=arabicForm(src[i+1]);next=cur->joinsNext&&n&&n->joinsPrevious;}if(prev&&next&&cur->medial)o[i]=cur->medial;else if(prev&&cur->finalForm)o[i]=cur->finalForm;else if(next&&cur->initial)o[i]=cur->initial;else o[i]=cur->isolated;}
    // Lam-alif ligatures make common Arabic words cleaner.
    for(size_t i=0;i+1<src.size();i++)if(src[i]==0x0644&&(src[i+1]==0x0627||src[i+1]==0x0623||src[i+1]==0x0625||src[i+1]==0x0622)){uint32_t lig=src[i+1]==0x0623?0xFEF7:src[i+1]==0x0625?0xFEF9:src[i+1]==0x0622?0xFEF5:0xFEF5;bool join=i>0&&arabicForm(src[i-1])&&arabicForm(src[i-1])->joinsNext;o[i]=lig+(join?1:0);o[i+1]=0;}
    return o;}
  float measureArabic(const std::string&s){auto cps=shapeArabic(utf8(s));float w=0;for(uint32_t cp:cps)if(cp){auto it=glyphs.find(cp);if(it!=glyphs.end())w+=it->second.advance;else w+=15;}return w;}
  void pushGlyph(std::vector<Vertex>&v,float x,float y,const Glyph&g,float scale,Color c){if(g.width<=0||g.height<=0)return;float x0=2*x/width-1,x1=2*(x+g.width*scale)/width-1,y0=1-2*y/height,y1=1-2*(y+g.height*scale)/height;Vertex a{x0,y0,0,c.r,c.g,c.b,c.a,g.u0,g.v0},b{x1,y0,0,c.r,c.g,c.b,c.a,g.u1,g.v0},cc{x1,y1,0,c.r,c.g,c.b,c.a,g.u1,g.v1},d{x0,y1,0,c.r,c.g,c.b,c.a,g.u0,g.v1};v.insert(v.end(),{a,b,cc,a,cc,d});}
  void drawArabicLine(std::vector<Vertex>&v,const std::string&s,float rightPx,float topPx,float pixelHeight,Color color){if(!fontReady||s.empty())return;auto cps=shapeArabic(utf8(s));float scale=pixelHeight/42.f,total=0;for(uint32_t cp:cps)if(cp){auto it=glyphs.find(cp);total+=it!=glyphs.end()?it->second.advance*scale:pixelHeight*.35f;}float cursor=rightPx-total,baseline=topPx+pixelHeight*.84f;for(int i=static_cast<int>(cps.size())-1;i>=0;i--){uint32_t cp=cps[i];if(!cp)continue;auto it=glyphs.find(cp);if(it==glyphs.end()){cursor+=pixelHeight*.35f;continue;}const Glyph&g=it->second;pushGlyph(v,cursor+g.xoff*scale,baseline+g.yoff*scale,g,scale,color);cursor+=g.advance*scale;}}
  void drawLatinLine(std::vector<Vertex>&v,const std::string&s,float leftPx,float topPx,float pixelHeight,Color color){if(!fontReady||s.empty())return;auto cps=utf8(s);float scale=pixelHeight/42.f,cursor=leftPx,baseline=topPx+pixelHeight*.84f;for(uint32_t cp:cps){auto it=glyphs.find(cp);if(it==glyphs.end()){cursor+=pixelHeight*.35f;continue;}const Glyph&g=it->second;pushGlyph(v,cursor+g.xoff*scale,baseline+g.yoff*scale,g,scale,color);cursor+=g.advance*scale;}}
  void drawArabicWrapped(std::vector<Vertex>&v,const std::string&s,float rightPx,float topPx,float maxWidth,float pixelHeight,Color color,int maxLines=3){std::vector<std::string>lines;std::string current,word;auto tooWide=[&](const std::string&t){return measureArabic(t)*(pixelHeight/42.f)>maxWidth;};for(char c:s){if(c==' '||c=='\n'){if(!word.empty()){std::string trial=current.empty()?word:current+" "+word;if(tooWide(trial)&&!current.empty()){lines.push_back(current);current=word;}else current=trial;word.clear();}if(c=='\n'&&!current.empty()){lines.push_back(current);current.clear();}}else word.push_back(c);}if(!word.empty()){std::string trial=current.empty()?word:current+" "+word;if(tooWide(trial)&&!current.empty()){lines.push_back(current);current=word;}else current=trial;}if(!current.empty())lines.push_back(current);if(static_cast<int>(lines.size())>maxLines)lines.resize(maxLines);for(size_t i=0;i<lines.size();i++)drawArabicLine(v,lines[i],rightPx,topPx+i*pixelHeight*1.35f,pixelHeight,color);}
  void loseGL(){ready=false;if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}display=EGL_NO_DISPLAY;context=EGL_NO_CONTEXT;surface=EGL_NO_SURFACE;}
  void setObjective(std::string text,float x=0,float z=0,bool marker=false){objective=std::move(text);missionTarget={x,0,z};missionTargetVisible=marker;}
  void toast(std::string text){toastText=std::move(text);toastTime=3.f;}
  void showDialog(int speaker,std::vector<std::string>pages,int after){dialogSpeaker=speaker;dialogPages=std::move(pages);dialogAction=after;dialogPage=0;dialogueOpen=true;}
  void beginStory(){showDialog(0,{"يا هلا يا سالم، نورت حي النخيل. أنا أبو فهد وجارك الجديد.","ابدأ مشوارك من الصفر: اجمع ثلاث قوارير من الشارع، وبعدها أعطيك ١٥٠ ريال."},1);}
  void advanceDialog(){if(!dialogueOpen)return;if(dialogPage+1<static_cast<int>(dialogPages.size())){dialogPage++;return;}int action=dialogAction;dialogueOpen=false;dialogPages.clear();dialogAction=0;finishDialogAction(action);}
  void finishDialogAction(int action){
    if(action==1){questStage=1;trashQuest=0;missionProgress=0;missionTotal=3;setObjective("اجمع ثلاث قوارير من الشارع",0,0,false);toast("ابدأ بالمخلفات القريبة من الرصيف");}
    else if(action==2){questStage=2;missionProgress=missionTotal=0;packageCarried=false;setObjective("خذ صندوق المخبز إلى المقهى",packageX,packageZ,true);toast("توجه إلى صندوق الطلب عند المخبز");}
    else if(action==3){questStage=4;passengerOnboard=false;setObjective("اذهب إلى موقف الأجرة",taxiPickupX,taxiPickupZ,true);toast("الراكب ينتظرك عند الموقف");}
    else if(action==4){questStage=6;if(dealerBuilding>=0){const auto&b=buildings[dealerBuilding];setObjective("اذهب إلى معرض السيارات واشتر أول سيارة",b.x,b.z+b.d*.5f+4.f,true);}else setObjective("اذهب إلى معرض السيارات",-32,50,true);toast("جمعت ٥٥٠ ريالاً؛ سعر السيارة ٥٠٠ ريال");}
    else if(action==5)purchaseCar();
    else if(action==6){setObjective("أكمل مهام المدينة واجمع المال",0,0,false);}
  }
  void purchaseCar(){if(money<500.f){showDialog(4,{"السيارة سعرها ٥٠٠ ريال، ورصيدك الحالي غير كاف.","ارجع للعمل واجمع طلبات أكثر، ثم عد للمعرض."},6);return;}money-=500.f;ownsCar=true;questStage=7;missionProgress=0;missionTotal=0;missionTargetVisible=false;setObjective("سيارتك الجديدة جاهزة؛ اقترب منها واضغط تفاعل",0,0,false);if(dealerBuilding>=0){const auto&b=buildings[dealerBuilding];cars.push_back({b.x+5.f,b.z+b.d*.5f+5.f,0,0,{.78f,.08f,.055f},false,true});}showDialog(4,{"مبروك يا سالم! صارت السيارة ملكك.","دفعت ٥٠٠ ريال وباقي معك ٥٠ ريال. مفتاحك معك، استمتع بالقيادة!"},0);}
  const char* speakerName()const{switch(dialogSpeaker){case 0:return "أبو فهد";case 1:return "خباز الحي";case 2:return "صاحب المقهى";case 3:return "الراكب";case 4:return "موظف المعرض";default:return "أحد أهل الحي";}}

  void buildWorld(){

    const float half=kCity*.5f,blockSize=kCity/kBlocks;
    // Terrain, asphalt lanes, raised sidewalks, lane dashes.
    box(cityVerts,{0,-.16f,0},{kCity+160,.25f,kCity+160},{.70f,.55f,.36f});
    for(int i=0;i<=kBlocks;i++){
      float p=-half+i*blockSize;
      box(cityVerts,{0,-.015f,p},{kCity,.08f,16},{.16f,.18f,.19f});
      box(cityVerts,{p,-.014f,0},{16,.08f,kCity},{.16f,.18f,.19f});
      for(float d:{-9.5f,9.5f}){
        box(cityVerts,{0,.025f,p+d},{kCity,.08f,3},{.54f,.52f,.46f});
        box(cityVerts,{p+d,.025f,0},{3,.08f,kCity},{.54f,.52f,.46f});
      }
      for(float q=-half+10;q<half;q+=28){box(cityVerts,{q,.035f,p},{8,.015f,.18f},{.88f,.75f,.32f});box(cityVerts,{p,.035f,q},{.18f,.015f,8},{.88f,.75f,.32f});}
    }
    // 144 buildings. Front doors, shop awnings, window bands and rooftop tanks.
    for(int ix=0;ix<kBlocks;ix++)for(int iz=0;iz<kBlocks;iz++){
      float x=-half+ix*blockSize+blockSize*.5f,z=-half+iz*blockSize+blockSize*.5f;
      bool dealer=(ix==5&&iz==6);bool shop=((ix+iz)%3==0)||dealer;float w=dealer?32.f:25+random()*8,d=dealer?30.f:25+random()*8,h=dealer?10.f:(shop?7.5f:12+random()*36);
      Color c=dealer?Color{.22f,.29f,.34f}:(shop?rndColor({.56f,.32f,.20f},{.83f,.68f,.43f}):rndColor({.49f,.48f,.44f},{.79f,.73f,.61f}));
      buildings.push_back({x,z,w,d,h,c,shop,dealer});if(dealer)dealerBuilding=static_cast<int>(buildings.size()-1);box(cityVerts,{x,h*.5f,z},{w,h,d},c);
      box(cityVerts,{x,h+.22f,z},{w+.5f,.44f,d+.5f},{.31f,.30f,.28f});
      const int rows=shop?1:std::max(2,static_cast<int>(h/5.0f));
      for(int side:{-1,1})for(int r=0;r<rows;r++)for(int col=0;col<4;col++){
        float wx=x+(col-1.5f)*w*.19f,wy=2.2f+r*(h-3.f)/rows;
        box(cityVerts,{wx,wy,z+side*(d*.5f+.035f)},{w*.10f,1.25f,.06f},random()<.52f?Color{.25f,.40f,.48f}:Color{.76f,.64f,.42f});
        box(cityVerts,{x+side*(w*.5f+.035f),wy,z+(col-1.5f)*d*.19f},{.06f,1.25f,d*.10f},{.28f,.41f,.47f});
      }
      // Door and colored shop canopy.

      if(shop)box(cityVerts,{x,dealer?5.25f:4.25f,z+d*.5f+.14f},{w*.72f,.9f,.28f},dealer?Color{.72f,.12f,.07f}:rndColor({.54f,.10f,.07f},{.10f,.38f,.24f}));
      if(dealer){box(cityVerts,{x,7.1f,z+d*.5f+.24f},{w*.63f,1.1f,.16f},{.18f,.52f,.58f});box(cityVerts,{x,3.8f,z+d*.5f+6.f},{22,.15f,10},{.22f,.24f,.25f});for(int k=0;k<3;k++)box(cityVerts,{x-6.f+k*6.f,.55f,z+d*.5f+5.f},{2.1f,.75f,4.2f},k==1?Color{.76f,.09f,.05f}:Color{.20f,.37f,.52f});}
      box(cityVerts,{x+w*.28f,h+.85f,z-d*.2f},{2.2f,1.5f,2.2f},{.35f,.39f,.39f});
    }
    // Park strip, palms and streetlights.
    box(cityVerts,{0,.015f,-half+32},{41,.07f,41},{.20f,.42f,.19f});
    for(int i=0;i<110;i++){
      float x=(random()*2-1)*half,z=(random()*2-1)*half;
      float gx=std::round((x+half)/blockSize)*blockSize-half,gz=std::round((z+half)/blockSize)*blockSize-half;
      if(std::abs(x-gx)<14||std::abs(z-gz)<14){
        box(cityVerts,{x,2.4f,z},{.48f,4.8f,.48f},{.40f,.27f,.13f});
        for(int l=0;l<6;l++){float a=l*kPi/3;V3 c0{x+std::sin(a)*1.05f,5.2f,z+std::cos(a)*1.05f};box(cityVerts,c0,{.44f,.18f,2.5f},{.13f,.34f,.13f},a);}
      }
    }
    // Parked cars and traffic vehicles with simple colored bodies/cabins.
    for(int i=0;i<32;i++){
      bool horizontal=random()<.5f;int idx=static_cast<int>(random()*(kBlocks+1));float p=-half+idx*blockSize;
      float along=-half+random()*kCity;float lane=(random()<.5f?-1.f:1.f)*5.2f;
      Car car{horizontal?along:p+lane,horizontal?p+lane:along,horizontal?(random()<.5f?0.f:kPi):kPi*.5f,0,rndColor({.12f,.21f,.34f},{.85f,.62f,.17f}),false,false};cars.push_back(car);
    }
    for(int i=0;i<12;i++){
      bool h=random()<.5f;int idx=static_cast<int>(random()*(kBlocks+1));float p=-half+idx*blockSize;float lane=random()<.5f?-4.f:4.f;
      cars.push_back({h?-half+random()*kCity:p+lane,h?p+lane:-half+random()*kCity,h?(random()<.5f?kPi*.5f:-kPi*.5f):(random()<.5f?0:kPi),5+random()*4,{.20f,.35f,.48f},true,false});
    }
    // Named story characters have distinct outfits, positions and conversations.
    npcs.push_back({10.f,-12.f,0,0,{.91f,.89f,.82f},0});
    npcs.push_back({packageX,packageZ,0,0,{.35f,.48f,.25f},1});
    npcs.push_back({deliveryX,deliveryZ,0,0,{.17f,.42f,.40f},2});
    npcs.push_back({taxiPickupX,taxiPickupZ,0,0,{.68f,.59f,.45f},3});
    if(dealerBuilding>=0){const auto&db=buildings[dealerBuilding];npcs.push_back({db.x,db.z+db.d*.5f+4.f,0,0,{.20f,.34f,.54f},4});}
    for(int i=0;i<24;i++){bool h=random()<.5f;float road=-half+static_cast<int>(random()*(kBlocks+1))*blockSize;float along=-half+random()*kCity;float side=random()<.5f?-10.2f:10.2f;npcs.push_back({h?along:road+side,h?road+side:along,random()*6.2f,1+random()*1.2f,rndColor({.15f,.25f,.40f},{.65f,.58f,.42f}),-1});}
    for(int i=0;i<120;i++){
      float x,z;int attempts=0;do{x=-half+random()*kCity;z=-half+random()*kCity;attempts++;}while(attempts<100&&std::abs(x-std::round((x+half)/blockSize)*blockSize+half)>13&&std::abs(z-std::round((z+half)/blockSize)*blockSize+half)>13);
      trash.push_back({x,z,true});
    }
    // Exactly 120,000 visible surface detail points, one GL_POINTS draw call.
    pointVerts.reserve(kDetailPoints);std::array<Color,5> palette={Color{.31f,.30f,.28f},Color{.36f,.34f,.30f},Color{.40f,.37f,.32f},Color{.28f,.29f,.29f},Color{.42f,.39f,.34f}};
    int made=0,tries=0;while(made<kDetailPoints&&tries<kDetailPoints*8){tries++;float x=(random()*2-1)*half,z=(random()*2-1)*half;float gx=std::round((x+half)/blockSize)*blockSize-half,gz=std::round((z+half)/blockSize)*blockSize-half;float dx=std::abs(x-gx),dz=std::abs(z-gz);if(!(dx<=11||dz<=11))continue;Color c=palette[randomU()%palette.size()];float shade=.82f+random()*.16f;pointVerts.push_back({x,.055f+random()*.006f,z,c.r*shade,c.g*shade,c.b*shade,.62f});made++;}
    LOGI("World ready: buildings=%zu points=%zu",buildings.size(),pointVerts.size());
  }

  void drawMesh(GLuint buffer,const std::vector<Vertex>&v,const M4&m,GLenum primitive,float pointSize=1.f,size_t first=0,size_t count=static_cast<size_t>(-1),bool textured=false){if(v.empty()||first>=v.size())return;count=std::min(count,v.size()-first);glUseProgram(program);glBindBuffer(GL_ARRAY_BUFFER,buffer);glEnableVertexAttribArray(posLoc);glEnableVertexAttribArray(colorLoc);glEnableVertexAttribArray(uvLoc);glVertexAttribPointer(posLoc,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,x)));glVertexAttribPointer(colorLoc,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,r)));glVertexAttribPointer(uvLoc,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,u)));glUniformMatrix4fv(mvpLoc,1,GL_FALSE,m.v);glUniform1f(pointLoc,pointSize);glUniform1f(texturedLoc,textured?1.f:0.f);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,fontTexture);glUniform1i(fontLoc,0);glDrawArrays(primitive,static_cast<GLint>(first),static_cast<GLsizei>(count));}
  bool collides(float x,float z){for(const auto&b:buildings){float r=.65f;if(x+r>b.x-b.w*.5f&&x-r<b.x+b.w*.5f&&z+r>b.z-b.d*.5f&&z-r<b.z+b.d*.5f)return true;}return false;}
  float distance(float x,float z,float a,float b){return std::sqrt((x-a)*(x-a)+(z-b)*(z-b));}
  int nearestCar(){int best=-1;float bd=3.5f;for(int i=0;i<static_cast<int>(cars.size());i++){if(cars[i].traffic)continue;float d=distance(player.x,player.z,cars[i].x,cars[i].z);if(d<bd){bd=d;best=i;}}return best;}
  int nearestDoor(){int best=-1;float bd=4.4f;for(int i=0;i<static_cast<int>(buildings.size());i++){const auto&b=buildings[i];float d=distance(player.x,player.z,b.x,b.z+b.d*.5f+1.8f);if(d<bd){bd=d;best=i;}}return best;}
  bool missionInteract(){
    if(!missionTargetVisible||distance(player.x,player.z,missionTarget.x,missionTarget.z)>5.f)return false;
    if(questStage==2){packageCarried=true;questStage=3;setObjective("وصل الصندوق إلى المقهى عند الطريق الشرقي",deliveryX,deliveryZ,true);toast("استلمت الصندوق، أوصله للزبون");return true;}
    if(questStage==3&&packageCarried){packageCarried=false;money+=200.f;missionTargetVisible=false;missionProgress=0;showDialog(2,{"وصلت الطلب في وقتها، تسلم يا سالم! هذه ٢٠٠ ريال مكافأتك.","موقف الأجرة قريب. خذ الراكب إلى السوق، ولك ٢٠٠ ريال ثانية."},3);return true;}
    if(questStage==4){passengerOnboard=true;questStage=5;setObjective("أوصل الراكب إلى سوق البطحاء",taxiDropX,taxiDropZ,true);showDialog(3,{"لو سمحت يا كابتن، وصلني إلى سوق البطحاء.","إذا وصلت اضغط تفاعل عند العلامة الصفراء."},0);return true;}
    if(questStage==5){passengerOnboard=false;money+=200.f;missionTargetVisible=false;showDialog(0,{"بيض الله وجهك يا سالم، خدمة ممتازة! تفضل ٢٠٠ ريال.","صار معك ٥٥٠ ريال. رح لمعرض السيارات واشتر أول سيارة لك بـ٥٠٠ ريال."},4);return true;}
    if(questStage==6&&dealerBuilding>=0){showDialog(4,{"أهلاً بك في معرض حي النخيل. هذه سيدان جديدة ومضمونة.","السعر ٥٠٠ ريال. اضغط تفاعل مرة ثانية للتأكيد والشراء."},5);return true;}
    return false;
  }
  void interact(){
    if(dialogueOpen){advanceDialog();return;}
    if(pendingEnter)return;
    if(room){room=false;player=exteriorReturn;insideBuilding=-1;doorAnim=0;playerY=0;return;}
    if(missionInteract())return;
    if(drivenCar>=0){Car&c=cars[drivenCar];player={c.x+std::cos(c.yaw)*2.f,0,c.z-std::sin(c.yaw)*2.f};drivenCar=-1;return;}
    int car=nearestCar();if(car>=0){drivenCar=car;cars[car].speed=0;toast(cars[car].owned?"ركبت سيارتك الجديدة":"ركبت السيارة؛ استخدم WASD للقيادة");return;}
    int b=nearestDoor();if(b>=0&&!pendingEnter){insideBuilding=b;const auto&shop=buildings[b];exteriorReturn={shop.x,0,shop.z+shop.d*.5f+3.f};doorAnim=0;pendingEnter=true;return;}
    int closest=-1;float best=3.3f;for(int i=0;i<static_cast<int>(npcs.size());i++){float d=distance(player.x,player.z,npcs[i].x,npcs[i].z);if(d<best&&npcs[i].identity>=0){best=d;closest=i;}}if(closest>=0){int id=npcs[closest].identity;if(id==0)showDialog(0,{"حياك يا سالم. شغلك مرتب، كمل مهامك وبتقدر تشتري سيارتك."},0);else if(id==4)showDialog(4,{"المعرض يفتح بعد ما تكمل التوصيلات وتجمع ٥٠٠ ريال."},0);else showDialog(id,{"حياك الله، الله يوفقك في مشوارك."},0);}
  }
  void inputEvent(AInputEvent*e){
    if(AInputEvent_getType(e)==AINPUT_EVENT_TYPE_KEY){int key=AKeyEvent_getKeyCode(e);bool down=AKeyEvent_getAction(e)==AKEY_EVENT_ACTION_DOWN;switch(key){case AKEYCODE_W:case AKEYCODE_DPAD_UP:keyForward=down;break;case AKEYCODE_S:case AKEYCODE_DPAD_DOWN:keyBack=down;break;case AKEYCODE_A:case AKEYCODE_DPAD_LEFT:keyLeft=down;break;case AKEYCODE_D:case AKEYCODE_DPAD_RIGHT:keyRight=down;break;case AKEYCODE_SHIFT_LEFT:case AKEYCODE_SHIFT_RIGHT:keyRun=down;break;case AKEYCODE_SPACE:keyJump=down;if(down)jumpRequested=true;break;case AKEYCODE_E:if(down)interact();break;case AKEYCODE_BACK:if(down){if(room)interact();else ANativeActivity_finish(app->activity);}break;}return;}
    if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return;
    int action=AMotionEvent_getAction(e),masked=action&AMOTION_EVENT_ACTION_MASK,index=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;int id=AMotionEvent_getPointerId(e,index);float x=AMotionEvent_getX(e,index),y=AMotionEvent_getY(e,index),w=std::max(1,width),h=std::max(1,height);
    if(masked==AMOTION_EVENT_ACTION_DOWN||masked==AMOTION_EVENT_ACTION_POINTER_DOWN){float jx=w*.18f,jy=h*.78f,joyRange=std::min(w,h)*.14f;if(x<w*.43f&&y>h*.52f){joyId=id;joyActive=true;joyX=(x-jx)/joyRange;joyY=(y-jy)/joyRange;}
      else if(x>w*.57f&&y>h*.53f){float runX=w*.67f,runY=h*.82f,jumpX=w*.84f,jumpY=h*.68f,useX=w*.87f,useY=h*.87f;if(distance(x,y,runX,runY)<std::min(w,h)*.12f)runHeld=true;else if(distance(x,y,jumpX,jumpY)<std::min(w,h)*.12f)jumpRequested=true;else if(distance(x,y,useX,useY)<std::min(w,h)*.12f)interact();}
      else{cameraId=id;cameraHeld=true;lastTouchX=x;lastTouchY=y;}return;}
    if(masked==AMOTION_EVENT_ACTION_MOVE){for(size_t p=0;p<AMotionEvent_getPointerCount(e);p++){int pid=AMotionEvent_getPointerId(e,p);float px=AMotionEvent_getX(e,p),py=AMotionEvent_getY(e,p);if(pid==joyId){float range=std::min(w,h)*.14f;joyX=std::clamp((px-w*.18f)/range,-1.f,1.f);joyY=std::clamp((py-h*.78f)/range,-1.f,1.f);}if(pid==cameraId){cameraYaw-=(px-lastTouchX)*.006f;cameraPitch=std::clamp(cameraPitch+(py-lastTouchY)*.004f,-.02f,.65f);lastTouchX=px;lastTouchY=py;}}return;}
    if(masked==AMOTION_EVENT_ACTION_UP||masked==AMOTION_EVENT_ACTION_POINTER_UP||masked==AMOTION_EVENT_ACTION_CANCEL){if(id==joyId){joyId=-1;joyActive=false;joyX=joyY=0;}if(id==cameraId){cameraId=-1;cameraHeld=false;}runHeld=false;}
  }
  static int inputThunk(android_app*a,AInputEvent*e){auto*g=static_cast<NativeGame*>(a->userData);if(g){g->inputEvent(e);return 1;}return 0;}
  static void commandThunk(android_app*a,int32_t cmd){auto*g=static_cast<NativeGame*>(a->userData);if(!g)return;switch(cmd){case APP_CMD_INIT_WINDOW:if(a->window&&!g->ready)g->initGL();break;case APP_CMD_TERM_WINDOW:if(g->ready)g->loseGL();break;case APP_CMD_GAINED_FOCUS:g->focused=true;break;case APP_CMD_LOST_FOCUS:g->focused=false;break;default:break;}}

  void update(float dt){
    dt=std::clamp(dt,0.f,.04f);toastTime=std::max(0.f,toastTime-dt);
    if(dialogueOpen){speed=0;return;}
    if(pendingEnter){doorAnim=std::min(1.f,doorAnim+dt*3.4f);if(doorAnim>=1.f){pendingEnter=false;room=true;player={0,0,0};playerY=verticalVelocity=0;}return;}
    float fx=std::sin(cameraYaw),fz=std::cos(cameraYaw),rx=fz,rz=-fx;
    float forward=(keyForward?1.f:0.f)-(keyBack?1.f:0.f),side=(keyRight?1.f:0.f)-(keyLeft?1.f:0.f);if(joyActive){forward=-joyY;side=joyX;}
    float mx=fx*forward+rx*side,mz=fz*forward+rz*side,ml=std::sqrt(mx*mx+mz*mz);if(ml>1){mx/=ml;mz/=ml;}
    bool sprint=keyRun||runHeld;float maxSpeed=sprint?8.8f:4.5f;
    if(drivenCar>=0){Car&c=cars[drivenCar];c.speed+=(forward*20.f-c.speed)*std::min(1.f,dt*1.6f);c.yaw+=side*dt*1.45f*std::clamp(std::abs(c.speed)/4.f,.25f,1.f);float nx=c.x+std::sin(c.yaw)*c.speed*dt,nz=c.z+std::cos(c.yaw)*c.speed*dt;if(!collides(nx,nz)){c.x=nx;c.z=nz;}else if(std::abs(c.speed)>2.f){c.speed=0;health=std::max(0.f,health-4.f);toast("انتبه للطريق، تضررت السيارة");}player={c.x,0,c.z};}
    else{float blend=std::min(1.f,dt*9.f);speed+=(ml*maxSpeed-speed)*blend;float nx=player.x+mx*maxSpeed*dt,nz=player.z+mz*maxSpeed*dt;if(!collides(nx,player.z))player.x=nx;if(!collides(player.x,nz))player.z=nz;if(ml>.05f)playerYaw=std::atan2(mx,mz);if(jumpRequested&&playerY<=.001f){verticalVelocity=6.8f;jumpRequested=false;}verticalVelocity-=17.f*dt;playerY+=verticalVelocity*dt;if(playerY<0){playerY=0;verticalVelocity=0;}walkPhase+=dt*(3+speed*1.4f);}
    if(room){player.x=std::clamp(player.x,-5.f,5.f);player.z=std::clamp(player.z,-5.f,5.f);}
    if(!room){float half=kCity*.5f-2;player.x=std::clamp(player.x,-half,half);player.z=std::clamp(player.z,-half,half);hunger=std::max(0.f,hunger-dt*.006f);if(hunger<=0)health=std::max(1.f,health-dt*.25f);
      for(auto&t:trash)if(t.alive&&distance(player.x,player.z,t.x,t.z)<1.2f){t.alive=false;if(questStage==1&&trashQuest<3){trashQuest++;missionProgress=trashQuest;if(trashQuest>=3){money+=150.f;missionTargetVisible=false;showDialog(0,{"كفو يا سالم! جمعت المطلوب ونظفت الشارع.","هذه ١٥٠ ريالاً لك. عند المخبز صندوق طلب؛ وصله للمقهى وخذ ٢٠٠ ريال."},2);}}else money+=5.f;}
      if(questStage==1&&!dialogueOpen){int nearest=-1;float nearestDistance=std::numeric_limits<float>::max();for(int i=0;i<static_cast<int>(trash.size());i++)if(trash[i].alive){float d=distance(player.x,player.z,trash[i].x,trash[i].z);if(d<nearestDistance){nearestDistance=d;nearest=i;}}if(nearest>=0){missionTarget={trash[nearest].x,0,trash[nearest].z};missionTargetVisible=true;}else missionTargetVisible=false;}
      for(auto&n:npcs)if(n.identity<0){n.phase+=dt*n.speed;n.x+=std::sin(n.phase)*dt*.45f;n.z+=std::cos(n.phase)*dt*.45f;if(std::abs(n.x)>half||std::abs(n.z)>half||collides(n.x,n.z)){n.phase+=kPi;n.x+=std::sin(n.phase)*dt;n.z+=std::cos(n.phase)*dt;}}
      for(int i=0;i<static_cast<int>(cars.size());i++){auto&c=cars[i];if(c.traffic&&i!=drivenCar){c.x+=std::sin(c.yaw)*c.speed*dt;c.z+=std::cos(c.yaw)*c.speed*dt;if(std::abs(c.x)>half||std::abs(c.z)>half){if(std::abs(std::sin(c.yaw))>.5f)c.x=-std::copysign(half-1,c.x);else c.z=-std::copysign(half-1,c.z);}}}}
  }

  void addPerson(std::vector<Vertex>&v,float x,float z,float phase,Color palette,bool playerModel,int identity=-1,float baseY=0){
    float yaw=playerModel?playerYaw:(identity>=0?0.f:std::sin(phase)*.25f),swing=std::sin(phase)*.18f,bob=std::abs(std::cos(phase))*.025f;
    Color robe=playerModel?Color{.92f,.91f,.86f}:identity==1?Color{.88f,.77f,.56f}:identity==2?Color{.35f,.43f,.39f}:identity==3?Color{.72f,.70f,.64f}:identity==4?Color{.35f,.55f,.72f}:palette;
    Color skin{.62f,.40f,.28f};auto pt=[&](float lx,float y,float lz){return add({x,y+baseY+ bob,z},rotateY({lx,0,lz},yaw));};
    cylinderBetween(v,{x,baseY+.12f,z},{x,baseY+1.22f+bob,z},.34f,.23f,robe,10);
    // Distinct uniforms/vests for story NPCs.
    if(identity==1)box(v,add({x,baseY+.78f,z},rotateY({0,0,.235f},yaw)),{.38f,.58f,.045f},{.20f,.39f,.16f},yaw);
    if(identity==2||identity==4)box(v,add({x,baseY+.87f,z},rotateY({0,0,.24f},yaw)),{.43f,.58f,.05f},identity==2?Color{.22f,.25f,.23f}:Color{.12f,.22f,.34f},yaw);
    cylinderBetween(v,pt(0,1.20f,0),pt(0,1.34f,0),.105f,.105f,skin,8);
    sphere(v,{x,baseY+1.52f+bob,z},{.18f,.21f,.18f},skin,yaw,9,12);
    // Ghutra and agal are modeled geometry rather than flat decals.
    Color headcloth=identity==0||playerModel?Color{.90f,.89f,.84f}:identity==1?Color{.92f,.91f,.85f}:Color{.88f,.87f,.82f};
    sphere(v,{x,baseY+1.70f+bob,z},{.23f,.13f,.22f},headcloth,yaw,6,12);
    torus(v,{x,baseY+1.76f+bob,z},.18f,.026f,{.10f,.09f,.08f},yaw);
    if(identity==0||playerModel){for(int k=-1;k<=1;k++)box(v,add({x,baseY+1.71f+bob,z},rotateY({k*.085f,.04f,.05f},yaw)),{.035f,.16f,.30f},{.68f,.12f,.10f},yaw);}
    // Eyes, nose, hands and articulated limbs.
    sphere(v,add({x,baseY+1.53f+bob,z},rotateY({-.065f,0,.165f},yaw)),{.018f,.018f,.018f},{.07f,.06f,.05f},yaw,5,6);
    sphere(v,add({x,baseY+1.53f+bob,z},rotateY({.065f,0,.165f},yaw)),{.018f,.018f,.018f},{.07f,.06f,.05f},yaw,5,6);
    for(int side:{-1,1}){V3 shoulder=pt(side*.27f,1.08f,0),hand=pt(side*.37f,.48f,side*swing);cylinderBetween(v,shoulder,hand,.095f,.065f,robe,7);sphere(v,hand,{.07f,.075f,.07f},skin,yaw,5,7);float legSwing=side*swing*.8f;V3 hip=pt(side*.13f,.38f,0),foot=pt(side*.14f,.05f,legSwing);cylinderBetween(v,hip,foot,.095f,.065f,{.17f,.16f,.15f},7);box(v,add(foot,rotateY({0,-.015f,.06f},yaw)),{.16f,.08f,.26f},{.12f,.11f,.10f},yaw);}
  }
  void addCar(std::vector<Vertex>&v,const Car&c,bool playerCar=false){
    float yaw=c.yaw;auto at=[&](float x,float y,float z){return add({c.x,y,c.z},rotateY({x,0,z},yaw));};
    Color body=c.color;box(v,{c.x,.48f,c.z},{1.95f,.72f,4.15f},body,yaw);box(v,at(0,.99f,-.08f),{1.62f,.66f,2.15f},{.14f,.25f,.31f},yaw);
    box(v,at(0,.99f,1.02f),{1.48f,.48f,.07f},{.23f,.39f,.47f},yaw);box(v,at(0,.99f,-1.18f),{1.48f,.48f,.07f},{.23f,.39f,.47f},yaw);
    for(int side:{-1,1}){box(v,at(side*.99f,.43f,.85f),{.07f,.22f,.82f},{.94f,.84f,.59f},yaw);box(v,at(side*.99f,.42f,-.90f),{.07f,.22f,.72f},{.94f,.84f,.59f},yaw);}
    for(int side:{-1,1})for(int f:{-1,1}){V3 center=at(side*1.02f,.27f,f*1.33f);cylinderBetween(v,add(center,rotateY({-.08f,0,0},yaw)),add(center,rotateY({.08f,0,0},yaw)),.34f,.34f,{.07f,.07f,.075f},10);cylinderBetween(v,add(center,rotateY({-.085f,0,0},yaw)),add(center,rotateY({.085f,0,0},yaw)),.14f,.14f,{.53f,.56f,.58f},8);}
    box(v,at(-.65f,.62f,2.1f),{.42f,.20f,.08f},{1.f,.92f,.64f},yaw);box(v,at(.65f,.62f,2.1f),{.42f,.20f,.08f},{1.f,.92f,.64f},yaw);box(v,at(-.65f,.60f,-2.1f),{.38f,.18f,.08f},{.76f,.12f,.08f},yaw);box(v,at(.65f,.60f,-2.1f),{.38f,.18f,.08f},{.76f,.12f,.08f},yaw);box(v,at(0,.42f,2.11f),{.7f,.12f,.08f},{.76f,.76f,.72f},yaw);(void)playerCar;
  }
  void addRoom(std::vector<Vertex>&v){box(v,{0,-.1f,0},{14,.2f,14},{.60f,.53f,.42f});box(v,{0,2.2f,7},{14,4.6f,.25f},{.77f,.70f,.57f});box(v,{-7,2.2f,0},{.25f,4.6f,14},{.77f,.70f,.57f});box(v,{7,2.2f,0},{.25f,4.6f,14},{.77f,.70f,.57f});box(v,{0,4.55f,0},{14,.18f,14},{.55f,.49f,.40f});for(int i=0;i<4;i++){box(v,{-4.5f+i*3,1.1f,-2.6f},{2.1f,2.2f,.75f},{.32f,.22f,.13f});box(v,{-4.5f+i*3,1.9f,-2.6f},{2.1f,.18f,.82f},{.73f,.43f,.13f});}box(v,{0,1.1f,4.8f},{3.4f,2.2f,.8f},{.29f,.21f,.15f});}

  std::string arabicNumber(int value){std::string out;std::string n=std::to_string(std::max(0,value));for(char c:n){uint32_t cp=0x0660+(c-'0');if(cp<0x80)out.push_back(static_cast<char>(cp));else if(cp<0x800){out.push_back(static_cast<char>(0xC0|(cp>>6)));out.push_back(static_cast<char>(0x80|(cp&0x3F)));}}return out;}
  void circleScreen(std::vector<Vertex>&v,float px,float py,float radiusPx,Color c){float x=px/width*2.f-1.f,y=1.f-py/height*2.f,rx=radiusPx*2.f/width,ry=radiusPx*2.f/height;for(int i=0;i<28;i++){float a=i*2*kPi/28,b=(i+1)*2*kPi/28;tri(v,{x,y,0},{x+std::cos(a)*rx,y+std::sin(a)*ry,0},{x+std::cos(b)*rx,y+std::sin(b)*ry,0},c);}}
  void addControlCircle(std::vector<Vertex>&v,float px,float py,float radiusPx,Color outer,Color inner){circleScreen(v,px,py,radiusPx,outer);circleScreen(v,px,py,radiusPx*.79f,inner);}
  void addWorldMarker(){if(!missionTargetVisible||room)return;float spin=walkPhase*.3f;for(int i=0;i<12;i++){float a=spin+i*kPi/6.f;box(frameVerts,{missionTarget.x+std::cos(a)*1.35f,.12f,missionTarget.z+std::sin(a)*1.35f},{.34f,.08f,.12f},{.98f,.73f,.10f},-a);}box(frameVerts,{missionTarget.x,1.1f,missionTarget.z},{.22f,2.1f,.22f},{.98f,.72f,.08f});sphere(frameVerts,{missionTarget.x,2.25f+std::sin(walkPhase)*.12f,missionTarget.z},{.42f,.42f,.42f},{1.f,.82f,.20f},0,7,10);}
  void makeFrame(){
    frameVerts.clear();if(room)addRoom(frameVerts);
    if(drivenCar<0)addPerson(frameVerts,player.x,player.z,walkPhase,{.88f,.87f,.80f},true,-1,playerY);
    for(const auto&n:npcs){if(n.identity==3&&passengerOnboard)continue;addPerson(frameVerts,n.x,n.z,n.phase,n.shirt,false,n.identity);}
    for(const auto&c:cars)addCar(frameVerts,c,drivenCar>=0);
    if(!room){
      for(const auto&t:trash)if(t.alive){Color waste{.51f,.74f,.34f};box(frameVerts,{t.x,.13f,t.z},{.19f,.28f,.18f},waste,walkPhase*.04f);cylinderBetween(frameVerts,{t.x,.25f,t.z},{t.x,.40f,t.z},.055f,.05f,{.67f,.82f,.58f},7);}
      for(int i=0;i<static_cast<int>(buildings.size());i++){const auto&b=buildings[i];if(distance(player.x,player.z,b.x,b.z+b.d*.5f)<28){float angle=(pendingEnter&&insideBuilding==i)?-doorAnim*1.45f:0.f;box(frameVerts,{b.x,1.15f,b.z+b.d*.5f+.12f},{2.1f,2.3f,.16f},{.30f,.20f,.12f},angle);}}
      if(questStage==2){box(frameVerts,{packageX,.46f,packageZ},{.85f,.78f,.85f},{.57f,.35f,.17f});box(frameVerts,{packageX,.47f,packageZ},{.12f,.8f,.89f},{.83f,.67f,.34f});}
      addWorldMarker();
    }
    worldFrameCount=frameVerts.size();
    auto nx=[&](float p){return p/static_cast<float>(width)*2.f-1.f;};auto ny=[&](float p){return 1.f-p/static_cast<float>(height)*2.f;};auto rectScreen=[&](float x,float y,float w,float h,Color c){rect2d(frameVerts,nx(x),ny(y),w/width*2.f,-h/height*2.f,c);};
    const Color panel{.035f,.055f,.075f,.90f},edge{.88f,.67f,.18f,.90f},white{.95f,.96f,.94f,1.f},gold{1.f,.80f,.25f,1.f};
    float shortSide=static_cast<float>(std::min(width,height));float uiPx=std::max(23.f,shortSide*.031f);float smallPx=std::max(18.f,shortSide*.024f);
    // Clean Arabic mission card on the left.
    rectScreen(width*.025f,height*.045f,width*.335f,height*.165f,panel);rectScreen(width*.025f,height*.045f,width*.335f,3.f,edge);
    drawArabicLine(frameVerts,"المهمة الحالية",width*.345f,height*.07f,uiPx,gold);
    std::string objectiveLine=objective.empty()?"استكشف المدينة وتحدث مع أهلها":objective;
    if(missionTotal>0)objectiveLine+="  "+arabicNumber(missionProgress)+" / "+arabicNumber(missionTotal);
    drawArabicWrapped(frameVerts,objectiveLine,width*.345f,height*.12f,width*.30f,smallPx,white,2);
    // Health, food and wallet panel.
    rectScreen(width*.69f,height*.045f,width*.285f,height*.205f,panel);rectScreen(width*.69f,height*.045f,width*.285f,3.f,edge);
    drawArabicLine(frameVerts,"الصحة  "+arabicNumber(static_cast<int>(health))+"٪",width*.955f,height*.07f,smallPx,white);
    rectScreen(width*.71f,height*.118f,width*.245f,height*.018f,{.12f,.15f,.17f,1});rectScreen(width*.71f,height*.118f,width*.245f*std::clamp(health/100.f,0.f,1.f),height*.018f,{.83f,.17f,.16f,1});
    drawArabicLine(frameVerts,"الشبع  "+arabicNumber(static_cast<int>(hunger))+"٪",width*.955f,height*.15f,smallPx,white);
    rectScreen(width*.71f,height*.195f,width*.245f,height*.014f,{.12f,.15f,.17f,1});rectScreen(width*.71f,height*.195f,width*.245f*std::clamp(hunger/100.f,0.f,1.f),height*.014f,{.83f,.58f,.16f,1});
    drawArabicLine(frameVerts,"الرصيد  "+arabicNumber(static_cast<int>(money))+" ريال",width*.955f,height*.225f,smallPx,gold);
    // Touch joystick and large, labeled actions.
    float radius=shortSide*.078f;float joyXpx=width*.17f+joyX*shortSide*.105f,joyYpx=height*.80f+joyY*shortSide*.105f;addControlCircle(frameVerts,width*.17f,height*.80f,radius,{.10f,.14f,.19f,.88f},{.29f,.34f,.39f,.92f});circleScreen(frameVerts,joyXpx,joyYpx,radius*.36f,{.91f,.71f,.22f,1});
    addControlCircle(frameVerts,width*.67f,height*.82f,radius*.72f,{.18f,.09f,.055f,.92f},{.84f,.34f,.12f,.98f});
    addControlCircle(frameVerts,width*.83f,height*.68f,radius*.72f,{.17f,.13f,.045f,.92f},{.91f,.72f,.15f,.98f});
    addControlCircle(frameVerts,width*.88f,height*.86f,radius*.72f,{.045f,.13f,.11f,.92f},{.12f,.59f,.43f,.98f});
    drawArabicLine(frameVerts,"تحرك",width*.21f,height*.62f,smallPx,white);
    drawArabicLine(frameVerts,"ركض",width*.69f,height*.70f,smallPx,white);
    drawArabicLine(frameVerts,"قفز",width*.85f,height*.56f,smallPx,white);
    drawArabicLine(frameVerts,dialogueOpen?"تابع":"تفاعل",width*.90f,height*.74f,smallPx,white);
    // Story dialogue card; every line uses the bundled Arabic font and RTL shaping.
    if(dialogueOpen&&!dialogPages.empty()){
      float left=width*.255f,top=height*.485f,cardW=width*.49f,cardH=height*.245f;rectScreen(left,top,cardW,cardH,{.025f,.045f,.065f,.96f});rectScreen(left,top,cardW,4.f,edge);
      drawArabicLine(frameVerts,speakerName(),left+cardW-width*.018f,top+height*.022f,uiPx,gold);
      drawArabicWrapped(frameVerts,dialogPages[dialogPage],left+cardW-width*.022f,top+height*.074f,cardW-width*.045f,smallPx*1.08f,white,3);
      drawArabicLine(frameVerts,"اضغط تفاعل للمتابعة",left+cardW-width*.018f,top+cardH-height*.035f,std::max(17.f,smallPx*.78f),{.72f,.82f,.89f,1});
    }
    if(toastTime>0.f){rectScreen(width*.34f,height*.29f,width*.32f,height*.062f,{.025f,.045f,.065f,.88f});drawArabicLine(frameVerts,toastText,width*.65f,height*.307f,smallPx,white);}
    drawLatinLine(frameVerts,"MFA 1",width*.43f,height*.035f,std::max(16.f,smallPx*.75f),{.95f,.92f,.83f,.92f});drawArabicLine(frameVerts,"مدينة النخيل",width*.98f,height*.035f,std::max(16.f,smallPx*.75f),{.95f,.92f,.83f,.92f});
  }
  void render(){if(!ready||!app->window)return;eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);glViewport(0,0,width,height);glClearColor(.43f,.66f,.80f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    float aspect=static_cast<float>(width)/std::max(1,height);V3 target={player.x,1.05f,player.z};float horiz=10.f*std::cos(cameraPitch);V3 eye={player.x-std::sin(cameraYaw)*horiz,player.y+2.8f+10.f*std::sin(cameraPitch),player.z-std::cos(cameraYaw)*horiz};M4 vp=multiply(perspective(62*kPi/180.f,aspect,.1f,1200.f),lookAt(eye,target,{0,1,0}));
    if(!room){drawMesh(cityBuffer,cityVerts,vp,GL_TRIANGLES);drawMesh(detailBuffer,pointVerts,vp,GL_POINTS,1.2f);}
    makeFrame();upload(frameBuffer,frameVerts,GL_DYNAMIC_DRAW);
    if(worldFrameCount>0)drawMesh(frameBuffer,frameVerts,vp,GL_TRIANGLES,1.f,0,worldFrameCount);
    glDisable(GL_DEPTH_TEST);drawMesh(frameBuffer,frameVerts,identity(),GL_TRIANGLES,1.f,worldFrameCount,frameVerts.size()-worldFrameCount,true);glEnable(GL_DEPTH_TEST);
    eglSwapBuffers(display,surface);
  }
};

static void androidCommand(android_app*a,int32_t cmd){auto*g=static_cast<NativeGame*>(a->userData);if(g)NativeGame::commandThunk(a,cmd);}
static int androidInput(android_app*a,AInputEvent*e){auto*g=static_cast<NativeGame*>(a->userData);return g?([&](){g->inputEvent(e);return 1;})():0;}
}

extern "C" void android_main(android_app* app){app_dummy();NativeGame game;game.app=app;game.beginStory();app->userData=&game;app->onAppCmd=androidCommand;app->onInputEvent=androidInput;ANativeActivity_setWindowFlags(app->activity,0x00000400u,0);auto previous=std::chrono::steady_clock::now();while(!app->destroyRequested){int events=0;android_poll_source*source=nullptr;int timeout=(game.ready&&game.focused)?0:-1;int ident=ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source));if(ident>=0&&source)source->process(app,source);if(app->destroyRequested)break;if(game.ready&&game.focused){auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-previous).count();previous=now;game.update(dt);game.render();}}game.loseGL();}
