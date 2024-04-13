#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <whirlwind/common/assert.hpp>
#include <whirlwind/common/namespace.hpp>
#include <whirlwind/common/numeric.hpp>
#include <whirlwind/common/stddef.hpp>

#include "unit_capacity.hpp"

WHIRLWIND_NAMESPACE_BEGIN

template<class Graph,
         class Cost,
         class Flow,
         template<class> class Container = std::vector,
         class Mixin = UnitCapacityMixin<Graph, Flow, Container>>
class Network : public Mixin {
private:
    using super_type = Mixin;

public:
    using graph_type = Graph;
    using node_type = typename super_type::node_type;
    using arc_type = typename super_type::arc_type;
    using cost_type = Cost;
    using flow_type = Flow;
    using size_type = Size;

    template<class T>
    using container_type = Container<T>;

    using super_type::contains_arc;
    using super_type::contains_node;
    using super_type::get_arc_id;
    using super_type::get_node_id;
    using super_type::nodes;
    using super_type::num_arcs;
    using super_type::num_nodes;

    constexpr Network(const graph_type& graph,
                      container_type<flow_type> surplus,
                      container_type<cost_type> cost)
        : super_type(graph),
          node_excess_(std::move(surplus)),
          node_potential_(num_nodes(), zero<cost_type>()),
          arc_cost_(std::move(cost))
    {
        WHIRLWIND_ASSERT(std::size(node_excess_) == num_nodes());
        WHIRLWIND_ASSERT(std::size(arc_cost_) == num_arcs());
        WHIRLWIND_DEBUG_ASSERT(std::size(node_potential_) == num_nodes());
    }

    template<class FlowInputRange, class CostInputRange>
    constexpr Network(const graph_type& graph,
                      FlowInputRange&& surplus,
                      CostInputRange&& cost)
        : super_type(graph),
          node_excess_(ranges::to<container_type<flow_type>>(
                  std::forward<FlowInputRange>(surplus))),
          node_potential_(num_nodes(), zero<cost_type>()),
          arc_cost_(ranges::to<container_type<cost_type>>(
                  std::forward<CostInputRange>(cost)))
    {
        WHIRLWIND_ASSERT(std::size(node_excess_) == num_nodes());
        WHIRLWIND_ASSERT(std::size(arc_cost_) == num_arcs());
        WHIRLWIND_DEBUG_ASSERT(std::size(node_potential_) == num_nodes());
    }

    [[nodiscard]] constexpr auto
    node_excess(const node_type& node) const -> const flow_type&
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_excess_));
        return node_excess_[node_id];
    }

    constexpr void
    increase_node_excess(const node_type& node, flow_type delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_excess_));
        node_excess_[node_id] += delta;
    }

    constexpr void
    decrease_node_excess(const node_type& node, flow_type delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_excess_));
        node_excess_[node_id] -= delta;
    }

    [[nodiscard]] constexpr auto
    is_excess_node(const node_type& node) const -> bool
    {
        WHIRLWIND_ASSERT(contains_node(node));
        return node_excess(node) > zero<flow_type>();
    }

    [[nodiscard]] constexpr auto
    is_deficit_node(const node_type& node) const -> bool
    {
        WHIRLWIND_ASSERT(contains_node(node));
        return node_excess(node) < zero<flow_type>();
    }

    [[nodiscard]] constexpr auto
    excess_nodes() const
    {
        return ranges::views::filter(
                nodes(), [&](const auto& node) { return is_excess_node(node); });
    }

    [[nodiscard]] constexpr auto
    deficit_nodes() const
    {
        return ranges::views::filter(
                nodes(), [&](const auto& node) { return is_deficit_node(node); });
    }

    [[nodiscard]] constexpr auto
    total_excess() const -> size_type
    {
        auto excesses = ranges::views::transform(
                excess_nodes(), [&](const auto& node) { return node_excess(node); });
        return ranges::fold_left(std::move(excesses), 0, std::plus<size_type>());
    }

    [[nodiscard]] constexpr auto
    node_potential(const node_type& node) const -> const cost_type&
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_potential_));
        return node_potential_[node_id];
    }

    constexpr void
    increase_node_potential(const node_type& node, cost_type delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_potential_));
        node_potential_[node_id] += delta;
    }

    constexpr void
    decrease_node_potential(const node_type& node, cost_type delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_potential_));
        node_potential_[node_id] -= delta;
    }

    [[nodiscard]] constexpr auto
    arc_cost(const arc_type& arc) const -> const cost_type&
    {
        WHIRLWIND_ASSERT(contains_arc(arc));
        const auto arc_id = get_arc_id(arc);
        WHIRLWIND_DEBUG_ASSERT(arc_id < std::size(arc_cost_));
        return arc_cost_[arc_id];
    }

    [[nodiscard]] constexpr auto
    arc_reduced_cost(const arc_type& arc,
                     const node_type& tail,
                     const node_type& head) const -> cost_type
    {
        WHIRLWIND_ASSERT(contains_arc(arc));
        WHIRLWIND_ASSERT(contains_node(tail));
        WHIRLWIND_ASSERT(contains_node(head));
        return arc_cost(arc) - node_potential(tail) + node_potential(head);
    }

private:
    container_type<flow_type> node_excess_;
    container_type<cost_type> node_potential_;
    container_type<cost_type> arc_cost_;
};

WHIRLWIND_NAMESPACE_END