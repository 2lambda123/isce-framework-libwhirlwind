#pragma once

#include <memory>
#include <utility>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/fill.hpp>

#include <whirlwind/common/assert.hpp>
#include <whirlwind/common/namespace.hpp>
#include <whirlwind/container/vector.hpp>

#include "graph_concepts.hpp"
#include "predecessors_view.hpp"

WHIRLWIND_NAMESPACE_BEGIN

/**
 * A forest of rooted trees in a graph.
 *
 * Each vertex belongs to a single tree in the forest and stores its predecessor (or
 * parent) vertex and edge in the tree, enabling traversal up to the tree's root. A root
 * vertex's predecessor is itself.
 *
 * A `Forest` maintains a non-owning pointer to its underlying graph. It may be
 * invalidated if the graph is modified or its lifetime ends.
 *
 * @tparam Graph
 *     The graph type.
 * @tparam Container
 *     A `std::vector`-like type template used to store the internal arrays of
 *     predecessor vertices and edges.
 */
template<GraphType Graph, template<class> class Container = Vector>
class Forest {
public:
    using graph_type = Graph;
    using vertex_type = typename graph_type::vertex_type;
    using edge_type = typename graph_type::edge_type;
    using pred_type = std::pair<vertex_type, edge_type>;

    template<class T>
    using container_type = Container<T>;

    /**
     * Create a new `Forest`.
     *
     * The forest is initialized such that each vertex in the graph is the root of its
     * own singleton tree (by setting its predecessor vertex to itself).
     *
     * @param[in] graph
     *     The forest's underlying graph.
     * @param[in] edge_fill_value
     *     The default predecessor edge value for vertices that have no predecessor.
     */
    explicit constexpr Forest(const graph_type& graph, edge_type edge_fill_value = {})
        : graph_(std::addressof(graph)),
          // XXX This should preferably use `ranges::to` but GCC v12.4.0 warns about
          // detecting UB  when compiling with `-Waggressive-loop-optimizations` so
          // let's use this workaround instead for now.
          pred_vertex_([&]() {
              auto pred_vertex = container_type<vertex_type>(graph.num_vertices());
              ranges::copy(graph.vertices(), std::begin(pred_vertex));
              return pred_vertex;
          }()),
          pred_edge_(graph.num_vertices(), edge_fill_value),
          edge_fill_value_(std::move(edge_fill_value))
    {
        WHIRLWIND_DEBUG_ASSERT(std::size(pred_vertex_) == graph.num_vertices());
        WHIRLWIND_DEBUG_ASSERT(std::size(pred_edge_) == graph.num_vertices());
    }

    /** The underlying graph. */
    [[nodiscard]] constexpr auto
    graph() const noexcept -> const graph_type&
    {
        WHIRLWIND_DEBUG_ASSERT(graph_ != nullptr);
        return *graph_;
    }

