#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include <set>

enum class Side : uint8_t { BUY, SELL };

using IdType = uint32_t;
using PriceType = uint16_t;
using QuantityType = uint16_t;

// You CANNOT change this
struct Order {
  IdType id; // Unique
  PriceType price;
  QuantityType quantity;
  Side side;
};

struct Level {
    std::vector<IdType> orders;
    uint32_t volume = 0;
};

constexpr uint16_t MAX = 10'000;
constexpr uint16_t PRICE_MAX = 4500;


// You CAN and SHOULD change this
struct Orderbook {
    std::set<PriceType, std::greater<>> buyOrders;
    std::array<Level, PRICE_MAX> buyLevels;

    std::set<PriceType> sellOrders;
    std::array<Level, PRICE_MAX> sellLevels;

    std::array<std::optional<Order>,MAX> orders;
};

extern "C" {

// Takes in an incoming order, matches it, and returns the number of matches
// Partial fills are valid

uint32_t match_order(Orderbook &orderbook, const Order &incoming);

// Sets the new quantity of an order. If new_quantity==0, removes the order
void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity);

// Returns total resting volume at a given price point
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity);

// Performance of these do not matter. They are only used to check correctness
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id);
bool order_exists(Orderbook &orderbook, IdType order_id);
Orderbook *create_orderbook();
}
