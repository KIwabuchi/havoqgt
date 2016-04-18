/*
 * Written by Keita Iwabuchi.
 * LLNL / TokyoTech
 */

#ifndef DEGAWARERHH_HPP
#define DEGAWARERHH_HPP

#include <type_traits>

#include <havoqgt/graphstore/rhh/rhh_defs.hpp>
#include <havoqgt/graphstore/rhh/rhh_utilities.hpp>
#include <havoqgt/graphstore/rhh/rhh_container.hpp>
#include <havoqgt/graphstore/rhh/rhh_allocator_holder.hpp>

#include <havoqgt/graphstore/graphstore_utilities.hpp>

namespace graphstore {

template <typename _vertex_type,
          typename _vertex_property_data_type,
          typename _edge_property_data_type,
          typename _segment_manager_type,
          size_t middle_high_degree_threshold = 2>
class degawarerhh
{
 public:
  using vertex_type                 = _vertex_type;
  using vertex_property_data_type   = _vertex_property_data_type;
  using edge_property_data_type     = _edge_property_data_type;
  using segment_manager_type        = _segment_manager_type;

  /// --- iterators --- ///
  class vertex_iterator;
  class adjacent_edge_iterator;

 private:
  using size_type              = size_t;
  using ldeg_table_value_type  = utility::packed_tuple<vertex_property_data_type, vertex_type, edge_property_data_type, void>;
  using ldeg_table_type        = rhh_container<vertex_type, ldeg_table_value_type, size_type, segment_manager_type>;

  using mhdeg_edge_chunk_type  = rhh_container<vertex_type, edge_property_data_type, size_type, segment_manager_type>;
  using mhdeg_table_value_type = utility::packed_pair<vertex_property_data_type, mhdeg_edge_chunk_type*>;
  using mhdeg_table_type       = rhh_container<vertex_type, mhdeg_table_value_type, size_type, segment_manager_type>;
  using degawarerhh_selftype   = degawarerhh<vertex_type, vertex_property_data_type, edge_property_data_type,
                                                          segment_manager_type, middle_high_degree_threshold>;

 public:

  explicit degawarerhh(segment_manager_type* segment_manager) :
    m_ldeg_table(nullptr),
    m_mhdeg_table(nullptr),
    m_num_edges(0)
  {
    static_assert(middle_high_degree_threshold > 1, "middle-high degree threshold is must be larger than 1");

    // -- init allocator -- //
    rhh::init_allocator<typename ldeg_table_type::allocator, segment_manager_type>(segment_manager);
    rhh::init_allocator<typename mhdeg_edge_chunk_type::allocator, segment_manager_type>(segment_manager);
    rhh::init_allocator<typename mhdeg_table_type::allocator, segment_manager_type>(segment_manager);

    m_ldeg_table = ldeg_table_type::allocate(2);
    m_mhdeg_table = mhdeg_table_type::allocate(2);

#if RHH_DETAILED_ANALYSYS
    m_ldeg_table->init_detailed_analysis();
#endif
  }

  ~degawarerhh()
  {
    clear();
    ldeg_table_type::deallocate(m_ldeg_table);
    mhdeg_table_type::deallocate(m_mhdeg_table);
    rhh::destroy_allocator<typename ldeg_table_type::allocator>();
    rhh::destroy_allocator<typename mhdeg_edge_chunk_type::allocator>();
    rhh::destroy_allocator<typename mhdeg_table_type::allocator>();
  }

  /// explicitly delete to prevent unexpected behaivers
  degawarerhh(const degawarerhh&) = delete;
  degawarerhh(degawarerhh&&) = delete;
  degawarerhh& operator=(const degawarerhh&) = delete;
  degawarerhh& operator=(degawarerhh&&) = delete;


  /// -------- Lookup -------- ///
  inline std::pair<vertex_iterator, vertex_iterator> find_vertex(const vertex_type& vertex)
  {
    return std::make_pair(vertex_iterator(m_ldeg_table->find(vertex), m_mhdeg_table->find(vertex)),
                          vertex_iterator(m_mhdeg_table->find_end(), m_mhdeg_table->find_end()));
  }

  /// TODO: implementation
  inline std::pair<adjacent_edge_iterator, adjacent_edge_iterator> find_edge(const vertex_type& vertex)
  {
  }

  inline std::pair<vertex_iterator, vertex_iterator> vertices() /// !!!!! this doesn't work !!!!!!
  {
    return std::make_pair(vertices_begin(), vertices_end());
  }

  inline std::pair<adjacent_edge_iterator, adjacent_edge_iterator> adjacent_edge(const vertex_type& src_vrtx)
  {
    return std::make_pair(adjacent_edge_begin(src_vrtx), adjacent_edge_end());
  }

