#include "engine.hpp"
#include <functional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename LevelMap, typename Comp>
inline __attribute__((always_inline, hot)) uint32_t process_order(
    Orderbook &ob,
    const Order &order,
    QuantityType &q,
    LevelMap &levelsMap,
    ChunkedBitset &bits,
    PriceType &p,
    int (ChunkedBitset::*next)(int) const,
    Comp comp
) {
  uint32_t matchCount = 0;

  while (q > 0 && (p == order.price || comp(p, order.price))) {
    auto &level = levelsMap[p];

    auto &ordersAtPrice = level.orders;
    for (auto idIt = ordersAtPrice.begin(); idIt != ordersAtPrice.end() && q > 0;) {
      auto &maybeOrder = ob.orders[*idIt];
      if (!maybeOrder.has_value()) {
        idIt = ordersAtPrice.erase(idIt);
        continue;
      }

      auto &orderIt = maybeOrder.value();
      QuantityType trade = std::min(q, orderIt.quantity);
      q -= trade;
      orderIt.quantity -= trade;
      level.volume -= trade;
      ++matchCount;

      if (orderIt.quantity == 0) {
        maybeOrder = std::nullopt;
        idIt = ordersAtPrice.erase(idIt);
      } else {
        ++idIt;
      }
    }

    if (ordersAtPrice.empty()) {
      bits.clear(p);
      p = (bits.*next)(p);
    }
  }

  return matchCount;
}

// OG FUNCTION
uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
  uint32_t matchCount = 0;
  QuantityType q = incoming.quantity;
  switch (incoming.side) {
    case Side::BUY:
      // For a BUY, match with sell orders priced at or below the order's price.
        if (orderbook.ba <= incoming.price) matchCount = process_order(
                                                              orderbook,
                                                              incoming,
                                                              q,
                                                              orderbook.sellLevels,
                                                              orderbook.sellBits,
                                                              orderbook.ba,
                                                              &ChunkedBitset::find_next,
                                                              std::less<>()
                                                          );
      if (q > 0) {
        // orderbook.buyOrders.insert(incoming.price);
        orderbook.buyBits.set(incoming.price);
        orderbook.bb = std::max(orderbook.bb, incoming.price);
        orderbook.buyLevels[incoming.price].orders.emplace_back(incoming.id);
        orderbook.buyLevels[incoming.price].volume += q;
        orderbook.orders[incoming.id] = {.id = incoming.id, .price = incoming.price, .quantity = q, .side = Side::BUY};
      }
      break;
    case Side::SELL: default:
      // For a SELL, match with buy orders priced at or above the order's price.
    if (orderbook.bb >= incoming.price) matchCount = process_order(
                                                          orderbook,
                                                          incoming,
                                                          q,
                                                          orderbook.buyLevels,
                                                          orderbook.buyBits,
                                                          orderbook.bb,
                                                          &ChunkedBitset::find_prev,
                                                          std::greater<>()
                                                      );
      if (q > 0) {
        // orderbook.sellOrders.insert(incoming.price);
        orderbook.sellBits.set(incoming.price);
        orderbook.ba = std::min(orderbook.ba, incoming.price);
        orderbook.sellLevels[incoming.price].orders.emplace_back(incoming.id);
        orderbook.sellLevels[incoming.price].volume += q;
        orderbook.orders[incoming.id] = {.id = incoming.id, .price = incoming.price, .quantity = q, .side = Side::SELL};
      }
  }
  return matchCount;
}

template <typename Orders, typename LevelMap>
inline __attribute__((always_inline, hot)) bool modify_order_in_map(Orders &orders, Side side, LevelMap &levelsMap, IdType order_id,
                         QuantityType new_quantity) {
  auto &order = orders[order_id];
  if (!order.has_value() || order.value().side != side) return false;

  levelsMap[order.value().price].volume += new_quantity - order.value().quantity;
  if (new_quantity == 0) {
    orders[order_id] = std::nullopt;
    return true;
  }
  order.value().quantity = new_quantity;
  return true;
}

// OG FUNCTION
void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {
  if (modify_order_in_map(orderbook.orders, Side::BUY, orderbook.buyLevels, order_id, new_quantity))
    return;
  modify_order_in_map(orderbook.orders, Side::SELL, orderbook.sellLevels, order_id, new_quantity);
}

inline __attribute__((always_inline, hot)) std::optional<Order> lookup_order_in_map(const Orderbook &orderbook, const IdType order_id) {
  return orderbook.orders[order_id];
}

// OG FUNCTION
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity) {
  switch (side) {
    case Side::BUY:
      return orderbook.buyLevels[quantity].volume;
    case Side::SELL: default:
      return orderbook.sellLevels[quantity].volume;
  }
}

// Functions below here don't need to be performant. Just make sure they're
// correct
// OG FUNCTION
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
  const auto order1 = lookup_order_in_map(orderbook, order_id);
  const auto order2 = lookup_order_in_map(orderbook, order_id);
  if (order1.has_value())
    return *order1;
  if (order2.has_value())
    return *order2;
  throw std::runtime_error("Order not found");
}

// OG FUNCTION
bool order_exists(Orderbook &orderbook, IdType order_id) {
  const auto order = orderbook.orders[order_id];
  return order.has_value() && order.value().quantity > 0;
}

// OG FUNCTION
Orderbook *create_orderbook() {
  Orderbook *ob = new Orderbook;
  for (PriceType price = 0; price < PRICE_MAX; ++price) {
    ob->buyLevels[price].orders.reserve(MAX);
    ob->sellLevels[price].orders.reserve(MAX);
  }
  return ob;
}
