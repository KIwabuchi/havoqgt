/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <rpearce@llnl.gov>.
 * LLNL-CODE-644630.
 * All rights reserved.
 *
 * This file is part of HavoqGT, Version 0.1.
 * For details, see https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Our Notice and GNU Lesser General Public License.
 *   http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
 *
 * Our Preamble Notice
 *
 * A. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at the Lawrence
 * Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with the DOE.
 *
 * B. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately-owned rights.
 *
 * C. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring by
 * the United States Government or Lawrence Livermore National Security, LLC. The
 * views and opinions of authors expressed herein do not necessarily state or
 * reflect those of the United States Government or Lawrence Livermore National
 * Security, LLC, and shall not be used for advertising or product endorsement
 * purposes.
 *
 */

#ifndef HAVOQGT_MPI_BREADTH_FIRST_SEARCH_HPP_INCLUDED
#define HAVOQGT_MPI_BREADTH_FIRST_SEARCH_HPP_INCLUDED


#include <havoqgt/visitor_queue.hpp>
#include <havoqgt/detail/visitor_priority_queue.hpp>

namespace havoqgt { namespace mpi {

template <typename Visitor>
class bfs_queue
{
public:
  typedef uint32_t level_number_type;
  typedef typename boost::container::deque<Visitor>::size_type size_type;

protected:
  std::vector<boost::container::deque<Visitor> > m_vec_bfs_level_stack;
  level_number_type m_cur_min_level;
  size_type m_size;
public:
  bfs_queue() : m_vec_bfs_level_stack(20), m_cur_min_level(std::numeric_limits<level_number_type>::max()), m_size(0) { }

  bool push(Visitor const & task)
  {
    while(task.level() >= m_vec_bfs_level_stack.size()) {
      m_vec_bfs_level_stack.push_back(boost::container::deque<Visitor>());
    }
    m_vec_bfs_level_stack[task.level()].push_back(task);
    ++m_size;
    m_cur_min_level = std::min(m_cur_min_level, (uint32_t)task.level());
    return true;
  }

  void pop()
  {
    m_vec_bfs_level_stack[m_cur_min_level].pop_back();
    --m_size;
    if(m_vec_bfs_level_stack[m_cur_min_level].empty()) {
      //if now empty, find next level;
      for(;m_cur_min_level < m_vec_bfs_level_stack.size(); ++m_cur_min_level) {
        if(!m_vec_bfs_level_stack[m_cur_min_level].empty()) break;
      }
    }
  }

  Visitor const & top() //const
  {
    return m_vec_bfs_level_stack[m_cur_min_level].back();
  }

  size_type size() const
  {
    return m_size;
  }

  bool empty() const
  {
    return (m_size == 0);
  }

  void clear()
  {
     for(typename std::vector<boost::container::deque<Visitor> >::iterator itr = m_vec_bfs_level_stack.begin();
       itr != m_vec_bfs_level_stack.end(); ++itr) {
       itr->clear();
     }
     m_size = 0;
     m_cur_min_level = std::numeric_limits<level_number_type>::max();
   }
};



    template<typename Graph, typename LevelData, typename ParentData, typename SourceData>
class bfs_visitor {
public:
  typedef typename Graph::vertex_locator                 vertex_locator;
  bfs_visitor(): m_level(std::numeric_limits<uint64_t>::max())  { }
  bfs_visitor(vertex_locator _vertex, uint64_t _level, vertex_locator _parent, vertex_locator _source)
    : vertex(_vertex)
    , m_parent(_parent)
    , m_source(_source)
    , m_level(_level) { }

  bfs_visitor(vertex_locator _vertex)
    : vertex(_vertex)
    , m_parent(_vertex)
    , m_level(0) { }


  bool pre_visit() const {
    if( (*level_data())[vertex] == std::numeric_limits<uint16_t>::max() ) {
      (*level_data())[vertex] = level();
      (*source_data())[vertex] = m_source;
      (*parent_data())[vertex] = parent();
      return true;
    }
    if( (*source_data())[vertex] == m_source ) {
      bool do_visit  = (*level_data())[vertex] > level();
      if(do_visit) {
	(*level_data())[vertex] = level();
	(*parent_data())[vertex] = parent();
      }
      return do_visit;
    }else {
      if( (*level_data())[vertex] < level() ) {
	(*source_data())[vertex] = m_source;
	(*level_data())[vertex] = level();
	(*parent_data())[vertex] = parent();
	return true;
      }else return false;
    }
  }

  template<typename VisitorQueueHandle>
  bool visit(Graph& g, VisitorQueueHandle vis_queue) const {
      typedef typename Graph::edge_iterator eitr_type;
      for(eitr_type eitr = g.edges_begin(vertex); eitr != g.edges_end(vertex); ++eitr) {
        vertex_locator neighbor = eitr.target();
        //std::cout << "Visiting neighbor: " << g.locator_to_label(neighbor) << std::endl;
        bfs_visitor new_visitor(neighbor, level() + 1,
				vertex, m_source);
        vis_queue->queue_visitor(new_visitor);
      }
    return false;
  }

  uint64_t level() const {  return m_level; }
  vertex_locator parent() const  { return m_parent; }

  friend inline bool operator>(const bfs_visitor& v1, const bfs_visitor& v2) {
    //return v1.level() > v2.level();
    if(v1.m_source != v2.m_source) return !(v1.m_source < v2.m_source);
    
    if(v1.level() > v2.level())
    {
      return true;
    } else if(v1.level() < v2.level())
    {
      return false;
    }
    return !(v1.vertex < v2.vertex);
  }

  // friend inline bool operator<(const bfs_visitor& v1, const bfs_visitor& v2) {
  //   return v1.level() < v2.level();
  // }

  static void set_level_data(LevelData* _data) { level_data() = _data; }

  static LevelData*& level_data() {
    static LevelData* data;
    return data;
  }

  static void set_parent_data(ParentData* _data) { parent_data() = _data; }
  static ParentData*& parent_data() {
    static ParentData* data;
    return data;
  }

  static void set_source_data(SourceData* _data) { source_data() = _data; }
  static SourceData*& source_data() {
    static SourceData* data;
    return data;
  }
      
  vertex_locator   vertex;
  //uint64_t         m_parent : 40;
  vertex_locator  m_parent;
  vertex_locator m_source;
  uint64_t         m_level : 16;
} __attribute__ ((packed));


    template <typename TGraph, typename LevelData, typename ParentData, typename SourceData>
void breadth_first_search(TGraph* g,
                          LevelData& level_data,
                          ParentData& parent_data,
			  SourceData& source_data,
                          typename TGraph::vertex_locator s) {

      typedef  bfs_visitor<TGraph, LevelData, ParentData, SourceData>    visitor_type;
  visitor_type::set_level_data(&level_data);
  visitor_type::set_parent_data(&parent_data);
  visitor_type::set_source_data(&source_data);
  typedef visitor_queue< visitor_type, detail::visitor_priority_queue, TGraph >    visitor_queue_type;

  visitor_queue_type vq(g);
  vq.init_visitor_traversal_reachability();
}



}} //end namespace havoqgt::mpi




#endif //HAVOQGT_MPI_BREADTH_FIRST_SEARCH_HPP_INCLUDED