  /// TOD: move the following functions to private
  inline vertex_iterator vertices_begin()  /// !!!!! this doesn't work !!!!!!
  {
    return vertex_iterator(ldeg_vertices_begin(), mhdeg_vertices_begin());
  }

  inline vertex_iterator vertices_end()
  {
    return vertex_iterator(ldeg_vertices_end(), mhdeg_vertices_end());
  }

  inline adjacent_edge_iterator adjacent_edge_begin(const vertex_type& src_vrtx)
  {
    return adjacent_edge_iterator(ldeg_adjacent_edge_begin(src_vrtx), mhdeg_adjacent_edge_begin(src_vrtx));
  }

  inline adjacent_edge_iterator adjacent_edge_end()
  {
    return adjacent_edge_iterator(ldeg_adjacent_edge_end(), mhdeg_adjacent_edge_end());
  }

  /// a wrapper so that provide compatible interface with baseline
  inline adjacent_edge_iterator adjacent_edge_end(const vertex_type&)
  {
    return adjacent_edge_end();
  }



  /// -------- Modifiers ------- ////

  ///
  /// \brief insert_edge
  ///   inert a edge uniquely
  /// \param src
  /// \param trg
  /// \param weight
  /// \return
  ///   true: if inserted
  ///   false: if a duplicated edge is found
  bool insert_edge(const vertex_type& src, const vertex_type& trg, const edge_property_data_type& weight)
  {

    /// -- count the degree of the source vertex in the low degree table -- ///
    size_type count_in_single = 0;
    for (auto itr_single = ldeg_adjacent_edge_begin(src); !itr_single.is_end(); ++itr_single) {
      if ((*itr_single).second == trg) {
        /// --- if the same edge is found, do nothing --- ///
        return false;
      }
      ++count_in_single;
    }

    if (count_in_single > 0) { /// -- the low table has the source vertex -- ///
      if (count_in_single + 1 < middle_high_degree_threshold) {
        /// --- insert into the low table --- ///
        rhh::insert(&m_ldeg_table, src, ldeg_table_value_type(vertex_property_data_type(), trg, weight));
      } else {

        /// --- move the elements from low table to high-mid table --- ///
        mhdeg_edge_chunk_type* adj_list = mhdeg_edge_chunk_type::allocate(middle_high_degree_threshold * 2);
        auto itr_single = m_ldeg_table->find(src);
        mhdeg_table_value_type value((*itr_single).first, nullptr);
        for (; !itr_single.is_end(); ++itr_single) {
          rhh::insert(&adj_list, (*itr_single).second, (*itr_single).third);
          m_ldeg_table->erase(itr_single);
        }
        rhh::insert(&adj_list, trg, weight);
        value.second = adj_list;
        rhh::insert(&m_mhdeg_table, src, value);
        /// rhh::shrink_to_fit(&m_ldeg_table);
      }

    } else {
      auto itr_src = m_mhdeg_table->find(src);
      if (itr_src.is_end()) {
        /// --- since the high-mid table dosen't have the vertex, insert into the low table (new vertex) --- ///
        ldeg_table_value_type value(vertex_property_data_type(), trg, weight);
        rhh::insert(&m_ldeg_table, src, value);
      } else {
        /// --- the high-mid table has source vertex --- ///
        mhdeg_edge_chunk_type* adj_list = itr_src->second;
        auto itr_trg = adj_list->find(trg);

        if (itr_trg.is_end()) {
          /// --- insert the edge --- ///
          rhh::insert(&adj_list, trg, weight);
          itr_src->second = adj_list;
        } else {
          /// --- if the same edge is found, do nothing --- ///
          return false;
        }
      }

    }

    /// TODO: insert the target vertex into the source vertex list
    ++m_num_edges;
    return true;
  }

