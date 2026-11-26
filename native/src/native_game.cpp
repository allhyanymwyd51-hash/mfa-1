#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
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
#include <initializer_list>
#include <chrono>
#include <string>
#include <vector>

namespace {
constexpr char kLog[] = "MFA_NATIVE";
constexpr float kPi = 3.1415926535f;
constexpr float kCity = 768.0f;
constexpr int kBlocks = 12;
constexpr int kDetailPoints = 120000;
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

struct Vertex { float x,y,z,r,g,b,a; };
struct Color { float r,g,b; };
void tri(std::vector<Vertex>&v,V3 a,V3 b,V3 c,Color q){v.push_back({a.x,a.y,a.z,q.r,q.g,q.b,1});v.push_back({b.x,b.y,b.z,q.r,q.g,q.b,1});v.push_back({c.x,c.y,c.z,q.r,q.g,q.b,1});}
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
void rect2d(std::vector<Vertex>&v,float x,float y,float w,float h,Color c){quad(v,{x,y,0},{x+w,y,0},{x+w,y+h,0},{x,y+h,0},c);}
void circle2d(std::vector<Vertex>&v,float x,float y,float r,Color c){for(int i=0;i<28;i++){float a=i*2*kPi/28,b=(i+1)*2*kPi/28;tri(v,{x,y,0},{x+std::cos(a)*r,y+std::sin(a)*r,0},{x+std::cos(b)*r,y+std::sin(b)*r,0},c);}}

struct Building {float x,z,w,d,h;Color color;bool shop;};
struct Trash {float x,z;bool alive{true};};
struct NPC {float x,z,phase,speed;Color shirt;};
struct Car {float x,z,yaw,speed;Color color;bool traffic;};

class NativeGame {
 public:
  android_app* app{};
  EGLDisplay display{EGL_NO_DISPLAY}; EGLSurface surface{EGL_NO_SURFACE}; EGLContext context{EGL_NO_CONTEXT};
  GLuint program{},cityBuffer{},detailBuffer{},frameBuffer{};
  GLint posLoc{-1},colorLoc{-1},mvpLoc{-1},pointLoc{-1};
  int width{1},height{1};bool ready{},focused{true},room{};int insideBuilding{-1},drivenCar{-1};
  std::vector<Vertex> cityVerts,frameVerts,screenVerts;size_t worldFrameCount{};std::vector<Vertex> pointVerts;
  std::vector<Building> buildings;std::vector<Trash> trash;std::vector<NPC> npcs;std::vector<Car> cars;
  V3 player{0,0,-4};float playerYaw{},speed{},cameraYaw{},cameraPitch{.23f},walkPhase{},health{1.f},money{},playerY{},verticalVelocity{};
  float joyX{},joyY{};bool joyActive{},runHeld{},jumpRequested{},cameraHeld{},pendingEnter{};int joyId{-1},cameraId{-1};float lastTouchX{},lastTouchY{};
  bool keyForward{},keyBack{},keyLeft{},keyRight{},keyRun{};bool keyJump{};
  uint32_t rng{0x4d464131u};float lastTime{};float doorAnim{};
  V3 exteriorReturn{};

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
    const char*vs="attribute vec3 aPos; attribute vec4 aColor; uniform mat4 uMvp; uniform float uPoint; varying vec4 vColor; void main(){gl_Position=uMvp*vec4(aPos,1.0);gl_PointSize=uPoint;vColor=aColor;}";
    const char*fs="precision mediump float; varying vec4 vColor; void main(){gl_FragColor=vColor;}";
    GLuint a=shader(GL_VERTEX_SHADER,vs),b=shader(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,a);glAttachShader(program,b);glLinkProgram(program);glDeleteShader(a);glDeleteShader(b);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)return false;
    posLoc=glGetAttribLocation(program,"aPos");colorLoc=glGetAttribLocation(program,"aColor");mvpLoc=glGetUniformLocation(program,"uMvp");pointLoc=glGetUniformLocation(program,"uPoint");
    glGenBuffers(1,&cityBuffer);glGenBuffers(1,&detailBuffer);glGenBuffers(1,&frameBuffer);
    if(cityVerts.empty())buildWorld();
    upload(cityBuffer,cityVerts,GL_STATIC_DRAW);
    upload(detailBuffer,pointVerts,GL_STATIC_DRAW);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    ready=true;LOGI("Native MFA C++ renderer ready; points=%d",kDetailPoints);return true;
  }
  void upload(GLuint buffer,const std::vector<Vertex>&v,GLenum usage){glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(v.size()*sizeof(Vertex)),v.empty()?nullptr:v.data(),usage);}
  void loseGL(){ready=false;if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}display=EGL_NO_DISPLAY;context=EGL_NO_CONTEXT;surface=EGL_NO_SURFACE;}

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
      bool shop=((ix+iz)%3==0);float w=25+random()*8,d=25+random()*8,h=shop?7.5f:12+random()*36;
      Color c=shop?rndColor({.56f,.32f,.20f},{.83f,.68f,.43f}):rndColor({.49f,.48f,.44f},{.79f,.73f,.61f});
      buildings.push_back({x,z,w,d,h,c,shop});box(cityVerts,{x,h*.5f,z},{w,h,d},c);
      box(cityVerts,{x,h+.22f,z},{w+.5f,.44f,d+.5f},{.31f,.30f,.28f});
      const int rows=shop?1:std::max(2,static_cast<int>(h/5.0f));
      for(int side:{-1,1})for(int r=0;r<rows;r++)for(int col=0;col<4;col++){
        float wx=x+(col-1.5f)*w*.19f,wy=2.2f+r*(h-3.f)/rows;
        box(cityVerts,{wx,wy,z+side*(d*.5f+.035f)},{w*.10f,1.25f,.06f},random()<.52f?Color{.25f,.40f,.48f}:Color{.76f,.64f,.42f});
        box(cityVerts,{x+side*(w*.5f+.035f),wy,z+(col-1.5f)*d*.19f},{.06f,1.25f,d*.10f},{.28f,.41f,.47f});
      }
      // Door and colored shop canopy.

      if(shop)box(cityVerts,{x,4.25f,z+d*.5f+.14f},{w*.72f,.9f,.28f},rndColor({.54f,.10f,.07f},{.10f,.38f,.24f}));
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
      Car car{horizontal?along:p+lane,horizontal?p+lane:along,horizontal?(random()<.5f?0.f:kPi):kPi*.5f,0,rndColor({.12f,.21f,.34f},{.85f,.62f,.17f}),false};cars.push_back(car);
    }
    for(int i=0;i<12;i++){
      bool h=random()<.5f;int idx=static_cast<int>(random()*(kBlocks+1));float p=-half+idx*blockSize;float lane=random()<.5f?-4.f:4.f;
      cars.push_back({h?-half+random()*kCity:p+lane,h?p+lane:-half+random()*kCity,h?(random()<.5f?kPi*.5f:-kPi*.5f):(random()<.5f?0:kPi),5+random()*4,{.20f,.35f,.48f},true});
    }
    for(int i=0;i<28;i++){bool h=random()<.5f;float road=-half+static_cast<int>(random()*(kBlocks+1))*blockSize;float along=-half+random()*kCity;float side=random()<.5f?-10.2f:10.2f;npcs.push_back({h?along:road+side,h?road+side:along,random()*6.2f,1+random()*1.2f,rndColor({.15f,.25f,.40f},{.65f,.58f,.42f})});}
    for(int i=0;i<120;i++){
      float x,z;int attempts=0;do{x=-half+random()*kCity;z=-half+random()*kCity;attempts++;}while(attempts<100&&std::abs(x-std::round((x+half)/blockSize)*blockSize+half)>13&&std::abs(z-std::round((z+half)/blockSize)*blockSize+half)>13);
      trash.push_back({x,z,true});
    }
    // Exactly 120,000 visible surface detail points, one GL_POINTS draw call.
    pointVerts.reserve(kDetailPoints);std::array<Color,5> palette={Color{.40f,.39f,.36f},Color{.57f,.52f,.43f},Color{.64f,.59f,.49f},Color{.34f,.37f,.38f},Color{.72f,.65f,.52f}};
    int made=0,tries=0;while(made<kDetailPoints&&tries<kDetailPoints*8){tries++;float x=(random()*2-1)*half,z=(random()*2-1)*half;float gx=std::round((x+half)/blockSize)*blockSize-half,gz=std::round((z+half)/blockSize)*blockSize-half;float dx=std::abs(x-gx),dz=std::abs(z-gz);if(!(dx<=11||dz<=11))continue;Color c=palette[randomU()%palette.size()];float shade=.78f+random()*.35f;pointVerts.push_back({x,.055f+random()*.01f,z,c.r*shade,c.g*shade,c.b*shade,.85f});made++;}
    LOGI("World ready: buildings=%zu points=%zu",buildings.size(),pointVerts.size());
  }

  void drawMesh(GLuint buffer,const std::vector<Vertex>&v,const M4&m,GLenum primitive,float pointSize=1.f,size_t first=0,size_t count=static_cast<size_t>(-1)){if(v.empty()||first>=v.size())return;count=std::min(count,v.size()-first);glUseProgram(program);glBindBuffer(GL_ARRAY_BUFFER,buffer);glEnableVertexAttribArray(posLoc);glEnableVertexAttribArray(colorLoc);glVertexAttribPointer(posLoc,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(0));glVertexAttribPointer(colorLoc,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));glUniformMatrix4fv(mvpLoc,1,GL_FALSE,m.v);glUniform1f(pointLoc,pointSize);glDrawArrays(primitive,static_cast<GLint>(first),static_cast<GLsizei>(count));}
  bool collides(float x,float z){for(const auto&b:buildings){float r=.65f;if(x+r>b.x-b.w*.5f&&x-r<b.x+b.w*.5f&&z+r>b.z-b.d*.5f&&z-r<b.z+b.d*.5f)return true;}return false;}
  float distance(float x,float z,float a,float b){return std::sqrt((x-a)*(x-a)+(z-b)*(z-b));}
  int nearestCar(){int best=-1;float bd=3.5f;for(int i=0;i<static_cast<int>(cars.size());i++){if(cars[i].traffic)continue;float d=distance(player.x,player.z,cars[i].x,cars[i].z);if(d<bd){bd=d;best=i;}}return best;}
  int nearestDoor(){int best=-1;float bd=4.4f;for(int i=0;i<static_cast<int>(buildings.size());i++){const auto&b=buildings[i];float d=distance(player.x,player.z,b.x,b.z+b.d*.5f+1.8f);if(d<bd){bd=d;best=i;}}return best;}
  void interact(){
    if(room){room=false;player=exteriorReturn;insideBuilding=-1;doorAnim=0;return;}
    if(drivenCar>=0){Car&c=cars[drivenCar];player={c.x+std::cos(c.yaw)*2.f,0,c.z-std::sin(c.yaw)*2.f};drivenCar=-1;return;}
    int car=nearestCar();if(car>=0){drivenCar=car;cars[car].speed=0;return;}
    int b=nearestDoor();if(b>=0&&!pendingEnter){insideBuilding=b;const auto&shop=buildings[b];exteriorReturn={shop.x,0,shop.z+shop.d*.5f+3.f};doorAnim=0;pendingEnter=true;return;}
  }
  void inputEvent(AInputEvent*e){
    if(AInputEvent_getType(e)==AINPUT_EVENT_TYPE_KEY){int key=AKeyEvent_getKeyCode(e);bool down=AKeyEvent_getAction(e)==AKEY_EVENT_ACTION_DOWN;switch(key){case AKEYCODE_W:case AKEYCODE_DPAD_UP:keyForward=down;break;case AKEYCODE_S:case AKEYCODE_DPAD_DOWN:keyBack=down;break;case AKEYCODE_A:case AKEYCODE_DPAD_LEFT:keyLeft=down;break;case AKEYCODE_D:case AKEYCODE_DPAD_RIGHT:keyRight=down;break;case AKEYCODE_SHIFT_LEFT:case AKEYCODE_SHIFT_RIGHT:keyRun=down;break;case AKEYCODE_SPACE:keyJump=down;if(down)jumpRequested=true;break;case AKEYCODE_E:if(down)interact();break;case AKEYCODE_BACK:if(down){if(room)interact();else ANativeActivity_finish(app->activity);}break;}return;}
    if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION)return;
    int action=AMotionEvent_getAction(e),masked=action&AMOTION_EVENT_ACTION_MASK,index=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;int id=AMotionEvent_getPointerId(e,index);float x=AMotionEvent_getX(e,index),y=AMotionEvent_getY(e,index),w=std::max(1,width),h=std::max(1,height);
    if(masked==AMOTION_EVENT_ACTION_DOWN||masked==AMOTION_EVENT_ACTION_POINTER_DOWN){float jx=w*.18f,jy=h*.78f;if(x<w*.43f&&y>h*.52f){joyId=id;joyActive=true;joyX=(x-jx)/(w*.14f);joyY=(y-jy)/(h*.14f);}
      else if(x>w*.57f&&y>h*.53f){float runX=w*.67f,runY=h*.82f,jumpX=w*.84f,jumpY=h*.68f,useX=w*.87f,useY=h*.87f;if(distance(x,y,runX,runY)<std::min(w,h)*.12f)runHeld=true;else if(distance(x,y,jumpX,jumpY)<std::min(w,h)*.12f)jumpRequested=true;else if(distance(x,y,useX,useY)<std::min(w,h)*.12f)interact();}
      else{cameraId=id;cameraHeld=true;lastTouchX=x;lastTouchY=y;}return;}
    if(masked==AMOTION_EVENT_ACTION_MOVE){for(size_t p=0;p<AMotionEvent_getPointerCount(e);p++){int pid=AMotionEvent_getPointerId(e,p);float px=AMotionEvent_getX(e,p),py=AMotionEvent_getY(e,p);if(pid==joyId){joyX=std::clamp((px-w*.18f)/(w*.14f),-1.f,1.f);joyY=std::clamp((py-h*.78f)/(h*.14f),-1.f,1.f);}if(pid==cameraId){cameraYaw-=(px-lastTouchX)*.006f;cameraPitch=std::clamp(cameraPitch+(py-lastTouchY)*.004f,-.02f,.65f);lastTouchX=px;lastTouchY=py;}}return;}
    if(masked==AMOTION_EVENT_ACTION_UP||masked==AMOTION_EVENT_ACTION_POINTER_UP||masked==AMOTION_EVENT_ACTION_CANCEL){if(id==joyId){joyId=-1;joyActive=false;joyX=joyY=0;}if(id==cameraId){cameraId=-1;cameraHeld=false;}runHeld=false;}
  }
  static int inputThunk(android_app*a,AInputEvent*e){auto*g=static_cast<NativeGame*>(a->userData);if(g){g->inputEvent(e);return 1;}return 0;}
  static void commandThunk(android_app*a,int32_t cmd){auto*g=static_cast<NativeGame*>(a->userData);if(!g)return;switch(cmd){case APP_CMD_INIT_WINDOW:if(a->window&&!g->ready)g->initGL();break;case APP_CMD_TERM_WINDOW:if(g->ready)g->loseGL();break;case APP_CMD_GAINED_FOCUS:g->focused=true;break;case APP_CMD_LOST_FOCUS:g->focused=false;break;default:break;}}

  void update(float dt){
    dt=std::clamp(dt,0.f,.04f);if(pendingEnter){doorAnim=std::min(1.f,doorAnim+dt*3.4f);if(doorAnim>=1.f){pendingEnter=false;room=true;player={0,0,0};playerY=verticalVelocity=0;}return;}float fx=std::sin(cameraYaw),fz=std::cos(cameraYaw),rx=fz,rz=-fx;
    float forward=(keyForward?1.f:0.f)-(keyBack?1.f:0.f),side=(keyRight?1.f:0.f)-(keyLeft?1.f:0.f);if(joyActive){forward=-joyY;side=joyX;}float mx=fx*forward+rx*side,mz=fz*forward+rz*side;float ml=std::sqrt(mx*mx+mz*mz);if(ml>1){mx/=ml;mz/=ml;}
    bool sprint=keyRun||runHeld;float maxSpeed=sprint?8.8f:4.5f;
    if(drivenCar>=0){Car&c=cars[drivenCar];c.speed+=(forward*20.f-c.speed)*std::min(1.f,dt*1.6f);c.yaw+=side*dt*1.45f*std::clamp(std::abs(c.speed)/4.f,.25f,1.f);float nx=c.x+std::sin(c.yaw)*c.speed*dt,nz=c.z+std::cos(c.yaw)*c.speed*dt;if(!collides(nx,nz)){c.x=nx;c.z=nz;}else c.speed=0;player={c.x,0,c.z};}
    else{float blend=std::min(1.f,dt*9.f);speed+=(ml*maxSpeed-speed)*blend;float nx=player.x+mx*maxSpeed*dt,nz=player.z+mz*maxSpeed*dt;if(!collides(nx,player.z))player.x=nx;if(!collides(player.x,nz))player.z=nz;if(ml>.05f)playerYaw=std::atan2(mx,mz);if(jumpRequested&&playerY<=0.001f){verticalVelocity=6.8f;jumpRequested=false;}verticalVelocity-=17.f*dt;playerY+=verticalVelocity*dt;if(playerY<0){playerY=0;verticalVelocity=0;}walkPhase+=dt*(3+speed*1.4f);}
    if(room){player.x=std::clamp(player.x,-5.f,5.f);player.z=std::clamp(player.z,-5.f,5.f);}
    if(!room){float half=kCity*.5f-2;player.x=std::clamp(player.x,-half,half);player.z=std::clamp(player.z,-half,half);for(auto&t:trash)if(t.alive&&distance(player.x,player.z,t.x,t.z)<1.2f){t.alive=false;money+=5.f;}for(auto&n:npcs){n.phase+=dt*n.speed;n.x+=std::sin(n.phase)*dt*.45f;n.z+=std::cos(n.phase)*dt*.45f;if(std::abs(n.x)>half||std::abs(n.z)>half||collides(n.x,n.z)){n.phase+=kPi;n.x+=std::sin(n.phase)*dt;n.z+=std::cos(n.phase)*dt;}}
      for(int i=0;i<static_cast<int>(cars.size());i++){auto&c=cars[i];if(c.traffic&&i!=drivenCar){c.x+=std::sin(c.yaw)*c.speed*dt;c.z+=std::cos(c.yaw)*c.speed*dt;if(std::abs(c.x)>half||std::abs(c.z)>half){if(std::abs(std::sin(c.yaw))>.5f)c.x=-std::copysign(half-1,c.x);else c.z=-std::copysign(half-1,c.z);}}}}
  }

  void addPerson(std::vector<Vertex>&v,float x,float z,float phase,Color shirt,bool playerModel,float baseY=0){float a=playerModel?playerYaw:std::sin(phase)*.3f;float swing=std::sin(phase)*.35f;box(v,{x,baseY+.92f,z},{.55f,.9f,.36f},shirt,a);box(v,{x,baseY+1.58f,z},{.32f,.38f,.32f},{.68f,.47f,.33f},a);box(v,{x,baseY+1.81f,z},{.4f,.12f,.42f},{.88f,.87f,.81f},a);box(v,{x-.22f,baseY+.92f,z},{.13f,.65f,.14f},{.88f,.86f,.79f},a-swing);box(v,{x+.22f,baseY+.92f,z},{.13f,.65f,.14f},{.88f,.86f,.79f},a+swing);box(v,{x-.13f,baseY+.27f+std::abs(swing)*.12f,z},{.18f,.52f,.2f},{.18f,.18f,.17f},a+swing);box(v,{x+.13f,baseY+.27f+std::abs(swing)*.12f,z},{.18f,.52f,.2f},{.18f,.18f,.17f},a-swing);}
  void addCar(std::vector<Vertex>&v,const Car&c,bool playerCar=false){box(v,{c.x,.48f,c.z},{1.9f,.72f,4.1f},c.color,c.yaw);box(v,{c.x,.96f,c.z-.12f},{1.55f,.65f,2.05f},{.16f,.25f,.31f},c.yaw);for(int side:{-1,1})for(int f:{-1,1})box(v,{c.x+side*.95f,.27f,c.z+f*1.28f},{.24f,.48f,.73f},{.08f,.08f,.08f},c.yaw);}
  void addRoom(std::vector<Vertex>&v){box(v,{0,-.1f,0},{14,.2f,14},{.60f,.53f,.42f});box(v,{0,2.2f,7},{14,4.6f,.25f},{.77f,.70f,.57f});box(v,{-7,2.2f,0},{.25f,4.6f,14},{.77f,.70f,.57f});box(v,{7,2.2f,0},{.25f,4.6f,14},{.77f,.70f,.57f});box(v,{0,4.55f,0},{14,.18f,14},{.55f,.49f,.40f});for(int i=0;i<4;i++){box(v,{-4.5f+i*3,1.1f,-2.6f},{2.1f,2.2f,.75f},{.32f,.22f,.13f});box(v,{-4.5f+i*3,1.9f,-2.6f},{2.1f,.18f,.82f},{.73f,.43f,.13f});}box(v,{0,1.1f,4.8f},{3.4f,2.2f,.8f},{.29f,.21f,.15f});}

  void addGlyph(std::vector<Vertex>&v,char c,float x,float y,float scale,Color col){
    static const uint8_t glyphs[36][5]={
      {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},{7,4,6,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},{5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},{7,5,7,4,4},{7,5,5,7,1},{7,5,7,5,5},{7,4,7,1,7},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},
      {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    int index=-1;if(c>='A'&&c<='Z')index=c-'A';else if(c>='0'&&c<='9')index=26+c-'0';if(index<0)return;
    for(int row=0;row<5;row++)for(int coln=0;coln<3;coln++)if(glyphs[index][row]&(1<<(2-coln)))rect2d(v,x+coln*scale,y+(4-row)*scale,scale*.82f,scale*.82f,col);
  }
  void text(std::vector<Vertex>&v,const char*s,float x,float y,float scale,Color col){for(;*s;s++){addGlyph(v,*s,x,y,scale,col);x+=scale*4;}}
  void makeFrame(){
    frameVerts.clear();if(room)addRoom(frameVerts);
    if(drivenCar<0){addPerson(frameVerts,player.x,player.z,walkPhase,{.88f,.87f,.80f},true,playerY);}
    for(const auto&n:npcs)addPerson(frameVerts,n.x,n.z,n.phase,n.shirt,false);
    for(const auto&c:cars)addCar(frameVerts,c,drivenCar>=0);
    if(drivenCar>=0){for(int i=0;i<static_cast<int>(cars.size());i++)if(i==drivenCar){}}
    if(!room){for(const auto&t:trash)if(t.alive)box(frameVerts,{t.x,.14f,t.z},{.18f,.24f,.18f},{.55f,.73f,.32f});for(int i=0;i<static_cast<int>(buildings.size());i++){const auto&b=buildings[i];if(distance(player.x,player.z,b.x,b.z+b.d*.5f)<26){float angle=(pendingEnter&&insideBuilding==i)?-doorAnim*1.45f:0.f;box(frameVerts,{b.x,1.15f,b.z+b.d*.5f+.12f},{2.1f,2.3f,.16f},{.30f,.20f,.12f},angle);}}}
    worldFrameCount=frameVerts.size();
    // On-screen touch controls. The player moves immediately; there is no title/menu gate.
    float sx=static_cast<float>(width),sy=static_cast<float>(height);auto nx=[&](float p){return p/sx*2-1;};auto ny=[&](float p){return 1-p/sy*2;};float radius=.21f;
    circle2d(frameVerts,nx(sx*.18f),ny(sy*.78f),radius,{.18f,.22f,.25f});circle2d(frameVerts,nx(sx*.18f),ny(sy*.78f),radius*.78f,{.32f,.35f,.36f});circle2d(frameVerts,nx(sx*(.18f+joyX*.09f)),ny(sy*(.78f+joyY*.09f)),radius*.36f,{.84f,.69f,.23f});
    circle2d(frameVerts,nx(sx*.67f),ny(sy*.82f),radius*.62f,{.85f,.37f,.14f});circle2d(frameVerts,nx(sx*.84f),ny(sy*.68f),radius*.62f,{.87f,.70f,.18f});circle2d(frameVerts,nx(sx*.87f),ny(sy*.87f),radius*.62f,{.15f,.62f,.48f});
    rect2d(frameVerts,-.96f,.84f,.31f,.07f,{.12f,.14f,.15f});rect2d(frameVerts,-.95f,.85f,.29f*health,.05f,{.75f,.16f,.15f});rect2d(frameVerts,-.96f,.72f,.31f,.025f,{.12f,.14f,.15f});rect2d(frameVerts,-.96f,.72f,std::min(.31f,money*.0004f),.025f,{.85f,.68f,.20f});
    text(frameVerts,"MFA 1",-.94f,.91f,.018f,{.98f,.86f,.42f});text(frameVerts,"MOVE",nx(sx*.11f),ny(sy*.64f),.012f,{.9f,.91f,.89f});text(frameVerts,"RUN",nx(sx*.63f),ny(sy*.72f),.012f,{1,.95f,.86f});text(frameVerts,"JUMP",nx(sx*.79f),ny(sy*.57f),.012f,{1,.95f,.86f});text(frameVerts,"USE",nx(sx*.84f),ny(sy*.75f),.012f,{1,.95f,.86f});
  }
  void render(){if(!ready||!app->window)return;eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);glViewport(0,0,width,height);glClearColor(.43f,.66f,.80f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    float aspect=static_cast<float>(width)/std::max(1,height);V3 target={player.x,1.05f,player.z};float horiz=10.f*std::cos(cameraPitch);V3 eye={player.x-std::sin(cameraYaw)*horiz,player.y+2.8f+10.f*std::sin(cameraPitch),player.z-std::cos(cameraYaw)*horiz};M4 vp=multiply(perspective(62*kPi/180.f,aspect,.1f,1200.f),lookAt(eye,target,{0,1,0}));
    if(!room){drawMesh(cityBuffer,cityVerts,vp,GL_TRIANGLES);drawMesh(detailBuffer,pointVerts,vp,GL_POINTS,2.1f);}
    makeFrame();upload(frameBuffer,frameVerts,GL_DYNAMIC_DRAW);
    if(worldFrameCount>0)drawMesh(frameBuffer,frameVerts,vp,GL_TRIANGLES,1.f,0,worldFrameCount);
    glDisable(GL_DEPTH_TEST);drawMesh(frameBuffer,frameVerts,identity(),GL_TRIANGLES,1.f,worldFrameCount,frameVerts.size()-worldFrameCount);glEnable(GL_DEPTH_TEST);
    eglSwapBuffers(display,surface);
  }
};

static void androidCommand(android_app*a,int32_t cmd){auto*g=static_cast<NativeGame*>(a->userData);if(g)NativeGame::commandThunk(a,cmd);}
static int androidInput(android_app*a,AInputEvent*e){auto*g=static_cast<NativeGame*>(a->userData);return g?([&](){g->inputEvent(e);return 1;})():0;}
}

extern "C" void android_main(android_app* app){app_dummy();NativeGame game;game.app=app;app->userData=&game;app->onAppCmd=androidCommand;app->onInputEvent=androidInput;auto previous=std::chrono::steady_clock::now();while(!app->destroyRequested){int events=0;android_poll_source*source=nullptr;int timeout=(game.ready&&game.focused)?0:-1;int ident=ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source));if(ident>=0&&source)source->process(app,source);if(app->destroyRequested)break;if(game.ready&&game.focused){auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-previous).count();previous=now;game.update(dt);game.render();}}game.loseGL();}