    /**
     * Get the predecessor (parent) vertex of a vertex.
     *
     * If `vertex` is a root vertex, its predecessor is itself.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid vertex in the graph.
     *
     * @returns
     *     The vertex's predecessor vertex.
     */
    [[nodiscard]] constexpr auto
    predecessor_vertex(const vertex_type& vertex) const -> const vertex_type&
    {
        WHIRLWIND_ASSERT(graph().contains_vertex(vertex));
        const auto vertex_id = graph().get_vertex_id(vertex);
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_vertex_));
        return pred_vertex_[vertex_id];
    }

    /**
     * Get the predecessor edge of a vertex.
     *
     * The predecessor edge is only valid if `vertex` is not a root vertex of a tree in
     * the forest.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid, non-root vertex in the graph.
     *
     * @returns
     *     The vertex's predecessor edge.
     */
    [[nodiscard]] constexpr auto
    predecessor_edge(const vertex_type& vertex) const -> const edge_type&
    {
        WHIRLWIND_ASSERT(graph().contains_vertex(vertex));
        WHIRLWIND_ASSERT(!is_root_vertex(vertex));
        const auto vertex_id = graph().get_vertex_id(vertex);
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_edge_));
        return pred_edge_[vertex_id];
    }

    /**
     * Get the predecessor vertex and edge of a vertex.
     *
     * The predecessor edge is only valid if `vertex` is not a root vertex of a tree in
     * the forest.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid, non-root vertex in the graph.
     *
     * @retval pred_vertex
     *     The vertex's predecessor vertex.
     * @retval pred_edge
     *     The vertex's predecessor edge.
     */
    [[nodiscard]] constexpr auto
    predecessor(const vertex_type& vertex) const -> pred_type
    {
        return pred_type(predecessor_vertex(vertex), predecessor_edge(vertex));
    }

    /**
     * Iterate over a vertex's predecessor vertices and edges.
     *
     * Returns a view of the input vertex's predecessors. Iterating over the result
     * traverses a path in the graph from `vertex` to the root of the tree on which it
     * resides, while yielding (tail,edge) pairs of the predecessors of each visited
     * vertex (excluding the root vertex).
     *
     * The view is empty if `vertex` is a root vertex.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid vertex in the graph.
     *
     * @returns
     *     A view of the vertex's predecessors.
     */
    [[nodiscard]] constexpr auto
    predecessors(const vertex_type& vertex) const
    {
        WHIRLWIND_ASSERT(graph().contains_vertex(vertex));
        return PredecessorsView(this, vertex);
    }

    /**
     * Assign a vertex's predecessor.
     *
     * @param[in] vertex
     *     The successor (child) vertex. Must be a valid vertex in the graph.
     * @param[in] pred_vertex
     *     The predecessor (parent) vertex. Must be a valid vertex in the graph.
     * @param[in] pred_edge
     *     The predecessor edge of `vertex`. If `vertex` is not a root vertex, it must
     *     be a valid edge in the graph whose head is `vertex` and whose tail is
     *     `pred_vertex`.
     */
    constexpr void
    set_predecessor(const vertex_type& vertex,
                    vertex_type pred_vertex,
                    edge_type pred_edge)
    {
        WHIRLWIND_ASSERT(graph().contains_vertex(vertex));
        WHIRLWIND_ASSERT(graph().contains_vertex(pred_vertex));
        WHIRLWIND_ASSERT((vertex == pred_vertex) || graph().contains_edge(pred_edge));

        const auto vertex_id = graph().get_vertex_id(vertex);
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_vertex_));
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_edge_));

        pred_vertex_[vertex_id] = std::move(pred_vertex);
        pred_edge_[vertex_id] = std::move(pred_edge);
    }

    /**
     * Assign a vertex's predecessor.
     *
     * @param[in] vertex
     *     The child vertex. Must be a valid vertex in the graph.
     * @param[in] pred
     *     The predecessor vertex and edge. `pred.first` must be a valid vertex in the
     *     graph. If `vertex` is not a root vertex, `pred.second` must be a valid edge
     *     in the graph whose head is `vertex` and whose tail is the `pred.first`.
     */
    constexpr void
    set_predecessor(const vertex_type& vertex, pred_type pred)
    {
        set_predecessor(vertex, std::move(pred.first), std::move(pred.second));
    }

    /**
     * Make the specified vertex into a root vertex.
     *
     * Make the specified vertex the root of a new tree by setting its predecessor
     * vertex to itself and setting its predecessor edge to `edge_fill_value`.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid vertex in the graph.
     */
    constexpr void
    make_root_vertex(vertex_type vertex)
    {
        WHIRLWIND_ASSERT(graph().contains_vertex(vertex));

        const auto vertex_id = graph().get_vertex_id(vertex);
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_vertex_));
        WHIRLWIND_DEBUG_ASSERT(vertex_id < std::size(pred_edge_));

        pred_vertex_[vertex_id] = std::move(vertex);
        pred_edge_[vertex_id] = edge_fill_value();
    }

    /**
     * Check if a vertex is the root of a tree in the forest.
     *
     * @param[in] vertex
     *     The input vertex. Must be a valid vertex in the graph.
     *
     * @returns
     *     True if the vertex is a root vertex; otherwise false.
     */
    [[nodiscard]] constexpr auto
    is_root_vertex(const vertex_type& vertex) const -> bool
    {
        return predecessor_vertex(vertex) == vertex;
    }

    /** The default predecessor edge value for vertices that have no predecessor. */
    [[nodiscard]] constexpr auto
    edge_fill_value() const noexcept -> const edge_type&
    {
        return edge_fill_value_;
    }

    /**
     * Reset the forest to its initial state.
     *
     * Re-initializes the forest such that each vertex in the graph is the root of its
     * own singleton tree (by setting its predecessor vertex to itself). Each
     * predecessor edge is set to the value of `edge_fill_value`.
     */
    constexpr void
    reset()
    {
        WHIRLWIND_DEBUG_ASSERT(std::size(pred_vertex_) == graph().num_vertices());
        ranges::copy(graph().vertices(), std::begin(pred_vertex_));
        WHIRLWIND_DEBUG_ASSERT(std::size(pred_edge_) == graph().num_vertices());
        ranges::fill(pred_edge_, edge_fill_value());
    }

private:
    const graph_type* graph_;
    container_type<vertex_type> pred_vertex_;
    container_type<edge_type> pred_edge_;
    edge_type edge_fill_value_;
};

WHIRLWIND_NAMESPACE_END
