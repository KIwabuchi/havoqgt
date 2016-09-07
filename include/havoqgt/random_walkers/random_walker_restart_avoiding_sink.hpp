#ifndef _SIMPLE_RANDOM_WALKER_HPP
#define _SIMPLE_RANDOM_WALKER_HPP

#include <unordered_set>
#include <iostream>
#include <tuple>
#include <random>
class random_number_generator{
public:
  uint32_t uniform_int( uint32_t size ) {
    std::uniform_int_distribution<uint64_t> uniform_int(0, size - 1);
    return uniform_int( mt );
  }

  static random_number_generator& get_rng() {
    if(!random_number_generator::initialized) {
      random_number_generator::initialized = true;
      rng = random_number_generator();
    }
    return rng;
  }
private:
  random_number_generator() { 
    std::random_device r;
    mt.seed(r());
  }
  std::mt19937 mt;
  static random_number_generator rng;
  static bool initialized;
};
random_number_generator random_number_generator::rng;
bool random_number_generator::initialized = false;


template <typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
class simple_random_walker {
public:
  using vertex_locator = VertexType;
  using metadata_type = MetaDataType;

  typedef std::pair< uint64_t, uint64_t> interval;

  simple_random_walker() : id(0) { }
  
  simple_random_walker( uint64_t _id, vertex_locator _start_from, interval _started_at
			,interval _cur_time
			,uint64_t _cost = 0
			,uint64_t _steps = 0
			,uint64_t _attempts = 0
			,uint64_t _total_steps = 0
			)
    : id(_id), start_from(_start_from), cost(_cost),
      cur_time(_cur_time), started_at(_started_at), 
      steps(_steps), attempts(_attempts), total_steps(_total_steps) { }

  bool is_complete(vertex_locator vertex) const {
    //return (local_targets.find(vertex) != local_targets.end() ) || steps >= simple_random_walker::max_steps;
    return steps >= simple_random_walker::max_steps;
  }

  bool restart_walker(bool has_neighbor,  uint64_t restart_prob) const {
    if( has_neighbor ) {
      return false;
      //auto random_percentage = random_number_generator::get_rng().uniform_int( (uint32_t)100 );
      //return random_percentage < restart_prob;
    }
    else {
    return true;
    }
  }

  //Function returning the next state for the random walkers
  std::tuple<bool, vertex_locator, simple_random_walker> next(vertex_locator cur_vertex ) const{
    uint64_t edge_cost = 0;
    vertex_locator edge_target;
    int type, ns;
    auto op = [&edge_cost, &edge_target, &type, &ns]( metadata_type& metadata, vertex_locator target)->void {
      edge_cost = metadata.redirect ? 0 : 1;
      edge_target = target;
      ns = metadata.ns;
      type = metadata.type;
    };
    
    std::pair<bool, interval> next;

    int32_t trials = -1;
    do {
      trials++;
      if( ( trials + steps ) >= simple_random_walker::max_steps ) { 
	return std::make_tuple( false, vertex_locator(), simple_random_walker() );
      }
      float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      //Die with probability death_prob
      if ( r < simple_random_walker::death_prob ) {
	return std::make_tuple( false, vertex_locator(), simple_random_walker() );
      }
      next = random_edge_container->template get_random_weighted_edge(op, cur_time, cur_vertex);
      bool restart_rw = restart_walker( next.first, prob_restart);
      if( restart_rw ) {
	if( attempts >= max_attempts ) {
	  //return std::make_tuple( false, vertex_locator(),  simple_random_walker() );
	}
	else {
	  simple_random_walker restart_rw( id, start_from, started_at, cur_time, 0, 0, attempts + 1, total_steps + 1 + trials);
	  return std::make_tuple( true, start_from, restart_rw);
	}
      }

    } while( next.first && (type == 2 || ns == 6));

    if( !next.first ) return std::make_tuple(false, vertex_locator(), simple_random_walker());

    simple_random_walker next_rw(id, start_from, started_at, next.second, cost + edge_cost + trials, steps + 1 + trials, attempts, total_steps + 1 + trials);
    return std::make_tuple( true , edge_target, next_rw );
  }

  friend std::ostream& operator<<(std::ostream& o, const simple_random_walker& rw) {
    return o << rw.id
	     << " "  << rw.started_at.first << " " << rw.started_at.second
             << " " << rw.cur_time.first   << " " << rw.cur_time.second
	     << " " << rw.cost << " " << rw.steps
	     << " " << rw.attempts << " " << rw.total_steps;
  }  
  // Just making them public
  uint64_t id;
  vertex_locator start_from;
  uint64_t cost;
  uint64_t steps;
  interval cur_time;
  interval started_at;
  uint64_t attempts;
  uint64_t total_steps;

  static std::unordered_set<vertex_locator, typename vertex_locator::hasher> local_targets;
  static uint64_t max_steps;
  static float death_prob;
  static RandomEdgeContainer* random_edge_container;
  static uint64_t max_attempts;
  static uint32_t prob_restart;
};

template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
std::unordered_set< VertexType, typename VertexType::hasher> simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::local_targets;

template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
uint64_t simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::max_steps;

template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
float simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::death_prob;

template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
RandomEdgeContainer *simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::random_edge_container;


template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
uint64_t simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::max_attempts = 10;

template<typename MetaDataType, typename VertexType, typename RandomEdgeContainer>
uint32_t simple_random_walker<MetaDataType, VertexType, RandomEdgeContainer>::prob_restart = 1;

#endif
