#pragma once
#include <cstdint>

namespace mfa {
struct Vec3 { float x{}, y{}, z{}; };
struct PlayerState { Vec3 position{}, velocity{}; float health{100.f}; float hunger{100.f}; };
struct TickResult { std::uint32_t frame{}; float city_time{}; };

// Native C++ gameplay kernel. The browser build uses the JS simulation until
// this library is linked through Android NDK/JNI in the packaged app.
class GameCore {
 public:
  TickResult tick(float dt, PlayerState& player, Vec3 input, bool sprint) noexcept;
  static constexpr std::uint32_t kWorldSeed = 0x4D464131u;
 private:
  std::uint32_t frame_{};
  float city_time_{};
};
}
