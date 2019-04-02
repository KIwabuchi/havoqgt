/*
 * Copyright (c) 2013, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by Roger Pearce <rpearce@llnl.gov>.
 * LLNL-CODE-644630.
 * All rights reserved.
 *
 * This file is part of HavoqGT, Version 0.1.
 * For details, see
 * https://computation.llnl.gov/casc/dcca-pub/dcca/Downloads.html
 *
 * Please also read this link – Our Notice and GNU Lesser General Public
 * License.
 *   http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR
 * A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
 *
 * Our Preamble Notice
 *
 * A. This notice is required to be provided under our contract with the
 * U.S. Department of Energy (DOE). This work was produced at the Lawrence
 * Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with the
 * DOE.
 *
 * B. Neither the United States Government nor Lawrence Livermore National
 * Security, LLC nor any of their employees, makes any warranty, express or
 * implied, or assumes any liability or responsibility for the accuracy,
 * completeness, or usefulness of any information, apparatus, product, or
 * process
 * disclosed, or represents that its use would not infringe privately-owned
 * rights.
 *
 * C. Also, reference herein to any specific commercial products, process, or
 * services by trade name, trademark, manufacturer or otherwise does not
 * necessarily constitute or imply its endorsement, recommendation, or favoring
 * by
 * the United States Government or Lawrence Livermore National Security, LLC.
 * The
 * views and opinions of authors expressed herein do not necessarily state or
 * reflect those of the United States Government or Lawrence Livermore National
 * Security, LLC, and shall not be used for advertising or product endorsement
 * purposes.
 *
 */

#ifndef HAVOQGT_MPI_VISITOR_QUEUE_HPP_INCLUDED
#define HAVOQGT_MPI_VISITOR_QUEUE_HPP_INCLUDED

// #include <havoqgt/new_mailbox.hpp>
// #include <havoqgt/mailbox.hpp>
// #include <havoqgt/termination_detection.hpp>
#include <sched.h>
#include <havoqgt/detail/reservable_priority_queue.hpp>
#include <havoqgt/detail/visitor_priority_queue.hpp>
#include <havoqgt/mpi.hpp>
#include <iterator>
#include <vector>
#include <ygm/mailbox_p2p_nrroute.hpp>

namespace havoqgt {

class oned_blocked_partitioned_t {};
class el_partitioned_t {};

template <typename TVisitor, template <typename T> class Queue, typename TGraph,
          typename AlgData>
class visitor_queue {
  typedef TVisitor visitor_type;

  typedef TGraph                          graph_type;
  typedef typename TGraph::vertex_locator vertex_locator;
  // typedef typename havoqgt::detail::reservable_priority_queue<visitor_type,
  //    std::vector<visitor_type>, std::greater<visitor_type> >
  //    local_queue_type;
  typedef Queue<visitor_type> local_queue_type;

  class mailbox_recv {
   public:
    mailbox_recv(visitor_queue* vq) : m_vq(vq) {}

    template <typename MAILBOX>
    void operator()(MAILBOX* mail, bool bcast, const visitor_type& data) {
      m_vq->handle_mailbox_receive(data);
    }

   private:
    visitor_queue* m_vq;
  };

  typedef ygm::mailbox_p2p_nrroute<visitor_type, mailbox_recv, ygm::atav_comm_exchanger> mailbox_type;

 public:
  visitor_queue(TGraph* _graph, AlgData& _alg_data)
      : m_ptr_graph(_graph), m_alg_data(_alg_data) {
    char* env_batch = getenv("YGM_BATCH_SIZE");
    int   ygm_batch = 1024 * 1024;
    if (env_batch != NULL) {
      ygm_batch = atoi(env_batch);
    }
    m_world_rank = comm_world().rank();
    m_world_size = comm_world().size();
    if (m_world_rank == 0) {
      // std::cout << "YGM_BATCH_SIZE = " << ygm_batch << std::endl;
    }
    m_p_mailbox = new mailbox_type(mailbox_recv(this), ygm_batch);
  }

