#include <gtest/gtest.h>
#include <havoqgt/delegate_partitioned_graph.hpp>
#include <havoqgt/parallel_edge_list_reader.hpp>
#include <havoqgt/environment.hpp>
#include <havoqgt/cache_utilities.hpp>
#include <havoqgt/distributed_db.hpp>
#include <havoqgt/breadth_first_search.hpp>
#include <havoqgt/bias_randwalk.hpp>
#include <iostream>
#include <assert.h>
#include <deque>
#include <utility>
#include <algorithm>
#include <functional>
#include <fstream>      // std::ifstream
#include <unistd.h>
#include <random>

namespace havoqgt { namespace test {

  using namespace havoqgt;

  typedef havoqgt::distributed_db::segment_manager_type segment_manager_t;
  typedef havoqgt::delegate_partitioned_graph<segment_manager_t> graph_type;

TEST(test_static_bfs, test_static_bfs) {
  MPI_Barrier(MPI_COMM_WORLD);  
  int mpi_rank(0), mpi_size(0);
  CHK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank));
  CHK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &mpi_size));
    
  havoqgt::distributed_db ddb(havoqgt::db_create(), "test_static_bfs", 0.01f);
  
  segment_manager_t* segment_manager = ddb.get_segment_manager();
  bip::allocator<void, segment_manager_t> alloc_inst(segment_manager);

  std::vector< std::pair<uint64_t,uint64_t> > edges;
  size_t num_vertices = 10000;
  for(size_t i=1; i<num_vertices; ++i) {
    if(i % mpi_size == mpi_rank) {
      edges.push_back(std::make_pair(i-1,i));
      edges.push_back(std::make_pair(i,i-1));
    }
  }

  graph_type *graph = segment_manager->construct<graph_type>
      ("graph_obj")
      (alloc_inst, MPI_COMM_WORLD, edges, num_vertices, 1024, 1, 1024); 
  
  graph_type::vertex_data<uint16_t, std::allocator<uint16_t> >                      bfs_level_data(*graph);
  graph_type::edge_data<uint16_t, std::allocator<uint16_t> >                        edge_data(*graph);  // how many times each edge has been visited
  graph_type::vertex_data<uint16_t, std::allocator<uint16_t> >                      vertex_data(*graph); //how many times each vertex has been visited
  graph_ytpe::vertex_data<std::discrete_distribution<>, std::allocator< discrete_distribution<> >>   prob_data(*graph);

  bfs_level_data.reset(num_vertices+1);
  graph_type::vertex_data<graph_type::vertex_locator, std::allocator<graph_type::vertex_locator> >  bfs_parent_data(*graph);
  graph_type::vertex_locator source = graph->label_to_locator(0);
  
  MPI_Barrier(MPI_COMM_WORLD);
  havoqgt::breadth_first_search(graph, bfs_level_data, bfs_parent_data, source);
  havoqgt::copy_edge(graph, bfs_level_data, copy_edge_data, vertex_data, prob_data);
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  std::cout << "FINISHED BFS" << std::endl;
/*
  for(size_t i=0; i<num_vertices; ++i) {
    graph_type::vertex_locator vertex = graph->label_to_locator(i);
    if(vertex.owner() == mpi_rank) {
      size_t level = bfs_level_data[vertex];
      EXPECT_EQ(level, i);
    }
  }
*/
  for(auto vitr = graph->vertices_begin(); vitr != graph->vertices_end(); ++vitr) {
    for(auto eitr = graph->edges_begin(*vitr); eitr != graph->edges_end(*vitr); ++eitr) {
//  for(size_t i=1; i<num_vertices; ++i) {
    //graph_type::vertex_locator vertex = graph->label_to_locator(edges[i].first);
    graph_type::vertex_locator vertex = *vitr; 
    //if(vertex.owner() == mpi_rank) {
       size_t level=graph->locator_to_label(eitr.target());
       EXPECT_EQ(level, copy_edge_data[eitr]);
    //}
   }
  } 
  for (auto vitr = graph->vertices_begin(); vitr != graph->vertices_end(); ++vitr) {
    for(auto eitr = graph->edges_begin(*vitr); eitr != graph->edges_end(*vitr); ++eitr) {
      //std::cout << bfs_level_data[*vitr] << " " << bfs_level_data[eitr.target()]<<" " << copy_edge_data[eitr]<<std::endl;
      std::cout << graph->locator_to_label(eitr.target()) << " " << copy_edge_data[eitr] << std::endl; 
  }
 }
}



}} //end namespace havoqgt::test

//mpi main for gteset
GTEST_API_ int main(int argc, char **argv) {
  // set up environment
  int mpi_rank(0), mpi_size(0), to_return;
  havoqgt::havoqgt_init(&argc, &argv);
  {
  CHK_MPI(MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank));
  CHK_MPI(MPI_Comm_size(MPI_COMM_WORLD, &mpi_size));
  havoqgt::get_environment();

  if (mpi_rank == 0) {
    std::cout << "MPI initialized with " << mpi_size << " ranks." << std::endl;
    //havoqgt::get_environment().print();
    //print_system_info(false); 
  }
  MPI_Barrier(MPI_COMM_WORLD);
   
  // execute tests
  testing::InitGoogleTest(&argc, argv);
  to_return = RUN_ALL_TESTS();
  }

  // delete the generated files

  havoqgt::havoqgt_finalize();

  return to_return;
}