  /// allows duplicated insertion
  void insert_edge_dup(const vertex_type& src, const vertex_type& trg, const edge_property_data_type& weight)
  {

    /// -- count the degree of the source vertex in the low degree table -- ///
    const size_type count_in_single = count_degree_ldeg_table(src);

    if (count_in_single > 0) { /// -- the low table has the source vertex -- ///
      if (count_in_single + 1 < middle_high_degree_threshold) {
        /// --- insert into the low table --- ///
        ldeg_table_value_type value(vertex_property_data_type(), trg, weight);
        rhh::insert(&m_ldeg_table, src, value);
      } else {

        /// --- move the elements from low table to high-mid table --- ///
        mhdeg_edge_chunk_type* adj_list = mhdeg_edge_chunk_type::allocate(middle_high_degree_threshold * 2);
        auto itr_single = m_ldeg_table->find(src);
        mhdeg_table_value_type value((*itr_single).first, nullptr);
        for (; !itr_single.is_end(); ++itr_single) {
          rhh::insert(&adj_list, (*itr_single).second, (*itr_single).third);
          m_ldeg_table->erase(itr_single);
        }
        rhh::insert(&adj_list, trg, weight);
        value.second = adj_list;
        rhh::insert(&m_mhdeg_table, src, value);
      }

    } else {
      auto itr_src = m_mhdeg_table->find(src);
      if (itr_src.is_end()) {
        /// --- since the high-mid table dosen't have the vertex, insert into the low table (new vertex) --- ///
        ldeg_table_value_type value(vertex_property_data_type(), trg, weight);
        rhh::insert(&m_ldeg_table, src, value);
      } else {
        /// --- the high-mid table has source vertex --- ///
        mhdeg_edge_chunk_type* adj_list = itr_src->second;
        /// --- insert the edge without check duplication --- ///
        rhh::insert(&adj_list, trg, weight);
        itr_src->second = adj_list;
      }

    }

    /// TODO: insert the target vertex into the source vertex list
    ++m_num_edges;
  }

  ///
  /// \brief erase_edge
  ///         erase a edge
  /// \param src
  /// \param trg
  /// \return
  ///         whether edge is erased
  bool erase_edge(const vertex_type& src, const vertex_type& trg)
  {
    for (auto itr = m_ldeg_table->find(src); !itr.is_end(); ++itr) {
      if ((*itr).second == trg) {
        m_ldeg_table->erase(itr);
        rhh::shrink_to_fit(&m_ldeg_table, 2.0);
        --m_num_edges;
        return true;
      }
    }

    auto itr_matrix = m_mhdeg_table->find(src);
    /// has source vertex ?
    if (itr_matrix.is_end()) return false;
    mhdeg_edge_chunk_type* adj_list = itr_matrix->second;
    /// has the target edge ?
    auto itr = adj_list->find(trg);
    if (itr.is_end()) return false;

    /// delete the edge
    adj_list->erase(itr);

    if (adj_list->size() < middle_high_degree_threshold) {
      const vertex_property_data_type& property_data = itr_matrix->first;
      for (auto itr = adj_list->begin(); !itr.is_end(); ++itr) {
        ldeg_table_value_type value(property_data, itr->key, itr->value);
        rhh::insert(&m_ldeg_table, src, value);
        adj_list->erase(itr);
      }
      mhdeg_edge_chunk_type::deallocate(adj_list);
      m_mhdeg_table->erase(itr_matrix);
      rhh::shrink_to_fit(&m_mhdeg_table);
    } else {
      rhh::shrink_to_fit(&adj_list);
      itr_matrix->second = adj_list;
    }

    --m_num_edges;
    return true;
  }

  ///
  /// \brief erase_edge
  ///         erase edges. this function can delete duplicated edges.
  /// \param src
  /// \param trg
  /// \return
  ///         the number of edges erased
  size_type erase_edge_dup(const vertex_type& src, const vertex_type& trg)
  {
    size_type count = 0;
    for (auto itr = m_ldeg_table->find(src); !itr.is_end(); ++itr) {
      if ((*itr).second == trg) {
        m_ldeg_table->erase(itr);
        ++count;
      }
    }
    if (count > 0) {
      rhh::shrink_to_fit(&m_ldeg_table, 2.0);
      m_num_edges -= count;
      return count;
    }

    auto itr_matrix = m_mhdeg_table->find(src);
    /// has source vertex ?
    if (itr_matrix.is_end()) return 0;
    mhdeg_edge_chunk_type* adj_list = itr_matrix->second;
    for (auto itr = adj_list->find(trg); !itr.is_end(); ++itr) {
      adj_list->erase(itr);
      ++count;
    }

    if (count > 0) {
      if (adj_list->size() < middle_high_degree_threshold) {
        const vertex_property_data_type& property_data = itr_matrix->first;
        for (auto itr = adj_list->begin(); !itr.is_end(); ++itr) {
          ldeg_table_value_type value(property_data, itr->key, itr->value);
          rhh::insert(&m_ldeg_table, src, value);
          adj_list->erase(itr);
        }
        mhdeg_edge_chunk_type::deallocate(adj_list);
        m_mhdeg_table->erase(itr_matrix);
        rhh::shrink_to_fit(&m_mhdeg_table);
      } else {
        rhh::shrink_to_fit(&adj_list);
        itr_matrix->second = adj_list;
      }
    }

    m_num_edges -= count;
    return count;
  }

  inline vertex_property_data_type& vertex_property_data(const vertex_type& vertex)
  {
    auto itr = m_ldeg_table->find(vertex);
    if (!itr.is_end()) {
      return itr->first;
    }
    auto itr_matrix = m_mhdeg_table->find(vertex);
    return itr_matrix->first;
  }