  ~visitor_queue() {
    delete m_p_mailbox;
    // if(m_world_rank == 0) {
    //   std::cout << "***************  Visitor Queue Statistics
    //   *****************" << std::endl;
    //   std::cout << "m_localqueue_owned.reserve  = " <<
    //   m_ptr_graph->num_local_vertices() << std::endl;
    //   std::cout << "m_localqueue_owned.capacity = " <<
    //   m_localqueue_owned.capacity() << std::endl;
    //   std::cout << "m_localqueue_delegates.reserve  = " <<
    //   0/*m_ptr_graph->num_delegates()*/ << std::endl;
    //   std::cout << "m_localqueue_delegates.capacity = " <<
    //   m_localqueue_delegates.capacity() << std::endl;
    //   std::cout <<
    //   "***********************************************************" <<
    //   std::endl;
    // }
  }

  // class visitor_queue_inserter
  //       : public std::iterator<std::output_iterator_tag, void, void, void,
  //       void> {
  // public:
  //   visitor_queue_inserter(visitor_queue* _vq):m_vq(_vq) { }
  //   visitor_queue_inserter& operator=(const visitor_type& __value) {
  //     m_vq->handle_mailbox_receive(__value);
  //     return *this;
  //   }

  //   bool intercept(const visitor_type& __value) {
  //     assert(m_vq->m_ptr_graph->master(__value.m_visitor.vertex) !=
  //     uint32_t(m_vq->m_world_rank));
  //     bool ret = __value.pre_visit(m_vq->m_alg_data);
  //     if(!ret) {

  //     }
  //     return ret;
  //   }

  //   /// Simply returns *this.
  //   visitor_queue_inserter&
  //   operator*()
  //   { return *this; }

  //   /// Simply returns *this.  (This %iterator does not "move".)
  //   visitor_queue_inserter&
  //   operator++()
  //   { return *this; }

  //   /// Simply returns *this.  (This %iterator does not "move".)
  //   visitor_queue_inserter
  //   operator++(int)
  //   { return *this; }

  // private:
  //   visitor_queue* m_vq;
  // };

  void init_visitor_traversal(vertex_locator _source_v) {
    if (0 /*_source_v.owner()*/ == m_world_rank) {
      queue_visitor(visitor_type(_source_v));
    }
    do {
      process_pending_controllers();
      while (!empty()) {
        process_pending_controllers();
        visitor_type this_visitor = pop_top();
        do_visit(this_visitor);
      }
    } while (!empty() || !m_p_mailbox->global_empty());
  }

  void do_visit(visitor_type& this_visitor) {
    vertex_locator v   = this_visitor.vertex;
    bool           ret = this_visitor.visit(*m_ptr_graph, this, m_alg_data);
    if (ret && v.is_delegate_master()) {
      visitor_type v = this_visitor;
      v.vertex.set_bcast(true);  // FIXME:  remove bcast bit from vertex_locator
      m_p_mailbox->send_bcast(v);
    }
  }

  void do_init_visit(visitor_type& this_visitor) {
    vertex_locator v = this_visitor.vertex;
    bool ret         = this_visitor.init_visit(*m_ptr_graph, this, m_alg_data);
    if (ret && v.is_delegate_master()) {
      visitor_type v = this_visitor;
      v.vertex.set_bcast(true);  // FIXME:  remove bcast bit from vertex_locator
      m_p_mailbox->send_bcast(v);
    }
  }

  void init_visitor_traversal() {
    auto citr = m_ptr_graph->controller_begin();
    auto vitr = m_ptr_graph->vertices_begin();
    do {
      if (citr != m_ptr_graph->controller_end()) {
        visitor_type v(*citr);
        do_init_visit(v);
        ++citr;
      }
      if (vitr != m_ptr_graph->vertices_end()) {
        visitor_type v(*vitr);
        do_init_visit(v);
        ++vitr;
      }
      process_pending_controllers();
      while (!empty()) {
        process_pending_controllers();
        visitor_type this_visitor = pop_top();
        do_visit(this_visitor);
      }
    } while (citr != m_ptr_graph->controller_end() ||
             vitr != m_ptr_graph->vertices_end() || !empty() ||
             !m_p_mailbox->global_empty());
  }

