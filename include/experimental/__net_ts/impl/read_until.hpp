//
// impl/read_until.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef NET_TS_IMPL_READ_UNTIL_HPP
#define NET_TS_IMPL_READ_UNTIL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <experimental/__net_ts/associated_allocator.hpp>
#include <experimental/__net_ts/associated_executor.hpp>
#include <experimental/__net_ts/buffer.hpp>
#include <experimental/__net_ts/buffers_iterator.hpp>
#include <experimental/__net_ts/detail/bind_handler.hpp>
#include <experimental/__net_ts/detail/handler_alloc_helpers.hpp>
#include <experimental/__net_ts/detail/handler_cont_helpers.hpp>
#include <experimental/__net_ts/detail/handler_invoke_helpers.hpp>
#include <experimental/__net_ts/detail/handler_type_requirements.hpp>
#include <experimental/__net_ts/detail/limits.hpp>
#include <experimental/__net_ts/detail/throw_error.hpp>

#include <experimental/__net_ts/detail/push_options.hpp>

namespace std {
namespace experimental {
namespace net {
inline namespace v1 {

template <typename SyncReadStream, typename DynamicBufferSequence>
inline std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers, char delim)
{
  std::error_code ec;
  std::size_t bytes_transferred = read_until(s,
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers), delim, ec);
  std::experimental::net::detail::throw_error(ec, "read_until");
  return bytes_transferred;
}

template <typename SyncReadStream, typename DynamicBufferSequence>
std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    char delim, std::error_code& ec)
{
  typename decay<DynamicBufferSequence>::type b(
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers));

  std::size_t search_position = 0;
  for (;;)
  {
    // Determine the range of the data to be searched.
    typedef typename DynamicBufferSequence::const_buffers_type buffers_type;
    typedef buffers_iterator<buffers_type> iterator;
    buffers_type data_buffers = b.data();
    iterator begin = iterator::begin(data_buffers);
    iterator start_pos = begin + search_position;
    iterator end = iterator::end(data_buffers);

    // Look for a match.
    iterator iter = std::find(start_pos, end, delim);
    if (iter != end)
    {
      // Found a match. We're done.
      ec = std::error_code();
      return iter - begin + 1;
    }
    else
    {
      // No match. Next search can start with the new data.
      search_position = end - begin;
    }

    // Check if buffer is full.
    if (b.size() == b.max_size())
    {
      ec = error::not_found;
      return 0;
    }

    // Need more data.
    std::size_t bytes_to_read = std::min<std::size_t>(
          std::max<std::size_t>(512, b.capacity() - b.size()),
          std::min<std::size_t>(65536, b.max_size() - b.size()));
    b.commit(s.read_some(b.prepare(bytes_to_read), ec));
    if (ec)
      return 0;
  }
}

template <typename SyncReadStream, typename DynamicBufferSequence>
inline std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const std::string& delim)
{
  std::error_code ec;
  std::size_t bytes_transferred = read_until(s,
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers), delim, ec);
  std::experimental::net::detail::throw_error(ec, "read_until");
  return bytes_transferred;
}

namespace detail
{
  // Algorithm that finds a subsequence of equal values in a sequence. Returns
  // (iterator,true) if a full match was found, in which case the iterator
  // points to the beginning of the match. Returns (iterator,false) if a
  // partial match was found at the end of the first sequence, in which case
  // the iterator points to the beginning of the partial match. Returns
  // (last1,false) if no full or partial match was found.
  template <typename Iterator1, typename Iterator2>
  std::pair<Iterator1, bool> partial_search(
      Iterator1 first1, Iterator1 last1, Iterator2 first2, Iterator2 last2)
  {
    for (Iterator1 iter1 = first1; iter1 != last1; ++iter1)
    {
      Iterator1 test_iter1 = iter1;
      Iterator2 test_iter2 = first2;
      for (;; ++test_iter1, ++test_iter2)
      {
        if (test_iter2 == last2)
          return std::make_pair(iter1, true);
        if (test_iter1 == last1)
        {
          if (test_iter2 != first2)
            return std::make_pair(iter1, false);
          else
            break;
        }
        if (*test_iter1 != *test_iter2)
          break;
      }
    }
    return std::make_pair(last1, false);
  }
} // namespace detail

