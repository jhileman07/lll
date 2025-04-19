#include "engine.hpp"
#include <functional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename VolumeMap, typename Condition>
uint32_t process_orders(Orderbook &orderbook, Order &order, OrderMap &ordersMap, VolumeMap &volumeMap, const Condition cond) {
  uint32_t matchCount = 0;
  auto it = ordersMap.begin();

  while (it != ordersMap.end() && order.quantity > 0 &&
         (it->first == order.price || cond(it->first, order.price))) {

    auto &ordersAtPrice = it->second;
    for (auto idIt = ordersAtPrice.begin();
         idIt != ordersAtPrice.end() && order.quantity > 0;) {

      if (!orderbook.orders[*idIt].has_value()) {
        idIt = ordersAtPrice.erase(idIt);
        continue;
      }
      auto &orderIt = orderbook.orders[*idIt].value();
      QuantityType trade = std::min(order.quantity, orderIt.quantity);
      order.quantity -= trade;
      orderIt.quantity -= trade;
      volumeMap[orderIt.price] -= trade;
      ++matchCount;

      if (orderIt.quantity == 0) {
        orderbook.orders[*idIt] = std::nullopt;
        idIt = ordersAtPrice.erase(idIt);
      }
      else
        ++idIt;
    }

    if (ordersAtPrice.empty())
      it = ordersMap.erase(it);
    else
      ++it;
  }

  return matchCount;
}

// OG FUNCTION
uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
  uint32_t matchCount = 0;

  if (Order order = incoming; order.side == Side::BUY) {
    // For a BUY, match with sell orders priced at or below the order's price.
    matchCount = process_orders(orderbook, order, orderbook.sellOrders, orderbook.sellVolume, std::less<>());
    if (order.quantity > 0) {
      orderbook.buyOrders[order.price].push_back(order.id);
      orderbook.buyVolume[order.price] += order.quantity;
      orderbook.orders[order.id] = order;
    }
  } else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    matchCount = process_orders(orderbook, order, orderbook.buyOrders, orderbook.buyVolume, std::greater<>());
    if (order.quantity > 0) {
      orderbook.sellOrders[order.price].push_back(order.id);
      orderbook.sellVolume[order.price] += order.quantity;
      orderbook.orders[order.id] = order;
    }
  }
  return matchCount;
}

template <typename Orders, typename VolumeMap>
bool modify_order_in_map(Orders &orders, Side side, VolumeMap &volumeMap, IdType order_id,
                         QuantityType new_quantity) {
  auto &order = orders[order_id];
  if (!order.has_value()) return false;
  if (order.value().side != side) return false;

  volumeMap[order.value().price] += new_quantity - order.value().quantity;
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
  if (modify_order_in_map(orderbook.orders, Side::BUY, orderbook.buyVolume, order_id, new_quantity))
    return;
  modify_order_in_map(orderbook.orders, Side::SELL, orderbook.sellVolume, order_id, new_quantity);
}

std::optional<Order> lookup_order_in_map(const Orderbook &orderbook, const IdType order_id) {
  return orderbook.orders[order_id];
}

// OG FUNCTION
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity) {
  switch (side) {
    case Side::BUY:
      return orderbook.buyVolume[quantity];
    case Side::SELL: default:
      return orderbook.sellVolume[quantity];
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
Orderbook *create_orderbook() { return new Orderbook; }
