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
    @tparam Value_T
            Type of the stored values. Should be copy constructible.
    @tparam Path_Ts
            Each path's type. Should be copy constructible.
    */
  template<typename Value_T, typename ...Path_Ts>
  class polykey_map
  {
  protected:
    //  ========
    //  Typedefs
    //  ========

    /**
      @brief  Returns a path's type
      @tparam P
              Path index
      */
    template<unsigned int P>
    using Path_T = typename std::tuple_element<P, std::tuple<Path_Ts...>>::type;

    /**
      @brief  The number of different paths
      */
    static const unsigned int N_Paths = sizeof...(Path_Ts);

    /**
      @brief  Type used for the intermediate key
      */
    using intermediate_key_t = unsigned long;

    /**
      @brief  A collection of linked keys which point to the same value
      */
    struct keyset_t
    {
      /**
        @brief  Linked keys
                If non-null, key is valid and points to a stored value
                (indirectly, through the intermediate key)
                If null, key is not valid
        */
      std::tuple<Path_Ts*...> keys;

      /**
        @brief  The linked intermediate key
        */
      intermediate_key_t ink;

      /**
        @brief  Helper function to initialize keys to all null
        */
      template<unsigned int P = 0>
      inline typename std::enable_if<P != N_Paths, void>::type _nullify()
      {
        static_assert(P < N_Paths);

        std::get<P>(keys) = nullptr;

        _nullify<P + 1>();
      }

      template<unsigned int P = 0>
      inline typename std::enable_if<P == N_Paths, void>::type _nullify()
      {}

      /**
        @brief  Default constructor
        */
      keyset_t(intermediate_key_t ink_)
        : ink(ink_)
      {
        _nullify();
      }
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
    template<unsigned int P>
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
    //  ========
    //  Iterator
    //  ========

    /**
      @brief  Iterator definition
              Used to iterate through stored values. Does not contain any info
              about keys.
      */
    class iterator
    {
      friend polykey_map;
    protected:
      /**
        @brief  Type of the underlying iterator
        */
      using underlying_t = typename std::unordered_map<intermediate_key_t, Value_T>::iterator;

      /**
        @brief  The underlying iterator for value access
                Our iterator delegates behavior to this underlying iterator. The
                only difference is that we return only the value (not a pair)
        */
      underlying_t underlying;

    public:
      /**
        @brief  Construct iterator with underlying
        */
      iterator(underlying_t underlying_)
        : underlying(underlying_)
      {}

      /**
        @brief  Copy constructor
        */
      iterator(const iterator& other)
        :underlying(other.underlying)
      {}

      /**
        @brief  Assignment
        */
      iterator& operator=(const iterator& other)
      {
        underlying = other.underlying;
        return *this;
      }

      /**
        @brief  Prefix increment
        */
      iterator& operator++()
      {
        underlying++;
        return *this;
      }

      /**
        @brief  Postfix increment
        */
      iterator operator++(int)
      {
        iterator res = *this;
        operator++();
        return res;
      }

      /**
        @brief  Equality
        */
      bool operator==(const iterator& other)
      {
        return underlying == other.underlying;
      }

      /**
        @brief  Inequality
        */
      bool operator!=(const iterator& other)
      {
        return underlying != other.underlying;
      }

      /**
        @brief  Dereference
        */
      Value_T& operator*()
      {
        return underlying->second;
      }

      /**
        @brief  Arrow operator
        */
      Value_T* operator->()
      {
        return &underlying->second;
      }
    };

    iterator begin()
    {
      return iterator(ink_to_val.begin());
    }

    iterator end()
    {
      return iterator(ink_to_val.end());
    }

  public:
    //  ======================
    //  Constructor/Destructor
    //  ======================

    /**
      @brief  Default constructor
      */
    polykey_map()
      : ink_cnt(0)
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

    //  ==================
    //  Container Behavior
    //  ==================

    /**
      @brief  Returns total number of stored values
      */
    size_t size()
    {
      return ink_to_val.size();
    }

    /**
      @brief  Returns number of valid keys for a path
      @tparam P
              Path index
      */
    template<unsigned int P>
    size_t size()
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
    template<unsigned int P>
    void insert(Path_T<P> key, Value_T value)
    {
      static_assert(P < N_Paths);

      auto it = std::get<P>(key_to_ink).find(key);

      if (it != std::get<P>(key_to_ink).end())
      {
        throw key_conflict_error("polykey_map::insert() : key already exists for path");
      }

      /* insert the value with intermediate key */
      ink_to_val.insert(ink_value_pair(ink_cnt, value));

      /* link key and intermediate key */
      keyset_t ks(ink_cnt);
      std::get<P>(ks.keys) = new Path_T<P>(key);
      
      ink_to_keys.insert(ink_keyset_pair(ink_cnt, ks));

      std::get<P>(key_to_ink).insert(key_ink_pair<P>(key, ink_cnt));

      ink_cnt++;
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
    template<unsigned int P>
    Value_T& at(Path_T<P> key)
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
    template<unsigned int P1, unsigned int P2>
    void link(Path_T<P1> key1, Path_T<P2> key2)
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
        std::get<P1>(ks.keys) = new Path_T<P1>(key1);

        std::get<P1>(key_to_ink).insert(key_ink_pair<P1>(key1, ks.ink));
      }
      /* link key2 with existing key1 */
      else if (it1 != std::get<P1>(key_to_ink).end() and it2 == std::get<P2>(key_to_ink).end())
      {
        keyset_t& ks =  ink_to_keys.at(it1->second);
        std::get<P2>(ks.keys) = new Path_T<P2>(key2);

        std::get<P2>(key_to_ink).insert(key_ink_pair<P2>(key2, ks.ink));
      }
    }

    /**
      @brief  Check whether a value exists for the given key
      @tparam P
              Path index
      @param  key
              Key to check
      */
    template<unsigned int P>
    bool contains(Path_T<P> key)
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
    
  protected:
    /**
      @brief  Helper function to iterate over keyset_t.keys
              Checks if any of keyset_t.keys str non-null and unlinks if so
      @note   std::apply was not used because we have multiple tuples which we
              want to iterate through at the same time
      @note   see stackoverflow question #1198260
      */
    template<unsigned int P = 0>
    inline typename std::enable_if<P != N_Paths, void>::type _erase(keyset_t& ks)
    {
      /*
        Here we use a static assert, instead of P < N_Paths in return type,
        simply because the '<' causes sublime to think it's another template arg
        and messes up the syntax coloring
        */
      static_assert(P < N_Paths);

      if (std::get<P>(ks.keys) != nullptr)
      {
        std::get<P>(key_to_ink).erase(*std::get<P>(ks.keys));
        delete std::get<P>(ks.keys);
        std::get<P>(ks.keys) = nullptr;
      }

      _erase<P + 1>(ks);
    }

    template<unsigned int P = 0>
    inline typename std::enable_if<P == N_Paths, void>::type _erase(keyset_t& ks)
    {}

  public:
    /**
      @brief  Remove a value and all keys which point to it
      @tparam P
              Key column index (which column key belongs to)
      @param  key
              Key to remove value for
      @throw  std::out_of_range
              If key does not exist
      */
    template<unsigned int P>
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
    iterator erase(const iterator& it)
    {
      /* first get the intermediate key */
      intermediate_key_t ink = it.underlying->first;

      /* then remove linked keys */
      _erase(ink_to_keys.at(ink));

      ink_to_keys.erase(ink);

      /* finally, erase the value itself */
      return ink_to_val.erase(it.underlying);
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