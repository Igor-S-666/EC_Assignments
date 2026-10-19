/**
 * @brief This files contain structures and types used in lab_3.
 */

#pragma once
#include <functional>
#include <utility>
#include <vector>
#include <array>
#include "../../types.hpp"

using neighbour_order_t = std::array<int, 3>;
using neighbour_orders_t = std::vector<neighbour_order_t>;
using cost_function = std::function<int(const distance_matrix_t&, int, int, const std::vector<int>&)>;
using exchange_function = std::function<void(std::vector<int>&, int, int)>;