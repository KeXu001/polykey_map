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

#include <string>
#include <iostream>
#include "polykey_map.hpp"

//g++ -I ../include -o bin/test_polykey_map test_polykey_map.cpp

enum Dim
{
  InternalOrderId,
  ExternalOrderId
};

using InternalOrderId_t = unsigned long;
using ExternalOrderId_t = std::string;

struct Order
{
  std::string ticker;
  int svol;
};

std::ostream& operator<<(std::ostream& stream, const Order& order)
{
  return stream << order.ticker << ":" << order.svol;
}

/* the first argument is the type of the stored values */
using OrderTracker = xu::polykey_map<Order, InternalOrderId_t, ExternalOrderId_t>;

void outputTest(const OrderTracker& otk)
{
  for (auto it = otk.cbegin(); it != otk.cend(); it++)
  {
    std::cout << *it << std::endl;
  }

  if (otk.contains<InternalOrderId>(14))
  {
    std::cout << "contains " << otk.at<InternalOrderId>(14) << std::endl;
  }
}

int main()
{
  OrderTracker otk;

  /* insert */
  otk.insert<InternalOrderId>(13, Order{"AAPL", 100});

  otk.insert<InternalOrderId>(14, Order{"MSFT", -100});

  otk.insert<InternalOrderId>(15, Order{"TSLA", 20});

  otk.insert<InternalOrderId>(19, Order{"FB", 50});

  std::cout << otk.at<InternalOrderId>(13) << std::endl;

  /* link */
  otk.link<InternalOrderId, ExternalOrderId>(13, "1337");

  otk.link<InternalOrderId, ExternalOrderId>(19, "9865");

  std::cout << otk.size<InternalOrderId>() << " != " << otk.size<ExternalOrderId>() << std::endl;

  /* modify */
  otk.at<ExternalOrderId>("1337").svol = 50;

  std::cout << otk.at<InternalOrderId>(13) << std::endl;

  /* erase */
  otk.erase<ExternalOrderId>("1337");

  OrderTracker::const_value_iterator cit = otk.begin();

  /* loop using iterator */
  for (OrderTracker::value_iterator it = otk.begin(); it != otk.end(); it++)
  {
    if (it->ticker == "TSLA")
    {
      it = otk.erase(it);
    }

    cit = it;

    std::cout << "not erased=" << *cit << std::endl;
    std::cout << "not erased, ticker=" << cit->ticker << std::endl;

    std::cout << "internal id=";
    if (cit.has_key<InternalOrderId>())
    {
      std::cout << cit.get_key<InternalOrderId>() << std::endl;
    }
    else
    {
      std::cout << "N/A" << std::endl;
    }

    std::cout << "external id=";
    if (cit.has_key<ExternalOrderId>())
    {
      std::cout << cit.get_key<ExternalOrderId>() << std::endl;
    }
    else
    {
      std::cout << "N/A" << std::endl;
    }
  }

  /* loop using colon syntax */
  for (auto& order : otk)
  {
    std::cout << order << std::endl;
  }

  std::cout << otk.size() << std::endl;


  OrderTracker otk_copy = otk;

  OrderTracker otk_copy2 = std::move(otk);

  std::cout << "otk.size()=" << otk.size() << std::endl;
  std::cout << "otk_copy.size()=" << otk_copy.size() << std::endl;
  std::cout << "otk_copy2.size()=" << otk_copy2.size() << std::endl;

  outputTest(otk_copy);


  std::cout << std::boolalpha << otk_copy.is_linked<InternalOrderId, ExternalOrderId>(19) << std::endl;

  ExternalOrderId_t external_order_id = otk_copy.convert_key<InternalOrderId, ExternalOrderId>(19);

  std::cout << "converted key=" << external_order_id << std::endl;
}