  inline edge_property_data_type& edge_property_data(const vertex_type& src, const vertex_type& trg)
  {
    for (auto itr = m_ldeg_table->find(src); !itr.is_end(); ++itr) {
      if ((*itr).second == trg) {
        return (*itr).third;
      }
    }

    auto itr_adjlist = m_mhdeg_table->find(src);
    mhdeg_edge_chunk_type* adj_list = itr_adjlist->second;
    auto edge_weight = adj_list->find(trg);
    return *edge_weight;
  }

  inline size_type degree(const vertex_type& vertex)
  {
    size_type degree = count_degree_ldeg_table(vertex);
    if (degree == 0) {
      degree = count_degree_mhdeg_table(vertex);
    }

    return degree;
  }

  inline size_type num_edges() const
  {
    return m_num_edges;
  }

  void clear()
  {
    for (const auto& itr : *m_mhdeg_table) {
      mhdeg_edge_chunk_type* const adj_list = itr.value.second;
      adj_list->clear();
    }
    m_mhdeg_table->clear();
    m_ldeg_table->clear();
  }


  /// -------- Performance Optimization ------- ////
  void opt()
  {
    rehash_ldeg_table();
  }

  void shrink_to_fit_ldeg_table()
  {
    rhh::shrink_to_fit(&m_ldeg_table);
  }

  void shrink_to_fit_mhdeg_table()
  {
    rhh::shrink_to_fit(&m_mhdeg_table);
  }

  void rehash_ldeg_table()
  {
    std::cout << "rehash_ldeg_table()" << std::endl;
    m_ldeg_table->rehash();
  }

  void rehash_mhdeg_table()
  {
    std::cout << "rehash_mhdeg_table()" << std::endl;
    m_mhdeg_table->rehash();
  }


  /// -------- Debug Helpers ------- ////

  ///
  /// \brief print_status
  ///   Note: this function accesses entier data of the rhhda containers to compute statuses
  ///         thus, this function would affect pagecache and cause I/Os
  void print_status(const int level) const
  {
    std::cout << "<low degree table>:"
              << "\n size, capacity, rate: " << m_ldeg_table->size()
              << ", " << m_ldeg_table->capacity() * m_ldeg_table->depth()
              << ", " << (double)(m_ldeg_table->size()) / (m_ldeg_table->capacity() * m_ldeg_table->depth())
              << "\n chaine depth: " << m_ldeg_table->depth()
              << "\n capacity*element_size(GB): "
              << (double)m_ldeg_table->capacity() * m_ldeg_table->depth() * ldeg_table_type::kElementSize / (1ULL<<30) << std::endl;
#if RHH_DETAILED_ANALYSYS
      m_ldeg_table->print_detailed_analysis();
#endif

    std::cout << "<high-middle degree table>: "
              << "\n size, capacity, rate: " << m_mhdeg_table->size()
              << ", " << m_mhdeg_table->capacity() * m_mhdeg_table->depth()
              << ", " << (double)(m_mhdeg_table->size()) / (m_mhdeg_table->capacity() * m_mhdeg_table->depth())
              << "\n chaine depth : " << m_mhdeg_table->depth()
              << "\n capacity*element_size(GB): "
              << (double)m_mhdeg_table->capacity() * m_mhdeg_table->depth() * mhdeg_table_type::kElementSize  / (1ULL<<30) << std::endl;
#if RHH_DETAILED_ANALYSYS
      m_mhdeg_table->print_detailed_analysis();
#endif
    if (level == 0) return;


    std::cout << "<low degree table>:"
              << "\n average probedistance: " << m_ldeg_table->load_factor()
              << std::endl;
    {
      size_type histgram_prbdist[ldeg_table_type::property_program::kLongProbedistanceThreshold] = {0};
      std::cout << "probedistance: ";
      m_ldeg_table->histgram_load_factor(histgram_prbdist);
      for (int i = 0; i < utility::array_length(histgram_prbdist); ++i) {
        std::cout << histgram_prbdist[i] << " ";
      }
      std::cout << std::endl;
    }

    std::cout << "<high-middle degree table>:"
              << "\n average probedistance: " << m_mhdeg_table->load_factor()
              << std::endl;
    {
      size_type histgram_ave_prbdist[mhdeg_table_type::property_program::kLongProbedistanceThreshold] = {0};
      size_type histgram_cap[50] = {0};
      size_type histgram_size[50] = {0};
      size_type histgram_dept[50] = {0};
      size_type size_sum = 0;
      size_type capacity_sum = 0;
      for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
        auto adj_list = itr->value.second;

        /// --- size ---- ///
        size_sum += adj_list->size();
        const size_type sz_log2 = std::min(std::log2(adj_list->size()),
                                        static_cast<double>(utility::array_length(histgram_size) - 1));
        ++histgram_size[sz_log2];

        /// --- average probe distance (load factor) ---- ///
        assert(adj_list->load_factor() < utility::array_length(histgram_ave_prbdist));
        ++histgram_ave_prbdist[static_cast<size_t>(adj_list->load_factor())];

        /// --- capacity --- ///
        capacity_sum += adj_list->capacity() * adj_list->depth();
        const size_type cap_log2 = std::min(std::log2(adj_list->capacity()),
                                        static_cast<double>(utility::array_length(histgram_cap) - 1));
        ++histgram_cap[cap_log2];

        /// --- depth --- ///
        const size_type depth = std::min(adj_list->depth(),
                                      utility::array_length(histgram_dept) - 1);
        ++histgram_dept[depth];
      }

      std::cout << "<high-middle edge-list>: "
                << "\n size: " << size_sum
                << "\n capacity: " << capacity_sum
                << "\n rate: " << (double)(size_sum) / capacity_sum
                << "\n capacity*element_size(GB): " << (double)capacity_sum * mhdeg_edge_chunk_type::kElementSize  / (1ULL<<30) << std::endl;

      double global_ave_prbdist = 0;
      std::cout << "average probedistance: ";
      for (int i = 0; i < utility::array_length(histgram_ave_prbdist); ++i) {
        std::cout << histgram_ave_prbdist[i] << " ";
        global_ave_prbdist += histgram_ave_prbdist[i] * i;
      }
      std::cout << std::endl;
      std::cout << "global average probedistance: " << global_ave_prbdist / m_mhdeg_table->size() << std::endl;

      std::cout << "capacity (log2): ";
      for (int i = 0; i < utility::array_length(histgram_cap); ++i) {
        std::cout << histgram_cap[i] << " ";
      }
      std::cout << std::endl;

      std::cout << "size (log2): ";
      for (int i = 0; i < utility::array_length(histgram_size); ++i) {
        std::cout << histgram_size[i] << " ";
      }
      std::cout << std::endl;

      std::cout << "depth: ";
      for (int i = 1; i < utility::array_length(histgram_dept); ++i) {
        std::cout << histgram_dept[i] << " ";
      }
      std::cout << std::endl;

    }
  }

