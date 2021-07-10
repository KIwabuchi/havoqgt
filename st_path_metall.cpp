#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "omp.h"

#include <assert.h>
#include <unistd.h>

//#include <iostream>
//#include <string>
#include <metall/metall.hpp>
#include <metall/container/experimental/jgraph/jgraph.hpp>

using namespace metall::container::experimental;

using graph_type = jgraph::jgraph<metall::manager::allocator_type<std::byte>>;

/**
 * Returns elapsed (monotonic) time.
 */
double getElapsedTimeSecond(
    std::chrono::time_point<std::chrono::steady_clock> startTime,
    std::chrono::time_point<std::chrono::steady_clock> endTime) {
  return ((endTime - startTime).count()) *
      std::chrono::steady_clock::period::num /
      static_cast<double>(std::chrono::steady_clock::period::den);
}

template <typename Graph, typename Vertex>
bool bfs_level_synchronous_single_source(Graph *graph,
                                         const Vertex &source_vertex, const Vertex &target_vertex) {

  size_t current_level = 0;
  std::unordered_map<Vertex, std::pair<size_t, Vertex>> bfs_level; // level, predecessor
  bfs_level[source_vertex].first = current_level;

  bool finished = false;
  bool target_vertex_found = false;

  do {

    finished = true;

    for (auto vitr = graph->vertices_begin(), vend = graph->vertices_end();
         vitr != vend; ++vitr) {

      const auto &v_value = vitr->value();
      const auto &v_id = v_value.as_object()["id"].as_string();
      auto find_vertex = bfs_level.find(v_id);
      if (find_vertex == bfs_level.end()) {
        continue;
      }

      if (bfs_level[v_id].first == current_level) {

        for (auto eitr = graph->edges_begin(vitr->key()),
                 eend = graph->edges_end(vitr->key()); eitr != eend; ++eitr) {

          auto &e_value = eitr->value();
          auto v_nbr = e_value.as_object()["end"].as_object()["id"].as_string();

          auto find_v_nbr = bfs_level.find(v_nbr);
          if (find_v_nbr == bfs_level.end()) {
            bfs_level[v_nbr].first = current_level + 1;
            bfs_level[v_nbr].second = v_id; // predecessor

            if (finished == true) {
              finished = false;
            }
          }

          if (v_nbr == target_vertex) {
            target_vertex_found = true;
            break;
          }
        } // for
      }

      if (target_vertex_found) {
        finished = true;
        break;
      }

    } // for

    current_level++;

  } while (!finished);

  if (target_vertex_found) {
    std::cout << target_vertex << " found at depth " <<
              bfs_level[target_vertex].first << std::endl;
  } else {
    std::cout << target_vertex << " was not found at maximum depth " <<
              (current_level - 1) << std::endl;
  }

  std::cout << "#visited vertices: " << bfs_level.size() << std::endl;

  if (target_vertex_found && (bfs_level.size() >= 2) &&
      bfs_level[target_vertex].first <= 25) {

    current_level = bfs_level[target_vertex].first;
    Vertex current_path_vertex = target_vertex;

    size_t path_length = 0;

    do {
      Vertex predecessor_vertex = bfs_level[current_path_vertex].second;

      std::cout << current_path_vertex << " " << predecessor_vertex
                << std::endl;

      current_level = bfs_level[predecessor_vertex].first;
      current_path_vertex = predecessor_vertex;

      if (current_level == bfs_level[source_vertex].first ||
          path_length >= (bfs_level[target_vertex].first - 1)) {
        break;
      } else {
        path_length++;
      }

    } while (current_path_vertex != source_vertex);

  } // if 

  return target_vertex_found;
} // bfs_level_synchronous_single_source  

int main() {
  metall::manager manager(metall::open_read_only, "/dev/shm/jgraph_spoke_undir_obj");
  const auto *graph = manager.find<graph_type>(metall::unique_instance).first;

  std::cout << "#of vertices: " << graph->num_vertices() << std::endl;
  std::cout << "#of edges: " << graph->num_edges() << std::endl;

  using Vertex = std::string; //std::string_view; // wrong

  Vertex source_vertex = "2283541"; //"2268378";
  Vertex target_vertex = "2268378"; //"2283541";

  std::chrono::time_point<std::chrono::steady_clock> global_start_time = std::chrono::steady_clock::now();

  bfs_level_synchronous_single_source(graph, source_vertex, target_vertex);

  std::chrono::time_point<std::chrono::steady_clock> global_end_time = std::chrono::steady_clock::now();
  double global_elapsed_time = getElapsedTimeSecond(global_start_time, global_end_time);
  std::cout << "Time: " << global_elapsed_time << " seconds." << std::endl;

  return 0;
}
