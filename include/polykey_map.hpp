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
              - The container stores values of a single type
              - A value can be accessed by 1 to N keys, where N is the number of
                key 'columns' (at most one key per key column)
              - The number and types of the key columns are limited and known at
                compile time
              - Keys are unique within a key column, and each key points to
                exactly one stored value
              - Removal of a value by key implies removal of all keys which
                point to that value
            This is accomplished by linking key columns to an intermediate key 
            (key -> intermediate key -> value), and by storing keys which point 
            to the same value together.
    @tparam Value_T
            Type of the stored values. Should be copy constructible.
    @tparam Key_Ts
            Types of the key columns. Each type should be copy constructible.
    */
  template<typename Value_T, typename ...Key_Ts>
  class polykey_map
  {
  protected:
    /**
      @brief  Returns a key column's type
      @tparam I
              Key column index
      */
    template<unsigned int I>
    using Key_T = typename std::tuple_element<I, std::tuple<Key_Ts...>>::type;

    /**
      @brief  The total number of key columns
      */
    static const unsigned int N_Keys = sizeof...(Key_Ts);

    /**
      @brief  Type used for the intermediate key
      */
    typedef unsigned long intermediate_key_t;

    /**
      @brief  A collection of linked keys which point to the same value
      */
    struct keyset_t
    {
      /**
        @brief  Keys, held in heap memory
                If non-null, key is valid and is associated with the
                intermediate key and value to which it points.
                If null, key is not linked
        */
      std::tuple<Key_Ts*...> tup;
      intermediate_key_t ink;

      /**
        @brief  First helper function to initialize tup to all null
        */
      template<unsigned int I = 0>
      inline typename std::enable_if<I == N_Keys, void>::type _nullify()
      {}

      /**
        @brief  Second helper function to initialize tup to all null
        */
      template<unsigned int I = 0>
      inline typename std::enable_if<I != N_Keys, void>::type _nullify()
      {
        std::get<I>(tup) = nullptr;

        _nullify<I + 1>();
      }

      /**
        @brief  Default keyset constructor
        @note   Keys in tup are initialized to nullptr using helper functions.
                We use helper functions instead of std::apply to be consistent
                with polykey_map::remove() below, which requires helper
                functions
        */
      keyset_t(intermediate_key_t ink_)
        : ink(ink_)
      {
        _nullify();
      }
    };

    /**
      @brief  Error type when inserting/linking keys
      */
    class key_conflict_error : public std::runtime_error
    {
    public:
      explicit key_conflict_error(const std::string& what_arg)
        : std::runtime_error(what_arg)
      {}
    };

  public:

    /**
      @brief  Insert a new value
      @tparam I
              Key column index (which column key belongs to)
      @param  key
              Key which will point to this value
      @param  value
              Value to insert
      @throw  xu::polykey_map::key_conflict_error
              If key already exists
      */
    template<unsigned int I>
    void insert(Key_T<I> key, Value_T value)
    {
      static_assert(I < N_Keys);

      auto it = std::get<I>(inks).find(key);

      if (it != std::get<I>(inks).end())
      {
        throw key_conflict_error("polykey_map::insert() : key already exists");
      }

      /* insert the value with intermediate key */
      vals.insert(std::pair<intermediate_key_t, Value_T>(ink_ctr, value));

      /* link key and intermediate key */
      keyset_t ks(ink_ctr);
      std::get<I>(ks.tup) = new Key_T<I>(key);
      
      keysets.insert(std::pair<intermediate_key_t, keyset_t>(ink_ctr, ks));
      std::get<I>(inks).insert(std::pair<Key_T<I>, intermediate_key_t>(key, ink_ctr));

      ink_ctr++;
    }

    /**
      @brief  Retrieve a value (by reference)
      @tparam I
              Key column index (which column key belongs to)
      @param  key
              Key to get value for
      @throw  std::out_of_range
              If key does not exist
      */
    template<unsigned int I>
    Value_T& get(Key_T<I> key)
    {
      static_assert(I < N_Keys);

      /* get intermediate key */
      auto it = std::get<I>(inks).find(key);

      if (it == std::get<I>(inks).end())
      {
        throw std::out_of_range("polykey_map::get() : key does not exist");
      }

      intermediate_key_t ink = it->second;

      /* return value for intermediate key */
      return vals.at(ink);
    }

    /**
      @brief  Link two keys so they point to the same value
              Takes two keys as parameters. If only one of the keys is valid
              (points to an object), the other key is set to point to the same
              value. If both keys are valid, or if neither key is valid, then an
              error is thrown
      @tparam I1
              Key column index 1
      @tparam I2
              Key column index 2
      @param  key1
              First key
      @param  key2
              Second key
      @throw  xu::polykey_map::key_conflict_error
              If both keys already exist
      @throw  std::out_of_range
              If neither key exists
      */
    template<unsigned int I1, unsigned int I2>
    void link(Key_T<I1> key1, Key_T<I2> key2)
    {
      static_assert(I1 < N_Keys);
      static_assert(I2 < N_Keys);
      static_assert(I1 != I2);

      /* get intermediate keys */
      auto it1 = std::get<I1>(inks).find(key1);
      auto it2 = std::get<I2>(inks).find(key2);

      if (it1 == std::get<I1>(inks).end() and it2 == std::get<I2>(inks).end())
      {
        throw std::out_of_range("polykey_map::link() : keys do not exist");
      }

      if (it1 != std::get<I1>(inks).end() and it2 != std::get<I2>(inks).end())
      {
        throw key_conflict_error("polykey_map::link() : both keys already exist");
      }

      /* link key1 with existing key2 */
      if (it1 == std::get<I1>(inks).end() and it2 != std::get<I2>(inks).end())
      {
        keyset_t& ks =  keysets.at(it2->second);
        std::get<I1>(ks.tup) = new Key_T<I1>(key1);
        std::get<I1>(inks).insert(std::pair<Key_T<I1>, intermediate_key_t>(key1, ks.ink));
      }
      /* link key2 with existing key1 */
      else if (it1 != std::get<I1>(inks).end() and it2 == std::get<I2>(inks).end())
      {
        keyset_t& ks =  keysets.at(it1->second);
        std::get<I2>(ks.tup) = new Key_T<I2>(key2);
        std::get<I2>(inks).insert(std::pair<Key_T<I2>, intermediate_key_t>(key2, ks.ink));
      }
    }

    /**
      @brief  Check whether a value exists for the given key
      @tparam I
              Key column index (which column key belongs to)
      @param  key
              Key to check for
      */
    template<unsigned int I>
    bool contains(Key_T<I> key)
    {
      static_assert(I < N_Keys);

      auto it = std::get<I>(inks).find(key);

      if (it == std::get<I>(inks).end())
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
      @brief  First helper function for iterating over keyset_t.tup for removal
      */
    template<unsigned int I = 0>
    inline typename std::enable_if<I == N_Keys, void>::type _remove(keyset_t& ks)
    {}

    /**
      @brief  Second helper function to iterating over keyset_t.tup for removal
      @note   std::apply was not used because we have multiple tuples (ks.tup 
              and inks) which we want to iterate through at the same time
      @note   see stackoverflow question #1198260
      */
    template<unsigned int I = 0>
    inline typename std::enable_if<I != N_Keys, void>::type _remove(keyset_t& ks)
    {
      if (std::get<I>(ks.tup) != nullptr)
      {
        std::get<I>(inks).erase(*std::get<I>(ks.tup));
        delete std::get<I>(ks.tup);
        std::get<I>(ks.tup) = nullptr;
      }

      _remove<I + 1>(ks);
    }

  public:
    /**
      @brief  Remove a value and all keys which point to it
      @note   This function has two helper functions which are used to iterate
              over a tuple (keyset_t.tup)
      @tparam I
              Key column index (which column key belongs to)
      @param  key
              Key to remove value for
      @throw  std::out_of_range
              If key does not exist
      */
    template<unsigned int I>
    void remove(Key_T<I> key)
    {
      static_assert(I < N_Keys);

      /* first get the intermediate key */
      auto it = std::get<I>(inks).find(key);

      if (it == std::get<I>(inks).end())
      {
        throw std::out_of_range("polykey_map::get() : key does not exist");
      }

      intermediate_key_t ink = it->second;

      /* then remove linked keys */
      _remove(keysets.at(ink));

      keysets.erase(ink);

      /* finally, erase the value itself */
      vals.erase(ink);
    }

    /**
      @brief  Default constructor
      */
    polykey_map()
      : ink_ctr(0)
    {}

    /**
      @brief  Deallocate heap memory on destruction
      */
    ~polykey_map()
    {
      for (auto& it : keysets)
      {
        _remove(it.second);
      }
    }

  protected:
    /**
      @brief  Container for actual stored values
      */
    std::unordered_map<intermediate_key_t, Value_T> vals;

    /**
      @brief  Keep keys together which point to the same value
      */
    std::unordered_map<intermediate_key_t, keyset_t> keysets;

    /**
      @brief Intermediate key which increments for each inserted value
      */
    intermediate_key_t ink_ctr;

    /**
      @brief Link key columns to intermediate key
      */
    std::tuple<std::unordered_map<Key_Ts, intermediate_key_t>...> inks;

  };
}