  void fprint_all_elements(std::ofstream& of)
  {
    for (auto itr = m_ldeg_table->begin(); !itr.is_end(); ++itr) {
      of << itr->key << " " << (itr->value).second << "\n";
    }

    for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
      auto adj_list = itr->value.second;
      for (auto itr2 = adj_list->begin(); !itr2.is_end(); ++itr2) {
        of << itr->key << " " << itr2->key << "\n";
      }
    }
  }

  void print_all_elements_low()
  {
    std::cout << "------------------" << std::endl;
    for (auto itr = m_ldeg_table->begin(); !itr.is_end(); ++itr) {
      std::cout << itr->key << " " << (itr->value).second << "\n";
    }
  }

  void print_all_elements_mh()
  {
    std::cout << "------------------" << std::endl;
    for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
      auto adj_list = itr->value.second;
      for (auto itr2 = adj_list->begin(); !itr2.is_end(); ++itr2) {
        std::cout << itr->key << " " << itr2->key << "\n";
      }
    }
  }


 private:

  /// --- Count edges ---- ///
  size_type count_degree_ldeg_table(const vertex_type& vertex)
  {
    size_type degree = 0;
    for (auto itr = m_ldeg_table->find(vertex); !itr.is_end(); ++itr) {
      ++degree;
    }
    return degree;
  }

  inline size_type count_degree_mhdeg_table(const vertex_type& vertex)
  {
    auto itr_matrix = m_mhdeg_table->find(vertex);
    if (!itr_matrix.is_end()) {
      const mhdeg_edge_chunk_type* const adj_list = itr_matrix->second;
      return adj_list->size();
    }
    return 0;
  }

  /// --- Iterator supporting functions --- ///

  /// TODO: this function has a bug
  inline typename ldeg_table_type::whole_iterator ldeg_vertices_begin()
  {
    assert(middle_high_degree_threshold <= 1);
    return m_ldeg_table->begin();
  }

  inline typename ldeg_table_type::whole_iterator ldeg_vertices_end()
  {
    return m_ldeg_table->end();
  }

  inline typename mhdeg_table_type::whole_iterator mhdeg_vertices_begin()
  {
    return m_mhdeg_table->begin();
  }

  inline typename mhdeg_table_type::whole_iterator mhdeg_vertices_end()
  {
    return m_mhdeg_table->end();
  }

  inline typename ldeg_table_type::value_iterator ldeg_adjacent_edge_begin (const vertex_type& src_vrt)
  {
    return m_ldeg_table->find(src_vrt);
  }

  inline static typename ldeg_table_type::value_iterator ldeg_adjacent_edge_end ()
  {
    return ldeg_table_type::find_end();
  }

  inline typename mhdeg_edge_chunk_type::whole_iterator mhdeg_adjacent_edge_begin (const vertex_type& src_vrt)
  {
    const auto itr_matrix = m_mhdeg_table->find(src_vrt);
    if (!itr_matrix.is_end()) {
      mhdeg_edge_chunk_type* adj_list = itr_matrix->second;
      return adj_list->begin();
    } else {
      return mhdeg_edge_chunk_type::end();
    }
  }

  inline static typename mhdeg_edge_chunk_type::whole_iterator mhdeg_adjacent_edge_end ()
  {
    return mhdeg_edge_chunk_type::end();
  }


  ldeg_table_type* m_ldeg_table;
  mhdeg_table_type* m_mhdeg_table;
  size_type m_num_edges;
};