template <typename SyncReadStream, typename DynamicBufferSequence>
std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const std::string& delim, std::error_code& ec)
{
  typename decay<DynamicBufferSequence>::type b(
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers));

  std::size_t search_position = 0;
  for (;;)
  {
    // Determine the range of the data to be searched.
    typedef typename DynamicBufferSequence::const_buffers_type buffers_type;
    typedef buffers_iterator<buffers_type> iterator;
    buffers_type data_buffers = b.data();
    iterator begin = iterator::begin(data_buffers);
    iterator start_pos = begin + search_position;
    iterator end = iterator::end(data_buffers);

    // Look for a match.
    std::pair<iterator, bool> result = detail::partial_search(
        start_pos, end, delim.begin(), delim.end());
    if (result.first != end)
    {
      if (result.second)
      {
        // Full match. We're done.
        ec = std::error_code();
        return result.first - begin + delim.length();
      }
      else
      {
        // Partial match. Next search needs to start from beginning of match.
        search_position = result.first - begin;
      }
    }
    else
    {
      // No match. Next search can start with the new data.
      search_position = end - begin;
    }

    // Check if buffer is full.
    if (b.size() == b.max_size())
    {
      ec = error::not_found;
      return 0;
    }

    // Need more data.
    std::size_t bytes_to_read = std::min<std::size_t>(
          std::max<std::size_t>(512, b.capacity() - b.size()),
          std::min<std::size_t>(65536, b.max_size() - b.size()));
    b.commit(s.read_some(b.prepare(bytes_to_read), ec));
    if (ec)
      return 0;
  }
}

#if defined(NET_TS_HAS_BOOST_REGEX)

template <typename SyncReadStream, typename DynamicBufferSequence>
inline std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const boost::regex& expr)
{
  std::error_code ec;
  std::size_t bytes_transferred = read_until(s,
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers), expr, ec);
  std::experimental::net::detail::throw_error(ec, "read_until");
  return bytes_transferred;
}

template <typename SyncReadStream, typename DynamicBufferSequence>
std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const boost::regex& expr, std::error_code& ec)
{
  typename decay<DynamicBufferSequence>::type b(
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers));

  std::size_t search_position = 0;
  for (;;)
  {
    // Determine the range of the data to be searched.
    typedef typename DynamicBufferSequence::const_buffers_type buffers_type;
    typedef buffers_iterator<buffers_type> iterator;
    buffers_type data_buffers = b.data();
    iterator begin = iterator::begin(data_buffers);
    iterator start_pos = begin + search_position;
    iterator end = iterator::end(data_buffers);

    // Look for a match.
    boost::match_results<iterator,
      typename std::vector<boost::sub_match<iterator> >::allocator_type>
        match_results;
    if (regex_search(start_pos, end, match_results, expr,
          boost::match_default | boost::match_partial))
    {
      if (match_results[0].matched)
      {
        // Full match. We're done.
        ec = std::error_code();
        return match_results[0].second - begin;
      }
      else
      {
        // Partial match. Next search needs to start from beginning of match.
        search_position = match_results[0].first - begin;
      }
    }
    else
    {
      // No match. Next search can start with the new data.
      search_position = end - begin;
    }

    // Check if buffer is full.
    if (b.size() == b.max_size())
    {
      ec = error::not_found;
      return 0;
    }

    // Need more data.
    std::size_t bytes_to_read = read_size_helper(b, 65536);
    b.commit(s.read_some(b.prepare(bytes_to_read), ec));
    if (ec)
      return 0;
  }
}

#endif // defined(NET_TS_HAS_BOOST_REGEX)

template <typename SyncReadStream,
    typename DynamicBufferSequence, typename MatchCondition>
inline std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    MatchCondition match_condition,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  std::error_code ec;
  std::size_t bytes_transferred = read_until(s,
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers),
      match_condition, ec);
  std::experimental::net::detail::throw_error(ec, "read_until");
  return bytes_transferred;
}

template <typename SyncReadStream,
    typename DynamicBufferSequence, typename MatchCondition>
std::size_t read_until(SyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    MatchCondition match_condition, std::error_code& ec,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  typename decay<DynamicBufferSequence>::type b(
      NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers));

  std::size_t search_position = 0;
  for (;;)
  {
    // Determine the range of the data to be searched.
    typedef typename DynamicBufferSequence::const_buffers_type buffers_type;
    typedef buffers_iterator<buffers_type> iterator;
    buffers_type data_buffers = b.data();
    iterator begin = iterator::begin(data_buffers);
    iterator start_pos = begin + search_position;
    iterator end = iterator::end(data_buffers);

    // Look for a match.
    std::pair<iterator, bool> result = match_condition(start_pos, end);
    if (result.second)
    {
      // Full match. We're done.
      ec = std::error_code();
      return result.first - begin;
    }
    else if (result.first != end)
    {
      // Partial match. Next search needs to start from beginning of match.
      search_position = result.first - begin;
    }
    else
    {
      // No match. Next search can start with the new data.
      search_position = end - begin;
    }

    // Check if buffer is full.
    if (b.size() == b.max_size())
    {
      ec = error::not_found;
      return 0;
    }

    // Need more data.
    std::size_t bytes_to_read = std::min<std::size_t>(
          std::max<std::size_t>(512, b.capacity() - b.size()),
          std::min<std::size_t>(65536, b.max_size() - b.size()));
    b.commit(s.read_some(b.prepare(bytes_to_read), ec));
    if (ec)
      return 0;
  }
}

