#include "engine.hpp"
#include <functional>
#include <stdexcept>

// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename LevelMap, typename Condition>
uint32_t process_orders(Orderbook &orderbook, const Order &order, QuantityType &q, OrderMap &ordersMap, LevelMap &levelsMap, const Condition cond) {
  uint32_t matchCount = 0;
  auto it = ordersMap.begin();

  while (it != ordersMap.end() && q > 0 &&
         (*it == order.price || cond(*it, order.price))) {

    auto &ordersAtPrice = levelsMap[*it].orders;
    for (auto idIt = ordersAtPrice.begin();
         idIt != ordersAtPrice.end() && q > 0;) {

      if (!orderbook.orders[*idIt].has_value()) {
        idIt = ordersAtPrice.erase(idIt);
        continue;
      }
      auto &orderIt = orderbook.orders[*idIt].value();
      QuantityType trade = std::min(q, orderIt.quantity);
      q -= trade;
      orderIt.quantity -= trade;
      levelsMap[orderIt.price].volume -= trade;
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

  if (QuantityType q = incoming.quantity; incoming.side == Side::BUY) {
    // For a BUY, match with sell orders priced at or below the order's price.
    if (*orderbook.sellOrders.begin() <= incoming.price) matchCount = process_orders(orderbook, incoming, q, orderbook.sellOrders, orderbook.sellLevels, std::less<>());
    if (q > 0) {
      orderbook.buyOrders.insert(incoming.price);
      orderbook.buyLevels[incoming.price].orders.push_back(incoming.id);
      orderbook.buyLevels[incoming.price].volume += incoming.quantity;
      orderbook.orders[incoming.id] = incoming;
      orderbook.orders[incoming.id].value().quantity = q;
    }
  } else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    if (*orderbook.buyOrders.begin() >= incoming.price) matchCount = process_orders(orderbook, incoming, q, orderbook.buyOrders, orderbook.buyLevels, std::greater<>());
    if (q > 0) {
      orderbook.sellOrders.insert(incoming.price);
      orderbook.sellLevels[incoming.price].orders.push_back(incoming.id);
      orderbook.sellLevels[incoming.price].volume += incoming.quantity;
      orderbook.orders[incoming.id] = incoming;
      orderbook.orders[incoming.id].value().quantity = q;
    }
  }
  return matchCount;
}

template <typename Orders, typename LevelMap>
bool modify_order_in_map(Orders &orders, Side side, LevelMap &levelsMap, IdType order_id,
                         QuantityType new_quantity) {
  auto &order = orders[order_id];
  if (!order.has_value()) return false;
  if (order.value().side != side) return false;

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

std::optional<Order> lookup_order_in_map(const Orderbook &orderbook, const IdType order_id) {
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
