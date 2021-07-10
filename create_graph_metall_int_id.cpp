#include <iostream>
#include <string>
#include <metall/metall.hpp>
#include <metall/container/experimental/json/json.hpp>
#include <metall/container/experimental/jgraph/jgraph_primitive_id.hpp>

using namespace metall::container::experimental;

using graph_type = jgraph::jgraph_primitive_id<uint32_t, metall::manager::allocator_type<std::byte>>;

template <typename T>
graph_type::key_type convert_id(const T& id) {
  return std::stoull(id);
}

int main() {

  // Assumes that the format of the input file is JSON Lines
  //const std::string input_json_file_name("/usr/workspace/reza2/metall/metalljson/spoke.json");//("../spoke.json");
  const std::string input_json_file_name("/p/lustre2/havoqgtu/Spoke/spoke-20201130.json");
  //const std::string input_json_file_name("/p/lustre2/havoqgtu/Spoke/spoke-20201130/file_3_l15");

  {
    std::cout << "--- Create ---" << std::endl;
    metall::manager manager(metall::create_only, "/dev/shm/jgraph_spoke_undir_obj");

    std::ifstream ifs(input_json_file_name);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open file: " << input_json_file_name << std::endl;
      std::abort();
    }

    std::string json_string;
    auto *graph = manager.construct<graph_type>(metall::unique_instance)(manager.get_allocator());

    // Parse each line of the input file one by one
    while (std::getline(ifs, json_string)) {

      try {

        // Parse the JSON string and allocate a JSON value object.
        // Pass a Metall allocator so that the contents of the object is allocated in Metall space.
        auto json_value = json::parse(json_string, graph->get_allocator());

        if (json_value.as_object()["type"].as_string() == "node") {
          const auto vertex_id = convert_id(json_value.as_object()["id"].as_string());
          graph->add_vertex(vertex_id)->value() = std::move(json_value);
        } else if (json_value.as_object()["type"].as_string() == "relationship") {
          const auto src_id = convert_id(json_value.as_object()["start"].as_object()["id"].as_string());
          const auto dst_id = convert_id(json_value.as_object()["end"].as_object()["id"].as_string());
          graph->add_edge(src_id, dst_id)->value() = json_value;
          graph->add_edge(dst_id, src_id)->value() = json_value;
        }

      } catch (const std::exception &e) {
        //std::cout << " a standard exception was caught, with message '" << e.what() << std::endl;
      }

    } // while

    std::cout << "#of vertices: " << graph->num_vertices() << std::endl;
    std::cout << "#of edges: " << graph->num_edges() << std::endl;
  }

  return 0;
}