#if !defined(NET_TS_NO_IOSTREAM)

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, char delim)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), delim);
}

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, char delim,
    std::error_code& ec)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), delim, ec);
}

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const std::string& delim)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), delim);
}

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const std::string& delim,
    std::error_code& ec)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), delim, ec);
}

#if defined(NET_TS_HAS_BOOST_REGEX)

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const boost::regex& expr)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), expr);
}

template <typename SyncReadStream, typename Allocator>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const boost::regex& expr,
    std::error_code& ec)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), expr, ec);
}

#endif // defined(NET_TS_HAS_BOOST_REGEX)

template <typename SyncReadStream, typename Allocator, typename MatchCondition>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, MatchCondition match_condition,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), match_condition);
}

template <typename SyncReadStream, typename Allocator, typename MatchCondition>
inline std::size_t read_until(SyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b,
    MatchCondition match_condition, std::error_code& ec,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  return read_until(s, basic_streambuf_ref<Allocator>(b), match_condition, ec);
}

#endif // !defined(NET_TS_NO_IOSTREAM)

namespace detail
{
  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  class read_until_delim_op
  {
  public:
    template <typename BufferSequence>
    read_until_delim_op(AsyncReadStream& stream,
        NET_TS_MOVE_ARG(BufferSequence) buffers,
        char delim, ReadHandler& handler)
      : stream_(stream),
        buffers_(NET_TS_MOVE_CAST(BufferSequence)(buffers)),
        delim_(delim),
        start_(0),
        search_position_(0),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(handler))
    {
    }

#if defined(NET_TS_HAS_MOVE)
    read_until_delim_op(const read_until_delim_op& other)
      : stream_(other.stream_),
        buffers_(other.buffers_),
        delim_(other.delim_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(other.handler_)
    {
    }

    read_until_delim_op(read_until_delim_op&& other)
      : stream_(other.stream_),
        buffers_(NET_TS_MOVE_CAST(DynamicBufferSequence)(other.buffers_)),
        delim_(other.delim_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(other.handler_))
    {
    }
#endif // defined(NET_TS_HAS_MOVE)

    void operator()(const std::error_code& ec,
        std::size_t bytes_transferred, int start = 0)
    {
      const std::size_t not_found = (std::numeric_limits<std::size_t>::max)();
      std::size_t bytes_to_read;
      switch (start_ = start)
      {
      case 1:
        for (;;)
        {
          {
            // Determine the range of the data to be searched.
            typedef typename DynamicBufferSequence::const_buffers_type
              buffers_type;
            typedef buffers_iterator<buffers_type> iterator;
            buffers_type data_buffers = buffers_.data();
            iterator begin = iterator::begin(data_buffers);
            iterator start_pos = begin + search_position_;
            iterator end = iterator::end(data_buffers);

            // Look for a match.
            iterator iter = std::find(start_pos, end, delim_);
            if (iter != end)
            {
              // Found a match. We're done.
              search_position_ = iter - begin + 1;
              bytes_to_read = 0;
            }

            // No match yet. Check if buffer is full.
            else if (buffers_.size() == buffers_.max_size())
            {
              search_position_ = not_found;
              bytes_to_read = 0;
            }

            // Need to read some more data.
            else
            {
              // Next search can start with the new data.
              search_position_ = end - begin;
              bytes_to_read = std::min<std::size_t>(
                    std::max<std::size_t>(512,
                      buffers_.capacity() - buffers_.size()),
                    std::min<std::size_t>(65536,
                      buffers_.max_size() - buffers_.size()));
            }
          }

          // Check if we're done.
          if (!start && bytes_to_read == 0)
            break;

          // Start a new asynchronous read operation to obtain more data.
          stream_.async_read_some(buffers_.prepare(bytes_to_read),
              NET_TS_MOVE_CAST(read_until_delim_op)(*this));
          return; default:
          buffers_.commit(bytes_transferred);
          if (ec || bytes_transferred == 0)
            break;
        }

        const std::error_code result_ec =
          (search_position_ == not_found)
          ? error::not_found : ec;

        const std::size_t result_n =
          (ec || search_position_ == not_found)
          ? 0 : search_position_;

        handler_(result_ec, result_n);
      }
    }

  //private:
    AsyncReadStream& stream_;
    DynamicBufferSequence buffers_;
    char delim_;
    int start_;
    std::size_t search_position_;
    ReadHandler handler_;
  };

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void* networking_ts_handler_allocate(std::size_t size,
      read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    return networking_ts_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_deallocate(void* pointer, std::size_t size,
      read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline bool networking_ts_handler_is_continuation(
      read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    return this_handler->start_ == 0 ? true
      : networking_ts_handler_cont_helpers::is_continuation(
          this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_invoke(Function& function,
      read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_invoke(const Function& function,
      read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }
} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename ReadHandler, typename Allocator>
struct associated_allocator<
    detail::read_until_delim_op<AsyncReadStream,
      DynamicBufferSequence, ReadHandler>,
    Allocator>
{
  typedef typename associated_allocator<ReadHandler, Allocator>::type type;

  static type get(
      const detail::read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>& h,
      const Allocator& a = Allocator()) NET_TS_NOEXCEPT
  {
    return associated_allocator<ReadHandler, Allocator>::get(h.handler_, a);
  }
};

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename ReadHandler, typename Executor>
struct associated_executor<
    detail::read_until_delim_op<AsyncReadStream,
      DynamicBufferSequence, ReadHandler>,
    Executor>
{
  typedef typename associated_executor<ReadHandler, Executor>::type type;

  static type get(
      const detail::read_until_delim_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>& h,
      const Executor& ex = Executor()) NET_TS_NOEXCEPT
  {
    return associated_executor<ReadHandler, Executor>::get(h.handler_, ex);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream,
    typename DynamicBufferSequence, typename ReadHandler>
NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    char delim, NET_TS_MOVE_ARG(ReadHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a ReadHandler.
  NET_TS_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

  async_completion<ReadHandler,
    void (std::error_code, std::size_t)> init(handler);

  detail::read_until_delim_op<AsyncReadStream,
    typename decay<DynamicBufferSequence>::type,
      NET_TS_HANDLER_TYPE(ReadHandler,
        void (std::error_code, std::size_t))>(
          s, NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers),
            delim, init.completion_handler)(std::error_code(), 0, 1);

  return init.result.get();
}

namespace detail
{
  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  class read_until_delim_string_op
  {
  public:
    template <typename BufferSequence>
    read_until_delim_string_op(AsyncReadStream& stream,
        NET_TS_MOVE_ARG(BufferSequence) buffers,
        const std::string& delim, ReadHandler& handler)
      : stream_(stream),
        buffers_(NET_TS_MOVE_CAST(BufferSequence)(buffers)),
        delim_(delim),
        start_(0),
        search_position_(0),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(handler))
    {
    }

#if defined(NET_TS_HAS_MOVE)
    read_until_delim_string_op(const read_until_delim_string_op& other)
      : stream_(other.stream_),
        buffers_(other.buffers_),
        delim_(other.delim_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(other.handler_)
    {
    }

    read_until_delim_string_op(read_until_delim_string_op&& other)
      : stream_(other.stream_),
        buffers_(NET_TS_MOVE_CAST(DynamicBufferSequence)(other.buffers_)),
        delim_(NET_TS_MOVE_CAST(std::string)(other.delim_)),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(other.handler_))
    {
    }
#endif // defined(NET_TS_HAS_MOVE)

    void operator()(const std::error_code& ec,
        std::size_t bytes_transferred, int start = 0)
    {
      const std::size_t not_found = (std::numeric_limits<std::size_t>::max)();
      std::size_t bytes_to_read;
      switch (start_ = start)
      {
      case 1:
        for (;;)
        {
          {
            // Determine the range of the data to be searched.
            typedef typename DynamicBufferSequence::const_buffers_type
              buffers_type;
            typedef buffers_iterator<buffers_type> iterator;
            buffers_type data_buffers = buffers_.data();
            iterator begin = iterator::begin(data_buffers);
            iterator start_pos = begin + search_position_;
            iterator end = iterator::end(data_buffers);

            // Look for a match.
            std::pair<iterator, bool> result = detail::partial_search(
                start_pos, end, delim_.begin(), delim_.end());
            if (result.first != end && result.second)
            {
              // Full match. We're done.
              search_position_ = result.first - begin + delim_.length();
              bytes_to_read = 0;
            }

            // No match yet. Check if buffer is full.
            else if (buffers_.size() == buffers_.max_size())
            {
              search_position_ = not_found;
              bytes_to_read = 0;
            }

            // Need to read some more data.
            else
            {
              if (result.first != end)
              {
                // Partial match. Next search needs to start from beginning of
                // match.
                search_position_ = result.first - begin;
              }
              else
              {
                // Next search can start with the new data.
                search_position_ = end - begin;
              }

              bytes_to_read = std::min<std::size_t>(
                    std::max<std::size_t>(512,
                      buffers_.capacity() - buffers_.size()),
                    std::min<std::size_t>(65536,
                      buffers_.max_size() - buffers_.size()));
            }
          }

          // Check if we're done.
          if (!start && bytes_to_read == 0)
            break;

          // Start a new asynchronous read operation to obtain more data.
          stream_.async_read_some(buffers_.prepare(bytes_to_read),
              NET_TS_MOVE_CAST(read_until_delim_string_op)(*this));
          return; default:
          buffers_.commit(bytes_transferred);
          if (ec || bytes_transferred == 0)
            break;
        }

        const std::error_code result_ec =
          (search_position_ == not_found)
          ? error::not_found : ec;

        const std::size_t result_n =
          (ec || search_position_ == not_found)
          ? 0 : search_position_;

        handler_(result_ec, result_n);
      }
    }

  //private:
    AsyncReadStream& stream_;
    DynamicBufferSequence buffers_;
    std::string delim_;
    int start_;
    std::size_t search_position_;
    ReadHandler handler_;
  };

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void* networking_ts_handler_allocate(std::size_t size,
      read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    return networking_ts_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_deallocate(void* pointer, std::size_t size,
      read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline bool networking_ts_handler_is_continuation(
      read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    return this_handler->start_ == 0 ? true
      : networking_ts_handler_cont_helpers::is_continuation(
          this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_invoke(Function& function,
      read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename ReadHandler>
  inline void networking_ts_handler_invoke(const Function& function,
      read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }
} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename ReadHandler, typename Allocator>
struct associated_allocator<
    detail::read_until_delim_string_op<AsyncReadStream,
      DynamicBufferSequence, ReadHandler>,
    Allocator>
{
  typedef typename associated_allocator<ReadHandler, Allocator>::type type;

  static type get(
      const detail::read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>& h,
      const Allocator& a = Allocator()) NET_TS_NOEXCEPT
  {
    return associated_allocator<ReadHandler, Allocator>::get(h.handler_, a);
  }
};

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename ReadHandler, typename Executor>
struct associated_executor<
    detail::read_until_delim_string_op<AsyncReadStream,
      DynamicBufferSequence, ReadHandler>,
    Executor>
{
  typedef typename associated_executor<ReadHandler, Executor>::type type;

  static type get(
      const detail::read_until_delim_string_op<AsyncReadStream,
        DynamicBufferSequence, ReadHandler>& h,
      const Executor& ex = Executor()) NET_TS_NOEXCEPT
  {
    return associated_executor<ReadHandler, Executor>::get(h.handler_, ex);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream,
    typename DynamicBufferSequence, typename ReadHandler>
NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const std::string& delim, NET_TS_MOVE_ARG(ReadHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a ReadHandler.
  NET_TS_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

  async_completion<ReadHandler,
    void (std::error_code, std::size_t)> init(handler);

  detail::read_until_delim_string_op<AsyncReadStream,
    typename decay<DynamicBufferSequence>::type,
      NET_TS_HANDLER_TYPE(ReadHandler,
        void (std::error_code, std::size_t))>(
          s, NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers),
            delim, init.completion_handler)(std::error_code(), 0, 1);

  return init.result.get();
}

#if defined(NET_TS_HAS_BOOST_REGEX)

namespace detail
{
  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename RegEx, typename ReadHandler>
  class read_until_expr_op
  {
  public:
    template <typename BufferSequence>
    read_until_expr_op(AsyncReadStream& stream,
        NET_TS_MOVE_ARG(BufferSequence) buffers,
        const boost::regex& expr, ReadHandler& handler)
      : stream_(stream),
        buffers_(NET_TS_MOVE_CAST(BufferSequence)(buffers)),
        expr_(expr),
        start_(0),
        search_position_(0),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(handler))
    {
    }

#if defined(NET_TS_HAS_MOVE)
    read_until_expr_op(const read_until_expr_op& other)
      : stream_(other.stream_),
        buffers_(other.buffers_),
        expr_(other.expr_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(other.handler_)
    {
    }

    read_until_expr_op(read_until_expr_op&& other)
      : stream_(other.stream_),
        buffers_(NET_TS_MOVE_CAST(DynamicBufferSequence)(other.buffers_)),
        expr_(other.expr_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(other.handler_))
    {
    }
#endif // defined(NET_TS_HAS_MOVE)

    void operator()(const std::error_code& ec,
        std::size_t bytes_transferred, int start = 0)
    {
      const std::size_t not_found = (std::numeric_limits<std::size_t>::max)();
      std::size_t bytes_to_read;
      switch (start_ = start)
      {
      case 1:
        for (;;)
        {
          {
            // Determine the range of the data to be searched.
            typedef typename DynamicBufferSequence::const_buffers_type
              buffers_type;
            typedef buffers_iterator<buffers_type> iterator;
            buffers_type data_buffers = buffers_.data();
            iterator begin = iterator::begin(data_buffers);
            iterator start_pos = begin + search_position_;
            iterator end = iterator::end(data_buffers);

            // Look for a match.
            boost::match_results<iterator,
              typename std::vector<boost::sub_match<iterator> >::allocator_type>
                match_results;
            bool match = regex_search(start_pos, end, match_results, expr_,
                boost::match_default | boost::match_partial);
            if (match && match_results[0].matched)
            {
              // Full match. We're done.
              search_position_ = match_results[0].second - begin;
              bytes_to_read = 0;
            }

            // No match yet. Check if buffer is full.
            else if (buffers_.size() == buffers_.max_size())
            {
              search_position_ = not_found;
              bytes_to_read = 0;
            }

            // Need to read some more data.
            else
            {
              if (match)
              {
                // Partial match. Next search needs to start from beginning of
                // match.
                search_position_ = match_results[0].first - begin;
              }
              else
              {
                // Next search can start with the new data.
                search_position_ = end - begin;
              }

              bytes_to_read = std::min<std::size_t>(
                    std::max<std::size_t>(512,
                      buffers_.capacity() - buffers_.size()),
                    std::min<std::size_t>(65536,
                      buffers_.max_size() - buffers_.size()));
            }
          }

          // Check if we're done.
          if (!start && bytes_to_read == 0)
            break;

          // Start a new asynchronous read operation to obtain more data.
          stream_.async_read_some(buffers_.prepare(bytes_to_read),
              NET_TS_MOVE_CAST(read_until_expr_op)(*this));
          return; default:
          buffers_.commit(bytes_transferred);
          if (ec || bytes_transferred == 0)
            break;
        }

        const std::error_code result_ec =
          (search_position_ == not_found)
          ? error::not_found : ec;

        const std::size_t result_n =
          (ec || search_position_ == not_found)
          ? 0 : search_position_;

        handler_(result_ec, result_n);
      }
    }

  //private:
    AsyncReadStream& stream_;
    DynamicBufferSequence buffers_;
    RegEx expr_;
    int start_;
    std::size_t search_position_;
    ReadHandler handler_;
  };

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename RegEx, typename ReadHandler>
  inline void* networking_ts_handler_allocate(std::size_t size,
      read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>* this_handler)
  {
    return networking_ts_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename RegEx, typename ReadHandler>
  inline void networking_ts_handler_deallocate(void* pointer, std::size_t size,
      read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>* this_handler)
  {
    networking_ts_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename RegEx, typename ReadHandler>
  inline bool networking_ts_handler_is_continuation(
      read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>* this_handler)
  {
    return this_handler->start_ == 0 ? true
      : networking_ts_handler_cont_helpers::is_continuation(
          this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename RegEx, typename ReadHandler>
  inline void networking_ts_handler_invoke(Function& function,
      read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename RegEx, typename ReadHandler>
  inline void networking_ts_handler_invoke(const Function& function,
      read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }
} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename RegEx, typename ReadHandler, typename Allocator>
struct associated_allocator<
    detail::read_until_expr_op<AsyncReadStream,
      DynamicBufferSequence, RegEx, ReadHandler>,
    Allocator>
{
  typedef typename associated_allocator<ReadHandler, Allocator>::type type;

  static type get(
      const detail::read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>& h,
      const Allocator& a = Allocator()) NET_TS_NOEXCEPT
  {
    return associated_allocator<ReadHandler, Allocator>::get(h.handler_, a);
  }
};

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename RegEx, typename ReadHandler, typename Executor>
struct associated_executor<
    detail::read_until_expr_op<AsyncReadStream,
      DynamicBufferSequence, RegEx, ReadHandler>,
    Executor>
{
  typedef typename associated_executor<ReadHandler, Executor>::type type;

  static type get(
      const detail::read_until_expr_op<AsyncReadStream,
        DynamicBufferSequence, RegEx, ReadHandler>& h,
      const Executor& ex = Executor()) NET_TS_NOEXCEPT
  {
    return associated_executor<ReadHandler, Executor>::get(h.handler_, ex);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream,
    typename DynamicBufferSequence, typename ReadHandler>
NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    const boost::regex& expr,
    NET_TS_MOVE_ARG(ReadHandler) handler)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a ReadHandler.
  NET_TS_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

  async_completion<ReadHandler,
    void (std::error_code, std::size_t)> init(handler);

  detail::read_until_expr_op<AsyncReadStream,
    typename decay<DynamicBufferSequence>::type,
      boost::regex, NET_TS_HANDLER_TYPE(ReadHandler,
        void (std::error_code, std::size_t))>(
          s, NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers),
            expr, init.completion_handler)(std::error_code(), 0, 1);

  return init.result.get();
}

#endif // defined(NET_TS_HAS_BOOST_REGEX)

namespace detail
{
  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename MatchCondition, typename ReadHandler>
  class read_until_match_op
  {
  public:
    template <typename BufferSequence>
    read_until_match_op(AsyncReadStream& stream,
        NET_TS_MOVE_ARG(BufferSequence) buffers,
        MatchCondition match_condition, ReadHandler& handler)
      : stream_(stream),
        buffers_(NET_TS_MOVE_CAST(BufferSequence)(buffers)),
        match_condition_(match_condition),
        start_(0),
        search_position_(0),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(handler))
    {
    }

#if defined(NET_TS_HAS_MOVE)
    read_until_match_op(const read_until_match_op& other)
      : stream_(other.stream_),
        buffers_(other.buffers_),
        match_condition_(other.match_condition_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(other.handler_)
    {
    }

    read_until_match_op(read_until_match_op&& other)
      : stream_(other.stream_),
        buffers_(NET_TS_MOVE_CAST(DynamicBufferSequence)(other.buffers_)),
        match_condition_(other.match_condition_),
        start_(other.start_),
        search_position_(other.search_position_),
        handler_(NET_TS_MOVE_CAST(ReadHandler)(other.handler_))
    {
    }
#endif // defined(NET_TS_HAS_MOVE)

    void operator()(const std::error_code& ec,
        std::size_t bytes_transferred, int start = 0)
    {
      const std::size_t not_found = (std::numeric_limits<std::size_t>::max)();
      std::size_t bytes_to_read;
      switch (start_ = start)
      {
      case 1:
        for (;;)
        {
          {
            // Determine the range of the data to be searched.
            typedef typename DynamicBufferSequence::const_buffers_type
              buffers_type;
            typedef buffers_iterator<buffers_type> iterator;
            buffers_type data_buffers = buffers_.data();
            iterator begin = iterator::begin(data_buffers);
            iterator start_pos = begin + search_position_;
            iterator end = iterator::end(data_buffers);

            // Look for a match.
            std::pair<iterator, bool> result = match_condition_(start_pos, end);
            if (result.second)
            {
              // Full match. We're done.
              search_position_ = result.first - begin;
              bytes_to_read = 0;
            }

            // No match yet. Check if buffer is full.
            else if (buffers_.size() == buffers_.max_size())
            {
              search_position_ = not_found;
              bytes_to_read = 0;
            }

            // Need to read some more data.
            else
            {
              if (result.first != end)
              {
                // Partial match. Next search needs to start from beginning of
                // match.
                search_position_ = result.first - begin;
              }
              else
              {
                // Next search can start with the new data.
                search_position_ = end - begin;
              }

              bytes_to_read = std::min<std::size_t>(
                    std::max<std::size_t>(512,
                      buffers_.capacity() - buffers_.size()),
                    std::min<std::size_t>(65536,
                      buffers_.max_size() - buffers_.size()));
            }
          }

          // Check if we're done.
          if (!start && bytes_to_read == 0)
            break;

          // Start a new asynchronous read operation to obtain more data.
          stream_.async_read_some(buffers_.prepare(bytes_to_read),
              NET_TS_MOVE_CAST(read_until_match_op)(*this));
          return; default:
          buffers_.commit(bytes_transferred);
          if (ec || bytes_transferred == 0)
            break;
        }

        const std::error_code result_ec =
          (search_position_ == not_found)
          ? error::not_found : ec;

        const std::size_t result_n =
          (ec || search_position_ == not_found)
          ? 0 : search_position_;

        handler_(result_ec, result_n);
      }
    }

  //private:
    AsyncReadStream& stream_;
    DynamicBufferSequence buffers_;
    MatchCondition match_condition_;
    int start_;
    std::size_t search_position_;
    ReadHandler handler_;
  };

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename MatchCondition, typename ReadHandler>
  inline void* networking_ts_handler_allocate(std::size_t size,
      read_until_match_op<AsyncReadStream, DynamicBufferSequence,
        MatchCondition, ReadHandler>* this_handler)
  {
    return networking_ts_handler_alloc_helpers::allocate(
        size, this_handler->handler_);
  }

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename MatchCondition, typename ReadHandler>
  inline void networking_ts_handler_deallocate(void* pointer, std::size_t size,
      read_until_match_op<AsyncReadStream, DynamicBufferSequence,
        MatchCondition, ReadHandler>* this_handler)
  {
    networking_ts_handler_alloc_helpers::deallocate(
        pointer, size, this_handler->handler_);
  }

  template <typename AsyncReadStream, typename DynamicBufferSequence,
      typename MatchCondition, typename ReadHandler>
  inline bool networking_ts_handler_is_continuation(
      read_until_match_op<AsyncReadStream, DynamicBufferSequence,
        MatchCondition, ReadHandler>* this_handler)
  {
    return this_handler->start_ == 0 ? true
      : networking_ts_handler_cont_helpers::is_continuation(
          this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename MatchCondition,
      typename ReadHandler>
  inline void networking_ts_handler_invoke(Function& function,
      read_until_match_op<AsyncReadStream, DynamicBufferSequence,
        MatchCondition, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }

  template <typename Function, typename AsyncReadStream,
      typename DynamicBufferSequence, typename MatchCondition,
      typename ReadHandler>
  inline void networking_ts_handler_invoke(const Function& function,
      read_until_match_op<AsyncReadStream, DynamicBufferSequence,
      MatchCondition, ReadHandler>* this_handler)
  {
    networking_ts_handler_invoke_helpers::invoke(
        function, this_handler->handler_);
  }
} // namespace detail

#if !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename MatchCondition, typename ReadHandler, typename Allocator>
struct associated_allocator<
    detail::read_until_match_op<AsyncReadStream,
      DynamicBufferSequence, MatchCondition, ReadHandler>,
    Allocator>
{
  typedef typename associated_allocator<ReadHandler, Allocator>::type type;

  static type get(
      const detail::read_until_match_op<AsyncReadStream,
        DynamicBufferSequence, MatchCondition, ReadHandler>& h,
      const Allocator& a = Allocator()) NET_TS_NOEXCEPT
  {
    return associated_allocator<ReadHandler, Allocator>::get(h.handler_, a);
  }
};

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename MatchCondition, typename ReadHandler, typename Executor>
struct associated_executor<
    detail::read_until_match_op<AsyncReadStream,
      DynamicBufferSequence, MatchCondition, ReadHandler>,
    Executor>
{
  typedef typename associated_executor<ReadHandler, Executor>::type type;

  static type get(
      const detail::read_until_match_op<AsyncReadStream,
        DynamicBufferSequence, MatchCondition, ReadHandler>& h,
      const Executor& ex = Executor()) NET_TS_NOEXCEPT
  {
    return associated_executor<ReadHandler, Executor>::get(h.handler_, ex);
  }
};

#endif // !defined(GENERATING_DOCUMENTATION)

template <typename AsyncReadStream, typename DynamicBufferSequence,
    typename MatchCondition, typename ReadHandler>
NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    NET_TS_MOVE_ARG(DynamicBufferSequence) buffers,
    MatchCondition match_condition, NET_TS_MOVE_ARG(ReadHandler) handler,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  // If you get an error on the following line it means that your handler does
  // not meet the documented type requirements for a ReadHandler.
  NET_TS_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

  async_completion<ReadHandler,
    void (std::error_code, std::size_t)> init(handler);

  detail::read_until_match_op<AsyncReadStream,
    typename decay<DynamicBufferSequence>::type,
      MatchCondition, NET_TS_HANDLER_TYPE(ReadHandler,
        void (std::error_code, std::size_t))>(
          s, NET_TS_MOVE_CAST(DynamicBufferSequence)(buffers),
            match_condition, init.completion_handler)(
              std::error_code(), 0, 1);

  return init.result.get();
}

#if !defined(NET_TS_NO_IOSTREAM)

template <typename AsyncReadStream, typename Allocator, typename ReadHandler>
inline NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b,
    char delim, NET_TS_MOVE_ARG(ReadHandler) handler)
{
  return async_read_until(s, basic_streambuf_ref<Allocator>(b),
      delim, NET_TS_MOVE_CAST(ReadHandler)(handler));
}

template <typename AsyncReadStream, typename Allocator, typename ReadHandler>
inline NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const std::string& delim,
    NET_TS_MOVE_ARG(ReadHandler) handler)
{
  return async_read_until(s, basic_streambuf_ref<Allocator>(b),
      delim, NET_TS_MOVE_CAST(ReadHandler)(handler));
}

#if defined(NET_TS_HAS_BOOST_REGEX)

template <typename AsyncReadStream, typename Allocator, typename ReadHandler>
inline NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b, const boost::regex& expr,
    NET_TS_MOVE_ARG(ReadHandler) handler)
{
  return async_read_until(s, basic_streambuf_ref<Allocator>(b),
      expr, NET_TS_MOVE_CAST(ReadHandler)(handler));
}

#endif // defined(NET_TS_HAS_BOOST_REGEX)

template <typename AsyncReadStream, typename Allocator,
    typename MatchCondition, typename ReadHandler>
inline NET_TS_INITFN_RESULT_TYPE(ReadHandler,
    void (std::error_code, std::size_t))
async_read_until(AsyncReadStream& s,
    std::experimental::net::basic_streambuf<Allocator>& b,
    MatchCondition match_condition, NET_TS_MOVE_ARG(ReadHandler) handler,
    typename enable_if<is_match_condition<MatchCondition>::value>::type*)
{
  return async_read_until(s, basic_streambuf_ref<Allocator>(b),
      match_condition, NET_TS_MOVE_CAST(ReadHandler)(handler));
}

#endif // !defined(NET_TS_NO_IOSTREAM)

} // inline namespace v1
} // namespace net
} // namespace experimental
} // namespace std

#include <experimental/__net_ts/detail/pop_options.hpp>

#endif // NET_TS_IMPL_READ_UNTIL_HPP
