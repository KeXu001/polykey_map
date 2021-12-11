/*
 *  MIT License
 *
 *  Copyright (c) 2020 Kevin Xu
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#pragma once

#include <optional>
#include <tuple>
#include <unordered_map>

namespace xu
{
  /**
    @brief  Many-to-one container class
            This class implements a container whose behavior is defined as
            follows:
              - The container stores values of a single type.
              - To access a stored value, the user specifies a 'path' and a
                valid key for that path.
              - Each stored value is retrievable by at least one path/key pair.
              - Each path/key pair 'points to' exactly one stored value.
              - A stored value may be retrievable by up to N path/key pairs,
                where N is the number of paths.
              - If a stored value is retrievable by multiple path/key pairs,
                the paths must be different (i.e. max one key per path per 
                value).
              - The number and types of the paths are limited and known at
                compile time.
              - Within a path, keys are unique.
              - Removal of a value implies removal of all keys which point to 
                that value.
            This is achieved by linking keys to an intermediate key, and also
            linking keys together which point to the same value, similar to:
              key  <----\
              key  <-----+----->  intermediate key  ----->  value
              key  <----/
            The container is analogous to a relational database table with N 
            nullable columns (representing the paths) plus a column for the 
            stored value.
    @note   There is a possibility that, after enough insertions, the 64-bit
            intermediate key ink_cnt wraps around to its starting value, after
            which we may end up with duplicate intermediate keys. We will simply
            assume that we will never reach this limit, and throw an error if
            we ever do.
    @note   The implementation uses `std::optional`, so C++17 support is
            required.
    @tparam Value_T
            Type of the stored values. Should be copy constructible.
    @tparam Path_Ts
            Each path's type. Should be copy constructible.
    */
  template <typename Value_T, typename ...Path_Ts>
  class polykey_map
  {
  protected:
    //  ========
    //  Typedefs
    //  ========

    /**
      @brief  Path index type
      */
    using path_index_t = size_t;

    /**
      @brief  Returns a path's type
      @tparam P
              Path index
      */
    template <path_index_t P>
    using Path_T = typename std::tuple_element<P, std::tuple<Path_Ts...>>::type;

    /**
      @brief  The number of different paths
      */
    static const path_index_t N_Paths = sizeof...(Path_Ts);

    /**
      @brief  Type used for the intermediate key
      @note   See note above about wrapping around of intermediate key before
              changing this type
      */
    using intermediate_key_t = unsigned long long;

    /**
      @brief  Initial value for intermediate key
      @note   Doesn't really matter but should be initialized to something
      */
    static const intermediate_key_t ink_cnt_init_val = 0;

    /**
      @brief  A collection of linked keys which point to the same value
      */
    class keyset_t
    {
    protected:
      //  ----------------
      //  Member Variables
      //  ----------------

      /**
        @brief  Linked keys
                If non-null, key is valid
        */
      std::tuple<std::optional<Path_Ts>...> keys;

      /**
        @brief  The linked intermediate key
        */
      const intermediate_key_t ink;

    public:
      //  -------
      //  Get/Set
      //  -------

      /**
        @brief  Set a key
        @note   Overwrites existing
        */
      template <path_index_t P>
      void set(const Path_T<P>& key)
      {
        std::get<P>(keys).emplace(key);
      }

      /**
        @brief  Clear a key
        */
      template <path_index_t P>
      void clear()
      {
        std::get<P>(keys).reset();
      }

      /**
        @brief  Checks if a key is set
        */
      template <path_index_t P>
      bool has_value() const
      {
        return std::get<P>(keys).has_value();
      }

      /**
        @brief  Returns copy of key
        @note   Must only be used after checking has_value() is true. Otherwise,
                behavior is not defined
        */
      template <path_index_t P>
      Path_T<P> get() const
      {
        return *std::get<P>(keys);
      }

      /**
        @brief  Returns copy of intermediate key
        */
      intermediate_key_t get_ink() const
      {
        return ink;
      }

    public:
      /**
        @brief  Construct keyset with intermediate key
                Intermediate key does not change for the life of the keyset
        */
      keyset_t(intermediate_key_t ink_)
        : ink(ink_)
      {}

    protected:
      /**
        @brief  Helper function to copy keys from another keyset
        */
      template <path_index_t P = 0>
      inline typename std::enable_if<P != N_Paths, void>::type _copy(const keyset_t& other)
      {
        static_assert(P < N_Paths);

        /* clear current key of this */
        clear<P>();

        /* create copy of other's key */
        if (other.has_value<P>())
        {
          set<P>(other.get<P>());
        }

        _copy<P + 1>(other);
      }

      template <path_index_t P = 0>
      inline typename std::enable_if<P == N_Paths, void>::type _copy(const keyset_t& other)
      {}

    public:
      /**
        @brief  Copy constructor
        */
      keyset_t(const keyset_t& other)
        : ink(other.ink)
      {
        _copy(other);
      }

      /**
        @brief  Copy assignment
        @note   Deleted because ink is const
        */
      keyset_t& operator=(const keyset_t& other) = delete;

      /**
        @brief  Move constructor
        @note   Deleted because ink is const
        */
      keyset_t(keyset_t&& other) = delete;

      /**
        @brief  Move assignment
        @note   Deleted because ink is const
        */
      keyset_t& operator=(keyset_t&& other) = delete;
    };

    /**
      @brief  Item type stored in ink_to_val
      */
    using ink_value_pair = std::pair<intermediate_key_t, Value_T>;

    /**
      @brief  Item type stored in ink_to_keys
      */
    using ink_keyset_pair = std::pair<intermediate_key_t, keyset_t>;

    /**
      @brief  Item type stored in key_to_ink
      */
    template <path_index_t P>
    using key_ink_pair = std::pair<Path_T<P>, intermediate_key_t>;

    /**
      @brief  Error type thrown when inserting or linking keys
      */
    class key_conflict_error : public std::runtime_error
    {
    public:
      explicit key_conflict_error(const std::string& what_arg)
        : std::runtime_error(what_arg)
      {}
    };

  public:
    //  =========
    //  Iterators
    //  =========

    /**
      @brief  Iterator for looping through all stored values.
              Dereferencing gives a single `Value_T`.
              In order to get the key, the user may call `has_key<P>()` and
              `get_key<P>()`
      @tparam Und_T
              Type of the underlying iterator, which can be `iterator` or
              `const_iterator`
      @tparam Deref_T
              Type of the dereferenced value, which can be `Value_T` or
              `const Value_T`
      */
    template <typename Und_T, typename Deref_T>
    class value_iterator_base
    {
      friend polykey_map;

    protected:
      /**
        @brief  A pointer to the associated polykey_map, so that keys can be
                retrieved.
        */
      const polykey_map* pk; 

      /**
        @brief  The underlying iterator for value access
                Our iterator delegates behavior to this underlying iterator. The
                only difference is that we return only the value (not a pair)
        */
      Und_T underlying;

    public:
      /**
        @brief  Construct iterator with underlying
        */
      value_iterator_base(const polykey_map* pk_, Und_T underlying_)
        : pk(pk_),
          underlying(underlying_)
      {}

      /**
        @brief  Copy constructor
        */
      value_iterator_base(const value_iterator_base& other)
        : pk(other.pk),
          underlying(other.underlying)
      {}

      /**
        @brief  Assignment
        */
      value_iterator_base& operator=(const value_iterator_base& other)
      {
        pk = other.pk;
        underlying = other.underlying;
        return *this;
      }

      /**
        @brief  Prefix increment
        */
      value_iterator_base& operator++()
      {
        underlying++;
        return *this;
      }

      /**
        @brief  Postfix increment
        */
      value_iterator_base operator++(int)
      {
        value_iterator_base res = *this;
        operator++();
        return res;
      }

      /**
        @brief  Equality
        */
      bool operator==(const value_iterator_base& other) const
      {
        return underlying == other.underlying;
      }

      /**
        @brief  Inequality
        */
      bool operator!=(const value_iterator_base& other) const
      {
        return underlying != other.underlying;
      }

      /**
        @brief  Dereference
        */
      Deref_T& operator*() const
      {
        return underlying->second;
      }

      /**
        @brief  Arrow operator
        */
      Deref_T* operator->() const
      {
        return &underlying->second;
      }

      /**
        @brief  Conversion from iterator to const_iterator
        */
      template <typename Und_T_new>
      operator value_iterator_base<Und_T_new, const Deref_T>() const
      {
        return value_iterator_base<Und_T_new, const Deref_T>(pk, underlying);
      }

      /**
        @brief  Check if a key is set for path
        @tparam P
                Path index
        */
      template <path_index_t P>
      bool has_key() const
      {
        intermediate_key_t ink = underlying->first;

        return pk->ink_to_keys.at(ink).template has_value<P>();
      }

      /**
        @brief  Return key for path
        @tparam P
                Path index
        */
      template <path_index_t P>
      const Path_T<P> get_key() const
      {
        intermediate_key_t ink = underlying->first;

        return pk->ink_to_keys.at(ink).template get<P>();
      }
    };

    using value_iterator = value_iterator_base<typename std::unordered_map<intermediate_key_t, Value_T>::iterator, Value_T>;
    using const_value_iterator = value_iterator_base<typename std::unordered_map<intermediate_key_t, Value_T>::const_iterator, const Value_T>;

    /**
      @brief  Returns a value_iterator pointing to the beginning.
              Used when no template parameter is provided
      */
    value_iterator begin()
    {
      return value_iterator(this, ink_to_val.begin());
    }

    /**
      @brief  Returns a value_iterator pointing to the end.
              Used when no template parameter is provided
      */
    value_iterator end()
    {
      return value_iterator(this, ink_to_val.end());
    }

    /**
      @brief  Returns a const_value_iterator pointing to the beginning.
              Used when no template parameter is provided
      */
    const_value_iterator cbegin() const
    {
      return const_value_iterator(this, ink_to_val.cbegin());
    }

    /**
      @brief  Returns a const_value_iterator pointing to the end.
              Used when no template parameter is provided
      */
    const_value_iterator cend() const
    {
      return const_value_iterator(this, ink_to_val.cend());
    }

  public:
    //  ======================
    //  Constructor/Destructor
    //  ======================

    /**
      @brief  Default constructor
      */
    polykey_map()
      : ink_cnt(ink_cnt_init_val)
    {}

    /**
      @brief  Free memory on destruction
      */
    ~polykey_map()
    {
      for (auto& it : ink_to_keys)
      {
        _erase(it.second);
      }
    }

    //  ===========
    //  Copy & Move
    //  ===========

    polykey_map(const polykey_map& other)
      : ink_cnt(other.ink_cnt),
        ink_to_val(other.ink_to_val),
        ink_to_keys(other.ink_to_keys),
        key_to_ink(other.key_to_ink)
    {

    }

    polykey_map& operator=(const polykey_map& other)
    {
      ink_cnt = other.ink_cnt;

      ink_to_val = other.ink_to_val;
      ink_to_keys = other.ink_to_keys;
      key_to_ink = other.key_to_ink;

      return *this;
    }

    polykey_map(polykey_map&& other)
      : ink_cnt(other.ink_cnt),
        ink_to_val(std::move(other.ink_to_val)),
        ink_to_keys(std::move(other.ink_to_keys)),
        key_to_ink(std::move(other.key_to_ink))
    {
      other.ink_cnt = ink_cnt_init_val;
    }

    polykey_map& operator=(const polykey_map&& other)
    {
      ink_cnt = other.ink_cnt;
      other.ink_cnt = ink_cnt_init_val;

      ink_to_val = std::move(other.ink_to_val);
      ink_to_keys = std::move(other.ink_to_keys);
      key_to_ink = std::move(other.key_to_ink);

      return *this;
    }

    //  ==================
    //  Container Behavior
    //  ==================

    /**
      @brief  Returns total number of stored values
      */
    size_t size() const
    {
      return ink_to_val.size();
    }

    /**
      @brief  Returns number of valid keys for a path
      @tparam P
              Path index
      */
    template <path_index_t P>
    size_t size() const
    {
      return std::get<P>(key_to_ink).size();
    }

    /**
      @brief  Insert a new value
      @tparam P
              Path index
      @param  key
              Key for path
      @param  value
              Value to insert
      @throw  xu::polykey_map::key_conflict_error
              If key already exists for path
      */
    template <path_index_t P>
    void insert(const Path_T<P>& key, const Value_T& value)
    {
      static_assert(P < N_Paths);

      auto it = std::get<P>(key_to_ink).find(key);

      if (it != std::get<P>(key_to_ink).end())
      {
        throw key_conflict_error("polykey_map::insert() : key already exists for path");
      }

      /* check intermediate key isn't already taken */
      auto ink_it = ink_to_val.find(ink_cnt);

      if (ink_it != ink_to_val.end())
      {
        throw std::out_of_range("polykey_map::insert() : reached polykey_map insertion limit");
      }

      /* insert the value with intermediate key */
      ink_to_val.insert(ink_value_pair(ink_cnt, value));

      /* link key and intermediate key */
      keyset_t ks(ink_cnt);
      ks.template set<P>(key);
      
      ink_to_keys.insert(ink_keyset_pair(ink_cnt, ks));

      std::get<P>(key_to_ink).insert(key_ink_pair<P>(key, ink_cnt));

      ink_cnt++;
    }

    /**
      @brief  Retrieve a value (const-qualified)
      @tparam P
              Path index
      @param  key
              Key to get value for
      @throw  std::out_of_range
              If key does not exist
      */
    template <path_index_t P>
    const Value_T& at(const Path_T<P>& key) const
    {
      static_assert(P < N_Paths);

      /* get intermediate key */
      auto it = std::get<P>(key_to_ink).find(key);

      if (it == std::get<P>(key_to_ink).end())
      {
        throw std::out_of_range("polykey_map::at() : key does not exist for path");
      }

      intermediate_key_t ink = it->second;

      /* return value for intermediate key */
      return ink_to_val.at(ink);
    }

    /**
      @brief  Retrieve a value (by reference)
      @tparam P
              Path index
      @param  key
              Key to get value for
      @throw  std::out_of_range
              If key does not exist
      */
    template <path_index_t P>
    Value_T& at(const Path_T<P>& key)
    {
      /* delegate at() */
      return const_cast<Value_T&>(const_cast<const polykey_map&>(*this).at<P>(key));
    }

    /**
      @brief  Link two keys so they point to the same value
              Takes two keys as parameters. If only one of the keys is valid
              (points to an object), the other key is set to point to the same
              value. If both keys are valid, or if neither key is valid, then an
              error is thrown
      @tparam P1
              Path index for first key
      @tparam P2
              Path index for second key
      @param  key1
              First key
      @param  key2
              Second key
      @throw  xu::polykey_map::key_conflict_error
              If both keys already exist
      @throw  std::out_of_range
              If neither key exists
      */
    template <path_index_t P1, path_index_t P2>
    void link(const Path_T<P1>& key1, const Path_T<P2>& key2)
    {
      static_assert(P1 < N_Paths);
      static_assert(P2 < N_Paths);
      static_assert(P1 != P2);

      /* get intermediate keys */
      auto it1 = std::get<P1>(key_to_ink).find(key1);
      auto it2 = std::get<P2>(key_to_ink).find(key2);

      if (it1 == std::get<P1>(key_to_ink).end() and it2 == std::get<P2>(key_to_ink).end())
      {
        throw std::out_of_range("polykey_map::link() : keys do not exist");
      }

      if (it1 != std::get<P1>(key_to_ink).end() and it2 != std::get<P2>(key_to_ink).end())
      {
        throw key_conflict_error("polykey_map::link() : both keys already exist");
      }

      /* link key1 with existing key2 */
      if (it1 == std::get<P1>(key_to_ink).end() and it2 != std::get<P2>(key_to_ink).end())
      {
        keyset_t& ks =  ink_to_keys.at(it2->second);
        ks.template set<P1>(key1);

        std::get<P1>(key_to_ink).insert(key_ink_pair<P1>(key1, ks.get_ink()));
      }
      /* link key2 with existing key1 */
      else if (it1 != std::get<P1>(key_to_ink).end() and it2 == std::get<P2>(key_to_ink).end())
      {
        keyset_t& ks =  ink_to_keys.at(it1->second);
        ks.template set<P2>(key2);

        std::get<P2>(key_to_ink).insert(key_ink_pair<P2>(key2, ks.get_ink()));
      }
    }

    /**
      @brief  Check whether a value exists for the given key
      @tparam P
              Path index
      @param  key
              Key to check
      */
    template <path_index_t P>
    bool contains(const Path_T<P>& key) const
    {
      static_assert(P < N_Paths);

      auto it = std::get<P>(key_to_ink).find(key);

      if (it == std::get<P>(key_to_ink).end())
      {
        return false;
      }
      else
      {
        return true;
      }
    }

    /**
      @brief  Check whether a value reachable by first path is also accessible by another path
      @tparam P1
              Path index of known key
      @tparam P2
              Path index to check for linked key
      @throw  std::out_of_range
              If first key does not exist
      */
    template <path_index_t P1, path_index_t P2>
    bool is_linked(const Path_T<P1>& key) const
    {
      static_assert(P1 < N_Paths);
      static_assert(P2 < N_Paths);

      auto ink_it = std::get<P1>(key_to_ink).find(key);

      if (ink_it == std::get<P1>(key_to_ink).end())
      {
        throw std::out_of_range("polykey_map::is_linked() : key does not exist for first path");
      }

      auto keys_it = ink_to_keys.find(ink_it->second);

      return keys_it->second.template has_value<P2>();
    }

    /**
      @brief  Given a key for one path, get the linked key along another path
      @tparam P1
              Path index of key
      @tparam P2
              Path index for which to get linked key
      @throw  std::out_of_range
              If either key does not exist
      */
    template <path_index_t P1, path_index_t P2>
    Path_T<P2> convert_key(const Path_T<P1>& key) const
    {
      static_assert(P1 < N_Paths);
      static_assert(P2 < N_Paths);

      auto ink_it = std::get<P1>(key_to_ink).find(key);

      if (ink_it == std::get<P1>(key_to_ink).end())
      {
        throw std::out_of_range("polykey_map::convert_key() : key does not exist for first path");
      }

      auto keys_it = ink_to_keys.find(ink_it->second);

      if (!keys_it->second.template has_value<P2>())
      {
        throw std::out_of_range("polykey_map::convert_key() : key does not exist for second path");
      }

      return keys_it->second.template get<P2>();
    }
    
  protected:
    /**
      @brief  Helper function to iterate over keyset_t.keys
              Checks if any of keyset_t.keys are non-null and unlinks if so
      @note   std::apply was not used because we have multiple tuples which we
              want to iterate through at the same time
      @note   see stackoverflow question #1198260
      */
    template <path_index_t P = 0>
    inline typename std::enable_if<P != N_Paths, void>::type _erase(keyset_t& ks)
    {
      /*
        Here we use a static assert, instead of P < N_Paths in return type,
        simply because the '<' causes sublime to think it's another template arg
        and messes up the syntax coloring
        */
      static_assert(P < N_Paths);

      if (ks.template has_value<P>())
      {
        std::get<P>(key_to_ink).erase(ks.template get<P>());
        ks.template clear<P>();
      }

      _erase<P + 1>(ks);
    }

    template <path_index_t P = 0>
    inline typename std::enable_if<P == N_Paths, void>::type _erase(keyset_t& ks)
    {}

  public:
    /**
      @brief  Remove a value and all keys which point to it
      @tparam P
              Path index (which path key belongs to)
      @param  key
              Key to remove value for
      @throw  std::out_of_range
              If key does not exist
      */
    template <path_index_t P>
    void erase(Path_T<P> key)
    {
      static_assert(P < N_Paths);

      /* first get the intermediate key */
      auto it = std::get<P>(key_to_ink).find(key);

      if (it == std::get<P>(key_to_ink).end())
      {
        throw std::out_of_range("polykey_map::erase() : key does not exist for path");
      }

      intermediate_key_t ink = it->second;

      /* then remove linked keys */
      _erase(ink_to_keys.at(ink));

      ink_to_keys.erase(ink);

      /* finally, erase the value itself */
      ink_to_val.erase(ink);
    }

    /**
      @brief  Remove a value using an iterator
      @param  it
              Valid iterator
      */
    value_iterator erase(const value_iterator& it)
    {
      /* first get the intermediate key */
      intermediate_key_t ink = it.underlying->first;

      /* then remove linked keys */
      _erase(ink_to_keys.at(ink));

      ink_to_keys.erase(ink);

      /* finally, erase the value itself */
      auto new_underlying = ink_to_val.erase(it.underlying);

      return value_iterator(it.pk, new_underlying);
    }

  protected:
    //  ================
    //  Member Variables
    //  ================

    /**
      @brief  Intermediate key used for linking and lookup
              Increments each time a new value is inserted
      */
    intermediate_key_t ink_cnt;

    /**
      @brief  Container which actually holds stored values
      */
    std::unordered_map<intermediate_key_t, Value_T> ink_to_val;

    /**
      @brief  Keysets which contain info on all keys for a value
      */
    std::unordered_map<intermediate_key_t, keyset_t> ink_to_keys;

    /**
      @brief  Link keys to intermediate key
      */
    std::tuple<std::unordered_map<Path_Ts, intermediate_key_t>...> key_to_ink;
  };
}