///
/// \brief The degawarerhh<vertex_type, vertex_property_data_type, edge_property_data_type, 1> class
///   partial speciallization class when middle_high_degree_threshold is 1
//template <typename vertex_type,
//          typename vertex_property_data_type,
//          typename edge_property_data_type,
//          typename segment_manager_type>
//class degawarerhh <vertex_type, vertex_property_data_type, edge_property_data_type, segment_manager_type, 1>
//{
// private:
//  using size_type = size_t;
//  using ldeg_table_value_type    = utility::packed_tuple<vertex_property_data_type, vertex_type, edge_property_data_type, void>;
//  using ldeg_table_type          = rhh_container<vertex_type, ldeg_table_value_type, size_type, segment_manager_type>;
//  using mhdeg_edge_chunk_type       = rhh_container<vertex_type, edge_property_data_type, size_type, segment_manager_type>;
//  using mhdeg_table_value_type = utility::packed_pair<vertex_property_data_type, mhdeg_edge_chunk_type*>;
//  using mhdeg_table_type     = rhh_container<vertex_type, mhdeg_table_value_type, size_type, segment_manager_type>;


// public:

//  explicit degawarerhh(segment_manager_type* segment_manager) :
//    m_ldeg_table(nullptr),
//    m_mhdeg_table(nullptr),
//    m_num_edges(0)
//  {
//    // -- init allocator -- //
//    rhh::init_allocator<typename ldeg_table_type::allocator, segment_manager_type>(segment_manager);
//    rhh::init_allocator<typename mhdeg_edge_chunk_type::allocator, segment_manager_type>(segment_manager);
//    rhh::init_allocator<typename mhdeg_table_type::allocator, segment_manager_type>(segment_manager);

//    m_ldeg_table = ldeg_table_type::allocate(2);
//    m_mhdeg_table = mhdeg_table_type::allocate(2);

//    std::cout << "Element size: \n"
//              << " ldeg_table = " << ldeg_table_type::kElementSize << "\n"
//              << " mh_edge_chunk = " << mhdeg_edge_chunk_type::kElementSize << "\n"
//              << " mhdeg_table = " << mhdeg_table_type::kElementSize << std::endl;
//    std::cout << "middle_high_degree_threshold (using only m_h table) = " << 0 << std::endl;
//  }

//  ~degawarerhh()
//  {
//    clear();
//    ldeg_table_type::deallocate(m_ldeg_table);
//    mhdeg_table_type::deallocate(m_mhdeg_table);
//    rhh::destroy_allocator<typename mhdeg_edge_chunk_type::allocator>();
//    rhh::destroy_allocator<typename mhdeg_table_type::allocator>();
//  }

//  void opt()
//  {
//  }

//  void shrink_to_fit_ldeg_table()
//  {
//    rhh::shrink_to_fit(&m_ldeg_table);
//  }

//  void shrink_to_fit_mhdeg_table()
//  {
//    rhh::shrink_to_fit(&m_mhdeg_table);
//  }

//  void rehash_ldeg_table()
//  {
//    std::cout << "rehash_ldeg_table()" << std::endl;
//    m_ldeg_table->rehash();
//  }

//  void rehash_mhdeg_table()
//  {
//    std::cout << "rehash_mhdeg_table()" << std::endl;
//    m_mhdeg_table->rehash();
//  }

//  ///
//  /// \brief insert_edge
//  ///   inert a edge uniquely
//  /// \param src
//  /// \param trg
//  /// \param weight
//  /// \return
//  ///   true: if inserted
//  ///   false: if a duplicated edge is found
//  ///
//  bool insert_edge(const vertex_type& src, const vertex_type& trg, const edge_property_data_type& weight)
//  {


