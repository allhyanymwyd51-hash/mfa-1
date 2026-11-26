#include "game_core.hpp"
#include <algorithm>
#include <cmath>

namespace mfa {
TickResult GameCore::tick(float dt, PlayerState& p, Vec3 input, bool sprint) noexcept {
  dt = std::clamp(dt, 0.0f, 0.05f);
  const float max_speed = sprint ? 11.0f : 5.2f;
  const float length = std::sqrt(input.x * input.x + input.z * input.z);
  if (length > 1.0f) { input.x /= length; input.z /= length; }
  const float blend = std::min(1.0f, 8.0f * dt);
  p.velocity.x += (input.x * max_speed - p.velocity.x) * blend;
  p.velocity.z += (input.z * max_speed - p.velocity.z) * blend;
  p.position.x = std::clamp(p.position.x + p.velocity.x * dt, -320.0f, 320.0f);
  p.position.z = std::clamp(p.position.z + p.velocity.z * dt, -320.0f, 320.0f);
  p.hunger = std::max(0.0f, p.hunger - dt * 0.18f);
  city_time_ += dt;
  return {++frame_, city_time_};
}
}
