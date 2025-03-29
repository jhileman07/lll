#include "engine.hpp"
#include <functional>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename Condition>
uint32_t process_orders(Order &order, OrderMap &ordersMap, Condition cond) {
  uint32_t matchCount = 0;
  auto it = ordersMap.begin();
  while (it != ordersMap.end() && order.quantity > 0 &&
         (it->first == order.price || cond(it->first, order.price))) {
    auto &ordersAtPrice = it->second;
    for (auto orderIt = ordersAtPrice.begin();
         orderIt != ordersAtPrice.end() && order.quantity > 0;) {
      QuantityType trade = std::min(order.quantity, orderIt->quantity);
      order.quantity -= trade;
      orderIt->quantity -= trade;
      ++matchCount;
      if (orderIt->quantity == 0)
        orderIt = ordersAtPrice.erase(orderIt);
      else
        ++orderIt;
    }
    if (ordersAtPrice.empty())
      it = ordersMap.erase(it);
    else
      ++it;
  }
  return matchCount;
}

uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
  uint32_t matchCount = 0;
  Order order = incoming; // Create a copy to modify the quantity

  if (order.side == Side::BUY) {
    // For a BUY, match with sell orders priced at or below the order's price.
    matchCount = process_orders(order, orderbook.sellOrders, std::less<>());
    if (order.quantity > 0)
      orderbook.buyOrders[order.price].push_back(order);
  } else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    matchCount = process_orders(order, orderbook.buyOrders, std::greater<>());
    if (order.quantity > 0)
      orderbook.sellOrders[order.price].push_back(order);
  }
  return matchCount;
}

// Templated helper to cancel an order within a given orders map.
template <typename OrderMap>
bool modify_order_in_map(OrderMap &ordersMap, IdType order_id,
                         QuantityType new_quantity) {
  for (auto it = ordersMap.begin(); it != ordersMap.end();) {
    auto &orderList = it->second;
    for (auto orderIt = orderList.begin(); orderIt != orderList.end();) {
      if (orderIt->id == order_id) {
        if (new_quantity == 0)
          orderIt = orderList.erase(orderIt);
        else {
          orderIt->quantity = new_quantity;
          return true;
        }
      } else {
        ++orderIt;
      }
    }
    if (orderList.empty())
      it = ordersMap.erase(it);
    else
      ++it;
  }
  return false;
}

void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {
  if (modify_order_in_map(orderbook.buyOrders, order_id, new_quantity))
    return;
  if (modify_order_in_map(orderbook.sellOrders, order_id, new_quantity))
    return;
}

template <typename OrderMap>
std::optional<Order> lookup_order_in_map(OrderMap &ordersMap, IdType order_id) {
  for (const auto &[price, orderList] : ordersMap) {
    for (const auto &order : orderList) {
      if (order.id == order_id) {
        return order;
      }
    }
  }
  return std::nullopt;
}

std::optional<Order> lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
  auto order1 = lookup_order_in_map(orderbook.buyOrders, order_id);
  auto order2 = lookup_order_in_map(orderbook.sellOrders, order_id);
  if (order1.has_value())
    return order1;
  if (order2.has_value())
    return order2;
  return std::nullopt;
}
