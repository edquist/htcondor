#ifndef __RANGER_H__
#define __RANGER_H__

#include <set>

template <class T>
struct ranger {
    struct range;
    struct elements;
    typedef T                                    element_type;
    typedef std::set<range>                      forest_type;
    typedef typename forest_type::const_iterator iterator;

    ranger() {};
    ranger(const std::initializer_list<range> &il);
    ranger(const std::initializer_list<element_type> &il);

    iterator insert(range r);
    iterator erase(range r);

    inline void insert(element_type e);
    inline void erase(element_type e);

    inline void insert_slice(element_type front, element_type back);
    inline void erase_slice(element_type front, element_type back);

    iterator lower_bound(element_type x) const;
    iterator upper_bound(element_type x) const;

    std::pair<iterator, bool> find(element_type x) const;

    bool contains(element_type x) const;
    bool empty()                  const { return forest.empty(); }
    size_t size()                 const { return forest.size(); }
    void clear()                        { forest.clear(); }

    inline iterator begin() const;
    inline iterator end()   const;

    inline elements get_elements() const;

    // first/final range/element; do not call if empty()
    inline const range &front()         const;
    inline const range &back()          const;
    inline element_type front_element() const;
    inline element_type back_element()  const;

    private:
    // the state of our ranger
    forest_type forest;
};

typedef ranger<int> range_mask;

template <class T>
struct ranger<T>::range {
    struct iterator;
    typedef ranger::element_type value_type;

    range(value_type e) : _start(0), _end(e) {}
    range(value_type s, value_type e) : _start(s), _end(e) {}

    value_type back()             const { return _end - 1; }
    value_type size()             const { return _end - _start; }
    bool contains(value_type x)   const { return _start <= x && x < _end; }
    bool contains(const range &r) const
    { return _start <= r._start && r._end < _end; }

    // only for use in our disjoint ranger forest context
    bool operator< (const range &r2) const { return _end < r2._end; }

    inline iterator begin() const;
    inline iterator end()   const;

    // data members; a valid range in ranger forest context has _start < _end
    mutable value_type _start;
    mutable value_type _end;
};

template <class T>
struct ranger<T>::range::iterator {
    typedef ranger::element_type value_type;

    iterator() : i(0) {}
    iterator(value_type n) : i(n) {}

    value_type  operator*()             const {      return i;     }
    iterator    operator+(value_type n) const {      return i+n;   }
    iterator    operator-(value_type n) const {      return i-n;   }
    iterator   &operator++()                  { ++i; return *this; }
    iterator   &operator--()                  { --i; return *this; }
    iterator    operator++(int)               {      return i++;   }
    iterator    operator--(int)               {      return i--;   }

    // takes care of rel ops :D
    operator value_type()               const {      return i;     }

    // this is both the iterator "position" and the value
    value_type i;
};

template <class T>
struct ranger<T>::elements {
    struct iterator;
    typedef ranger::element_type value_type;

    elements(const ranger &r) : r(r) {}

    inline iterator begin() const;
    inline iterator end()   const;

    const ranger &r;
};

template <class T>
struct ranger<T>::elements::iterator {
    iterator(ranger::iterator si) : sit(si), rit_valid(0) {}
    iterator() : rit_valid(0) {}

    value_type operator*();
    iterator &operator++();
    iterator &operator--();
    bool operator==(iterator &it);
    bool operator!=(iterator &it);

    private:
    void mk_valid();

    typename ranger::iterator sit;
    typename range::iterator rit;
    bool rit_valid;
};


// these are inline but must appear after their return type definitions

template <class T> typename ranger<T>::elements
ranger<T>::get_elements()    const { return *this; }

template <class T> typename ranger<T>::elements::iterator
ranger<T>::elements::begin() const { return r.begin(); }

template <class T> typename ranger<T>::elements::iterator
ranger<T>::elements::end()   const { return r.end();   }

template <class T> typename ranger<T>::range::iterator
ranger<T>::range::begin()    const { return _start; }

template <class T> typename ranger<T>::range::iterator
ranger<T>::range::end()      const { return _end;   }

template <class T> typename ranger<T>::iterator
ranger<T>::begin()   const { return forest.begin(); }

template <class T> typename ranger<T>::iterator
ranger<T>::end()     const { return forest.end();   }

template <class T> const typename ranger<T>::range &
ranger<T>::front()         const { return *begin();       }

template <class T> typename ranger<T>::element_type
ranger<T>::front_element() const { return front()._start; }

template <class T> const typename ranger<T>::range &
ranger<T>::back()          const { return *--end();       }

template <class T> typename ranger<T>::element_type
ranger<T>::back_element()  const { return back().back();  }

template <class T>
void ranger<T>::insert(element_type e) { insert(range(e, e + 1)); }
template <class T>
void ranger<T>::erase(element_type e)  { erase(range(e, e + 1));  }

template <class T>
void ranger<T>::insert_slice(element_type front, element_type back)
{ insert(range(front, back + 1)); }

template <class T>
void ranger<T>::erase_slice(element_type front, element_type back)
{ erase(range(front, back + 1)); }


/*  persist / load ranger objects
 *
 *  The serialized format is one or more sub-ranges, separated by semicolons,
 *  where each sub-range is either N-M (for inclusive N..M) or N for a single
 *  integer.  Eg, "2", "5-10", "4;7;10-20;44;50-60"
 */

#include <string>

template <class T>
void persist(std::string &s, const ranger<T> &r);

template <class T>
void persist_slice(std::string &s, const ranger<T> &r, int start, int back);

template <class T>
void persist_range(std::string &s, const ranger<T> &r,
                                   const typename ranger<T>::range &rr);

// return 0 on success, (-1 - (position in string)) on parse failure
template <class T>
int load(ranger<T> &r, const char *s);


#endif