  void init_visitor_traversal(
      const std::vector<vertex_locator>& local_sources) {
    auto sitr = local_sources.begin();
    do {
      if (sitr != local_sources.end()) {
        visitor_type v(*sitr);
        do_init_visit(v);
        ++sitr;
      }
      process_pending_controllers();
      while (!empty()) {
        process_pending_controllers();
        visitor_type this_visitor = pop_top();
        do_visit(this_visitor);
      }
    } while (sitr != local_sources.end() || !empty() ||
             !m_p_mailbox->global_empty());
  }

  void queue_visitor(const visitor_type& v) {
    // std::cout << "queue_visitor()" << std::endl;
    if (v.vertex.is_delegate()) {
      local_delegate_visit(v);
    } else {
      if (v.vertex.owner() == uint32_t(m_world_rank)) {
        if (v.pre_visit(m_alg_data)) {
          push(v);
        }
      } else {
        visitor_type vw = v;
        // vw.m_dest = v.vertex.owner();
        m_p_mailbox->send(v.vertex.owner(), vw);
      }
    }
  }

 private:
  // This occurs when the local process first encounters a delegate
  void local_delegate_visit(const visitor_type& v) {
    if (v.pre_visit(m_alg_data)) {
      if (m_ptr_graph->master(v.vertex) == uint32_t(m_world_rank)) {
        // delegate_bcast(v);
        push(v);
        /*  THIS was working, but trying to change how delegates are bcast from
        the master
        visitor_wrapper vw;
        vw.m_visitor = v;
        vw.set_bcast(true);
        m_p_mailbox.bcast(vw, visitor_queue_inserter(this));*/
      } else {  // send interceptable to parent
        uint32_t master_rank = m_ptr_graph->master(v.vertex);
        m_p_mailbox->send(master_rank, v);
        // delegate_parent(v);
      }
    }
  }

  /*oid delegate_parent(const visitor_type& v) {
    uint64_t parent = offset_tree_parent(m_world_size, 2,
  m_ptr_graph->master(v.vertex),
                                m_world_rank);
    visitor_wrapper vw;
    vw.m_visitor = v;
    vw.m_visitor.vertex.set_dest(parent);
    m_p_mailbox.send(parent, vw, visitor_queue_inserter(this));
  }*/

  /*void delegate_bcast(const visitor_type& v) {
    uint32_t root = m_ptr_graph->master(v.vertex);
    uint32_t num_bcast_children = offset_tree_num_children(m_world_size,
                                                           2, root,
                                                           m_world_rank);
    if(num_bcast_children > 0) {
      uint32_t first_bcast_child = offset_tree_first_child(m_world_size,
                                                           2, root,
                                                           m_world_rank);
      for(uint32_t i=0; i<num_bcast_children; ++i) {
        uint32_t child = (first_bcast_child + i)%m_world_size;
        visitor_wrapper vw;
        vw.m_visitor = v;
        vw.m_visitor.vertex.set_dest(child);
        vw.m_visitor.vertex.set_bcast(true);
        m_p_mailbox.send(child, vw, visitor_queue_inserter(this));
      }
    }
  }*/

  void process_pending_controllers() {
    while (!m_local_controller_queue.empty()) {
      TVisitor v = m_local_controller_queue.front();
      m_local_controller_queue.pop();
      v.visit(*m_ptr_graph, this, m_alg_data);
    }
  }