//    auto itr_src = m_mhdeg_table->find(src);
//    if (itr_src.is_end()) {
//      /// --- new vertex --- ///
//      mhdeg_edge_chunk_type* adj_list = mhdeg_edge_chunk_type::allocate(2);
//      rhh::insert(&adj_list, trg, weight);
//      mhdeg_table_value_type value(vertex_property_data_type(), adj_list);
//      rhh::insert(&m_mhdeg_table, src, value);
//    } else {
//      /// --- high-mid table has source vertex --- ///
//      mhdeg_edge_chunk_type* adj_list = itr_src->second;
//      auto itr_trg = adj_list->find(trg);

//      if (itr_trg.is_end()) {
//        /// --- insert the edge --- ///
//        rhh::insert(&adj_list, trg, weight);
//        itr_src->second = adj_list;
//      } else {
//        /// --- if the same edge is found, do nothing --- ///
//        return false;
//      }

//    }

//    ++m_num_edges;
//    return true;
//  }


//  inline bool insert_vertex(const vertex_type& vertex, const vertex_property_data_type& property_data)
//  {
//    auto itr = m_mhdeg_table->find(vertex);
//    if (itr.is_end()) {
//      mhdeg_table_value_type value(property_data, nullptr);
//      rhh::insert(&m_mhdeg_table, vertex, value);
//      return true;
//    }
//    return false;
//  }


//  ///
//  /// \brief erase_edge
//  ///         erase edges. this function can delete duplicated edges.
//  /// \param src
//  /// \param trg
//  /// \return
//  ///         the number of edges erased
//  size_type erase_edge(const vertex_type& src, const vertex_type& trg)
//  {
//    size_type count = 0;

//    auto itr_matrix = m_mhdeg_table->find(src);
//    /// has source vertex ?
//    if (itr_matrix.is_end()) return false;
//    mhdeg_edge_chunk_type* adj_list = itr_matrix->second;

//    for (auto itr = adj_list->find(trg); !itr.is_end(); ++itr) {
//      adj_list->erase(itr);
//      ++count;
//    }

//    if (count > 0) {
//      if (adj_list->size() == 0) {
//        mhdeg_edge_chunk_type::deallocate(adj_list);
//        m_mhdeg_table->erase(itr_matrix);
//        /// rhh::shrink_to_fit(&m_mhdeg_table);
//      }
//    }

//    m_num_edges -= count;
//    return count;
//  }

////  size_type erase_vertex(const vertex_type& vertex)
////  {
////    return m_mhdeg_table->erase(vertex);
////  }

//  vertex_property_data_type& vertex_property_data(const vertex_type& vertex)
//  {
//    auto itr_matrix = m_mhdeg_table->find(vertex);
//    return itr_matrix->first;
//  }

//  inline size_type num_edges() const
//  {
//    return m_num_edges;
//  }

//  void clear()
//  {
//    // for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr)
//    for (const auto& itr : *m_mhdeg_table) {
//      mhdeg_edge_chunk_type* const adj_list = itr.value.second;
//      adj_list->clear();
//    }
//  }

//  ///
//  /// \brief print_status
//  ///   Note: this function accesses entier data of the rhhda containers to compute statuses
//  ///         thus, this function would affect pagecache and cause I/Os
//  void print_status(const int level) const
//  {

//    std::cout << "<high-middle degree table>: "
//              << "\n size, capacity, rate: " << m_mhdeg_table->size()
//              << ", " << m_mhdeg_table->capacity() * m_mhdeg_table->depth()
//              << ", " << (double)(m_mhdeg_table->size()) / (m_mhdeg_table->capacity() * m_mhdeg_table->depth())
//              << "\n chaine depth : " << m_mhdeg_table->depth()
//              << "\n capacity*element_size(GB): "
//              << (double)m_mhdeg_table->capacity() * m_mhdeg_table->depth() * mhdeg_table_type::kElementSize  / (1ULL<<30) << std::endl;
//    if (level == 0) return;

//    std::cout << "<high-middle degree table>:"
//              << "\n average probedistance: " << m_mhdeg_table->load_factor()
//              << std::endl;
//    {
//      size_type histgram_ave_prbdist[mhdeg_table_type::property_program::kLongProbedistanceThreshold] = {0};
//      size_type histgram_cap[50] = {0};
//      size_type histgram_size[50] = {0};
//      size_type histgram_dept[50] = {0};
//      size_type size_sum = 0;
//      size_type capacity_sum = 0;
//      for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
//        auto adj_list = itr->value.second;

//        /// --- size ---- ///
//        size_sum += adj_list->size();
//        const size_type sz_log2 = std::min(std::log2(adj_list->size()),
//                                        static_cast<double>(utility::array_length(histgram_size) - 1));
//        ++histgram_size[sz_log2];

