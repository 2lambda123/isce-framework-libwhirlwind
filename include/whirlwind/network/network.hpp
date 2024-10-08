#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include <range/v3/algorithm/fold_left.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

#include <whirlwind/common/assert.hpp>
#include <whirlwind/common/namespace.hpp>
#include <whirlwind/container/vector.hpp>
#include <whirlwind/graph/graph_concepts.hpp>
#include <whirlwind/math/numbers.hpp>

#include "uncapacitated.hpp"

WHIRLWIND_NAMESPACE_BEGIN

template<GraphType Graph,
         class Cost,
         class Flow,
         template<class> class Container = Vector,
         class Mixin = UncapacitatedMixin<Graph, Flow, Container>>
class Network : public Mixin {
    WHIRLWIND_STATIC_ASSERT(std::is_signed_v<Cost>);

private:
    using super_type = Mixin;

public:
    using graph_type = Graph;
    using node_type = typename super_type::node_type;
    using arc_type = typename super_type::arc_type;
    using cost_type = Cost;
    using flow_type = Flow;
    using size_type = std::size_t;
    using ssize_type = std::make_signed_t<size_type>;

    template<class T>
    using container_type = Container<T>;

    using super_type::arc_flow;
    using super_type::arcs;
    using super_type::contains_arc;
    using super_type::contains_node;
    using super_type::forward_arcs;
    using super_type::get_arc_id;
    using super_type::get_node_id;
    using super_type::is_forward_arc;
    using super_type::nodes;
    using super_type::num_arcs;
    using super_type::num_forward_arcs;
    using super_type::num_nodes;

    template<class RandomAccessRange>
    constexpr Network(const graph_type& graph,
                      container_type<flow_type> surplus,
                      const RandomAccessRange& cost)
        : super_type(graph),
          node_excess_(std::move(surplus)),
          node_potential_(num_nodes(), zero<cost_type>()),
          arc_cost_(make_residual_arc_costs(cost))
    {
        WHIRLWIND_ASSERT(std::size(node_excess_) == num_nodes());
        WHIRLWIND_DEBUG_ASSERT(std::size(arc_cost_) == num_arcs());
        WHIRLWIND_DEBUG_ASSERT(std::size(node_potential_) == num_nodes());
    }

    template<class InputRange, class RandomAccessRange>
    constexpr Network(const graph_type& graph,
                      const InputRange& surplus,
                      const RandomAccessRange& cost)
        : super_type(graph),
          node_excess_(ranges::to<container_type<flow_type>>(surplus)),
          node_potential_(num_nodes(), zero<cost_type>()),
          arc_cost_(make_residual_arc_costs(cost))
    {
        WHIRLWIND_ASSERT(std::size(node_excess_) == num_nodes());
        WHIRLWIND_DEBUG_ASSERT(std::size(arc_cost_) == num_arcs());
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

    /**
     * Get the total excess surplus of all excess nodes.
     *
     * @returns
     *     The sum of the excess surplus among all excess nodes in the network.
     */
    [[nodiscard]] constexpr auto
    total_excess() const -> ssize_type
    {
        auto excesses = ranges::views::transform(
                excess_nodes(), [&](const auto& node) { return node_excess(node); });
        return ranges::fold_left(std::move(excesses), 0, std::plus<ssize_type>());
    }

    /**
     * Get the total excess demand of all deficit nodes.
     *
     * @returns
     *     The sum of the excess surplus among all deficit nodes in the network (a
     *     negative value).
     */
    [[nodiscard]] constexpr auto
    total_deficit() const -> ssize_type
    {
        auto deficits = ranges::views::transform(
                deficit_nodes(), [&](const auto& node) { return node_excess(node); });
        return ranges::fold_left(std::move(deficits), 0, std::plus<ssize_type>());
    }

    [[nodiscard]] constexpr auto
    is_balanced() const -> bool
    {
        const auto imbalance =
                ranges::fold_left(node_excess_, 0, std::plus<ssize_type>());
        return imbalance == ssize_type{0};
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
    increase_node_potential(const node_type& node, const cost_type& delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_potential_));
        node_potential_[node_id] += delta;
    }

    constexpr void
    decrease_node_potential(const node_type& node, const cost_type& delta)
    {
        WHIRLWIND_ASSERT(contains_node(node));
        const auto node_id = get_node_id(node);
        WHIRLWIND_DEBUG_ASSERT(node_id < std::size(node_potential_));
        node_potential_[node_id] -= delta;
    }

    /**
     * Get the cost per unit of flow in an arc.
     *
     * @param[in] arc
     *     The input arc. Must be a valid arc in the network's residual graph (though
     *     its residual capacity may be zero).
     *
     * @returns
     *     The unit cost of flow in the arc.
     */
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

    [[nodiscard]] constexpr auto
    total_cost() const -> cost_type
    {
        auto arc_costs = ranges::views::transform(forward_arcs(), [&](const auto& arc) {
            const auto flow = arc_flow(arc);
            const auto cost = arc_cost(arc);
            return cost * flow;
        });
        return ranges::fold_left(std::move(arc_costs), zero<cost_type>(),
                                 std::plus<cost_type>());
    }

protected:
    template<class RandomAccessRange>
    [[nodiscard]] constexpr auto
    make_residual_arc_costs(const RandomAccessRange& forward_cost)
            -> container_type<cost_type>
    {
        WHIRLWIND_ASSERT(std::size(forward_cost) == num_forward_arcs());
        return arcs() | ranges::views::transform([&](const auto& arc) {
                   if (is_forward_arc(arc)) {
                       const auto edge_id = this->get_edge_id(arc);

                       WHIRLWIND_DEBUG_ASSERT(edge_id < std::size(forward_cost));
                       const auto cost = forward_cost[edge_id];
                       if constexpr (std::is_floating_point_v<decltype(cost)>) {
                           WHIRLWIND_ASSERT(!std::isnan(cost));
                       }
                       WHIRLWIND_ASSERT(cost >= zero<cost_type>());
                       return cost;
                   } else {
                       const auto transpose_arc = this->get_transpose_arc_id(arc);
                       const auto edge_id = this->get_edge_id(transpose_arc);

                       WHIRLWIND_DEBUG_ASSERT(edge_id < std::size(forward_cost));
                       const auto cost = forward_cost[edge_id];
                       if constexpr (std::is_floating_point_v<decltype(cost)>) {
                           WHIRLWIND_ASSERT(!std::isnan(cost));
                       }
                       WHIRLWIND_ASSERT(cost >= zero<cost_type>());
                       return -cost;
                   }
               }) |
               ranges::to<container_type<cost_type>>();
    }

private:
    container_type<flow_type> node_excess_;
    container_type<cost_type> node_potential_;
    container_type<cost_type> arc_cost_;
};

WHIRLWIND_NAMESPACE_END