  void handle_mailbox_receive(visitor_type v) {
    if (v.vertex.is_delegate()) {
      if (v.vertex.get_bcast()) {
        // delegate_bcast(vw.m_visitor);
        if (m_ptr_graph->master(v.vertex) == uint32_t(m_world_rank)) {
          // This is because the mailbox bcast returns to self -- this should be
          // fixed!
        } else {
          // vw.m_visitor.pre_visit();
          // push(vw.m_visitor);
          /*  2013.10.11 -- this causes too much recursion in mailbox, trying
          something new....
          vw.m_visitor.visit(*m_ptr_graph, this);
          */
          m_local_controller_queue.push(v);
        }
      } else {
        assert(m_ptr_graph->master(v.vertex) == uint32_t(m_world_rank));
        if (v.pre_visit(m_alg_data)) {
          // if(m_ptr_graph->master(vw.m_visitor.vertex) == m_world_rank) {
          // delegate_bcast(vw.m_visitor);
          push(v);
          /* This was working, trying new way for master bcast
          vw.set_bcast(true);
          vw.set_intercept(false);
          m_p_mailbox.bcast(vw, visitor_queue_inserter(this));
          m_termination_detection.inc_queued(m_world_size);*/
          //} else {
          //  delegate_parent(vw.m_visitor);
          //}
        } else {
        }
      }
    } else {
      assert(v.vertex.owner() == uint32_t(m_world_rank));
      //
      // Now handle owned vertices
      if (v.pre_visit(m_alg_data)) {
        push(v);
      } else {
      }
    }
  }

  void push(const visitor_type& v) {
    // std::cout << "push()" << std::endl;
    /*if(v.vertex.is_delegate()) {
      m_localqueue_delegates.push(v);
    } else {
      m_localqueue_owned.push(v);
    }*/
    m_localqueue_owned.push(v);
  }

  visitor_type pop_top() {
    visitor_type to_return;
    assert(!(m_localqueue_delegates.empty() && m_localqueue_owned.empty()));

    /*if(m_localqueue_delegates.empty()) {
      to_return = m_localqueue_owned.top();
      m_localqueue_owned.pop();
    } else {
      if(m_localqueue_owned.empty()) {
        to_return = m_localqueue_delegates.top();
        m_localqueue_delegates.pop();
      } else {
        if(m_localqueue_delegates.top() < m_localqueue_owned.top()) {
          to_return = m_localqueue_delegates.top();
          m_localqueue_delegates.pop();
        } else {
          to_return = m_localqueue_owned.top();
          m_localqueue_owned.pop();
        }
      }
    }*/
    to_return = m_localqueue_owned.top();
    m_localqueue_owned.pop();
    return to_return;
  }

  bool empty() {
    return m_localqueue_owned.empty() && m_localqueue_delegates.empty() &&
           m_local_controller_queue.empty();
  }

  /*  void init_visitor_traversal() {
      typedef typename graph_type::vertex_iterator vitr_type;
      std::pair< vitr_type, vitr_type > vitr = vertices(*m_ptr_graph);

      do {
        do {
          do {
            if(vitr.first != vitr.second) {
              uint64_t sampled_vertex = *(vitr.first);
              uint64_t my_rank = m_world_rank;
              if(m_ptr_graph->is_local(vertex(sampled_vertex, *m_ptr_graph))) {
                sampled_vertex |= (my_rank << owner_shift);
                queue_visitor( visitor_type( sampled_vertex ) );
              }
              ++vitr.first;
            }
            while(!empty()) {
              visitor_type this_visitor = top(); pop();
              vertex_locator v = this_visitor.vertex;
              this_visitor.visit((*m_ptr_graph)[v], *m_ptr_graph, this);
              m_termination_detection.inc_completed();
            }
         } while( vitr.first != vitr.second);
          m_p_mailbox.flush_buffers_if_idle();
        } while(!m_p_mailbox.is_idle() );
      } while(!m_termination_detection.test_for_termination());
    }*/

  mailbox_type*        m_p_mailbox;
  local_queue_type     m_localqueue_owned;
  local_queue_type     m_localqueue_delegates;
  TGraph*              m_ptr_graph;
  std::queue<TVisitor> m_local_controller_queue;
  AlgData&             m_alg_data;
  int                  m_world_rank;
  int                  m_world_size;
};

template <typename TVisitor, template <typename T> class Queue, typename TGraph,
          typename AlgData>
visitor_queue<TVisitor, Queue, TGraph, AlgData> create_visitor_queue(
    TGraph* g, AlgData& alg_data) {
  typedef visitor_queue<TVisitor, Queue, TGraph, AlgData> visitor_queue_type;
  visitor_queue_type vq(g, alg_data);
  return vq;
}

}  // namespace havoqgt

#endif  // HAVOQGT_MPI_VISITOR_QUEUE_HPP_INCLUDED
