#include "engine.hpp"
#include <functional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename Condition>
uint32_t process_orders(Orderbook &orderbook, Order &order, OrderMap &ordersMap, const Condition cond) {
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
    matchCount = process_orders(orderbook, order, orderbook.sellOrders, std::less<>());
    if (order.quantity > 0) {
      orderbook.buyOrders[order.price].push_back(order.id);
      orderbook.orders[order.id] = order;
    }
  } else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    matchCount = process_orders(orderbook, order, orderbook.buyOrders, std::greater<>());
    if (order.quantity > 0) {
      orderbook.sellOrders[order.price].push_back(order.id);
      orderbook.orders[order.id] = order;
    }
  }
  return matchCount;
}

// OG FUNCTION
void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {
  if (!orderbook.orders[order_id].has_value()) return;
  if (new_quantity == 0) {
    orderbook.orders[order_id] = std::nullopt;
    return;
  }
  orderbook.orders[order_id].value().quantity = new_quantity;
}

template <typename OrderMap>
std::optional<Order> lookup_order_in_map(const Orderbook &orderbook, const OrderMap &ordersMap, const IdType order_id) {
  for (const auto &[price, idList] : ordersMap) {
    for (const auto &id : idList) {
      if (id == order_id) {
        return orderbook.orders[id];
      }
    }
  }
  return std::nullopt;
}

// OG FUNCTION
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity) {
  uint32_t total = 0;
  if (side == Side::BUY) {
    const auto buy_orders = orderbook.buyOrders.find(quantity);
    if (buy_orders == orderbook.buyOrders.end()) {
      return 0;
    }
    for (const auto &id : buy_orders->second) {
      const auto order = orderbook.orders[id];
      if (!order.has_value()) continue;
      total += order.value().quantity;
    }
  } else if (side == Side::SELL) {
    const auto sell_orders = orderbook.sellOrders.find(quantity);
    if (sell_orders == orderbook.sellOrders.end()) {
      return 0;
    }
    for (const auto &id : sell_orders->second) {
      const auto order = orderbook.orders[id];
      if (!order.has_value()) continue;
      total += order.value().quantity;
    }
  }
  return total;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
// OG FUNCTION
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
  const auto order1 = lookup_order_in_map(orderbook, orderbook.buyOrders, order_id);
  const auto order2 = lookup_order_in_map(orderbook, orderbook.sellOrders, order_id);
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
