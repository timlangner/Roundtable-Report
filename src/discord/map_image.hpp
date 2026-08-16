#pragma once

#include "run/snapshot.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace erstats {

std::optional<std::vector<uint8_t>> render_location_map(const RunSnapshot& snapshot);

}  // namespace erstats