//        /// --- average probe distance (laod factor) ---- ///
//        assert(adj_list->load_factor() < utility::array_length(histgram_ave_prbdist));
//        ++histgram_ave_prbdist[static_cast<size_t>(adj_list->load_factor())];

//        /// --- capacity --- ///
//        capacity_sum += adj_list->capacity() * adj_list->depth();
//        const size_type cap_log2 = std::min(std::log2(adj_list->capacity()),
//                                        static_cast<double>(utility::array_length(histgram_cap) - 1));
//        ++histgram_cap[cap_log2];

//        /// --- depth --- ///
//        const size_type depth = std::min(adj_list->depth(),
//                                      utility::array_length(histgram_dept) - 1);
//        ++histgram_dept[depth];
//      }

//      std::cout << "<high-middle edge chunks>: "
//                << "\n size: " << size_sum
//                << "\n capacity: " << capacity_sum
//                << "\n rate: " << (double)(size_sum) / capacity_sum
//                << "\n capacity*element_size(GB): " << (double)capacity_sum * mhdeg_edge_chunk_type::kElementSize  / (1ULL<<30) << std::endl;

//      std::cout << "average probedistance: ";
//      for (int i = 0; i < utility::array_length(histgram_ave_prbdist); ++i) {
//        std::cout << histgram_ave_prbdist[i] << " ";
//      }
//      std::cout << std::endl;

//      std::cout << "capacity (log2): ";
//      for (int i = 0; i < utility::array_length(histgram_cap); ++i) {
//        std::cout << histgram_cap[i] << " ";
//      }
//      std::cout << std::endl;

//      std::cout << "size (log2): ";
//      for (int i = 0; i < utility::array_length(histgram_size); ++i) {
//        std::cout << histgram_size[i] << " ";
//      }
//      std::cout << std::endl;

//      std::cout << "depth: ";
//      for (int i = 1; i < utility::array_length(histgram_dept); ++i) {
//        std::cout << histgram_dept[i] << " ";
//      }
//      std::cout << std::endl;

//    }
//  }

//  void fprint_all_elements(std::ofstream& of)
//  {
//    for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
//      auto adj_list = itr->value.second;
//      for (auto itr2 = adj_list->begin(); !itr2.is_end(); ++itr2) {
//        of << itr->key << " " << itr2->key << "\n";
//      }
//    }
//  }

//  void print_all_elements_low()
//  {
//    std::cout << "------------------" << std::endl;
//  }

//  void print_all_elements_mh()
//  {
//    std::cout << "------------------" << std::endl;
//    for (auto itr = m_mhdeg_table->begin(); !itr.is_end(); ++itr) {
//      auto adj_list = itr->value.second;
//      for (auto itr2 = adj_list->begin(); !itr2.is_end(); ++itr2) {
//        std::cout << itr->key << " " << itr2->key << "\n";
//      }
//    }
//  }


// private:
////  inline typename ldeg_table_type::whole_iterator begin_low_edges()
////  {
////    return ldeg_table_type::end();
////  }

////  inline typename mhdeg_table_type::whole_iterator begin_mh_edges()
////  {
////    return m_mhdeg_table->begin();
////  }

//  inline typename ldeg_table_type::value_iterator find_low_edge (const vertex_type& src_vrt)
//  {
//    return ldeg_table_type::find_end();
//  }

//  inline typename ldeg_table_type::const_value_iterator find_low_edge (const vertex_type& src_vrt) const
//  {
//    return m_ldeg_table->find(src_vrt);
//  }

//  inline typename mhdeg_edge_chunk_type::whole_iterator find_mh_edge (const vertex_type& src_vrt)
//  {
//    const auto itr_matrix = m_mhdeg_table->find(src_vrt);
//    mhdeg_edge_chunk_type* const adj_list = itr_matrix->second;
//    return adj_list->begin();
//  }

//  inline typename mhdeg_edge_chunk_type::const_whole_iterator find_mh_edge (const vertex_type& src_vrt) const
//  {
//    const auto itr_matrix = m_mhdeg_table->find(src_vrt);
//    const mhdeg_edge_chunk_type* const adj_list = itr_matrix->second;
//    return adj_list->cbegin();
//  }


//  ldeg_table_type* m_ldeg_table;
//  mhdeg_table_type* m_mhdeg_table;
//  size_type m_num_edges;
//};

}

#include <havoqgt/graphstore/degawarerhh/degawarerhh_vertex_iterator.hpp>
#include <havoqgt/graphstore/degawarerhh/degawarerhh_adjacent_edge_iterator.hpp>

#endif // DEGAWARERHH_HPP
