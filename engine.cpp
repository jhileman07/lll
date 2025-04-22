#include "engine.hpp"
#include <functional>
#include <stdexcept>
#include <iostream>

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
    uint16_t (ChunkedBitset::*next)(uint16_t) const,
    Comp comp
) {
    uint32_t matchCount = 0;

    for (; q > 0 && comp(p, order.price); bits.clear(p), p = (bits.*next)(p)) {
        auto &level = levelsMap[p];

        for (size_t i = level.begin; q > 0 && i < level.orders.size(); ++i) {
            auto id = level.orders[i];
            auto &maybeOrder = ob.orders[id];
            if (!maybeOrder.has_value()) {
                ++level.begin;
                break;
            }

            auto &orderIt = maybeOrder.value();
            QuantityType trade = std::min(q, orderIt.quantity);
            q -= trade;
            orderIt.quantity -= trade;
            level.volume -= trade;
            ++matchCount;

            if (orderIt.quantity == 0) {
                maybeOrder = std::nullopt;
                ++level.begin;
            }
        }

        if (q == 0) return matchCount;
    }

    return matchCount;
}

// Top-level match function
uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
    uint32_t matchCount = 0;
    QuantityType q = incoming.quantity;

    switch (incoming.side) {
        case Side::BUY:
            if (orderbook.ba <= incoming.price) {
                matchCount = process_order(
                    orderbook,
                    incoming,
                    q,
                    orderbook.sellLevels,
                    orderbook.sellBits,
                    orderbook.ba,
                    &ChunkedBitset::find_next,
                    std::less_equal<>()
                );
            }

            if (q > 0 && !orderbook.buyFlag[incoming.price]) {
                auto &level = orderbook.buyLevels[incoming.price];
                orderbook.buyBits.set(incoming.price);
                orderbook.bb = std::max(orderbook.bb, incoming.price);

                level.orders.emplace_back(incoming.id);
                if (level.orders.size() == 30) {
                    orderbook.buyFlag[incoming.price] = true;
                }

                level.volume += q;
                orderbook.orders[incoming.id] = {.id = incoming.id, .price = incoming.price, .quantity = q, .side = Side::BUY};
            }
            break;

        case Side::SELL: default:
            if (orderbook.bb >= incoming.price) {
                matchCount = process_order(
                    orderbook,
                    incoming,
                    q,
                    orderbook.buyLevels,
                    orderbook.buyBits,
                    orderbook.bb,
                    &ChunkedBitset::find_prev,
                    std::greater_equal<>()
                );
            }

            if (q > 0) {
                auto &level = orderbook.sellLevels[incoming.price];
                orderbook.sellBits.set(incoming.price);
                orderbook.ba = std::min(orderbook.ba, incoming.price);

                level.orders.emplace_back(incoming.id);
                if (level.orders.size() == 30) {
                    orderbook.sellFlag[incoming.price] = true;
                }

                level.volume += q;
                orderbook.orders[incoming.id] = {.id = incoming.id, .price = incoming.price, .quantity = q, .side = Side::SELL};
            }
    }

    return matchCount;
}

template <typename Orders, typename LevelMap>
inline __attribute__((always_inline, hot)) bool modify_order_in_map(Orders &orders, Side side, LevelMap &levelsMap,
                                                                     IdType order_id, QuantityType new_quantity) {
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

void modify_order_by_id(Orderbook &orderbook, IdType order_id, QuantityType new_quantity) {
    if (modify_order_in_map(orderbook.orders, Side::BUY, orderbook.buyLevels, order_id, new_quantity)) return;
    modify_order_in_map(orderbook.orders, Side::SELL, orderbook.sellLevels, order_id, new_quantity);
}

inline __attribute__((always_inline, hot)) std::optional<Order> lookup_order_in_map(const Orderbook &orderbook, const IdType order_id) {
    return orderbook.orders[order_id];
}

uint32_t get_volume_at_level(Orderbook &orderbook, Side side, PriceType quantity) {
    switch (side) {
        case Side::BUY:
            return orderbook.buyLevels[quantity].volume;
        case Side::SELL: default:
            return orderbook.sellLevels[quantity].volume;
    }
}

Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
    const auto order1 = lookup_order_in_map(orderbook, order_id);
    const auto order2 = lookup_order_in_map(orderbook, order_id);
    if (order1.has_value()) return *order1;
    if (order2.has_value()) return *order2;
    throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook &orderbook, IdType order_id) {
    const auto order = orderbook.orders[order_id];
    return order.has_value() && order.value().quantity > 0;
}

Orderbook *create_orderbook() {
    Orderbook *ob = new Orderbook;
    for (PriceType price = 0; price < PRICE_MAX; ++price) {
        ob->buyLevels[price].orders.reserve(MAX);

        ob->sellLevels[price].orders.reserve(MAX);
    }
    return ob;
}